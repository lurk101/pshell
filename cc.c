/*
 * mc is capable of compiling a (subset of) C source files
 * There is no preprocessor.
 *
 * The following options are supported:
 *   -s : Print source and generated representation.
 *
 * If -s is omitted, the compiled code is executed immediately
 *
 * All modifications as of Feb 19 2022 are by HPCguy.
 * See AMaCC project repository for baseline code prior to that date.
 *
 * Further modifications by lurk101 for RP Pico
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hardware/timer.h>

#include "fs.h"

extern char* full_path(char* name);

#define SMALL_TBL_WRDS 256
#define BIG_TBL_BYTES (16 * 1024)

char *freep, *p, *lp;  // current position in source code
char *freedata, *data; // data/bss pointer

static int* free_sp;
static int *e, *le, *text; // current position in emitted code
static int* cas;           // case statement patch-up pointer
static int* def;           // default statement patch-up pointer
static int* brks;          // break statement patch-up pointer
static int* cnts;          // continue statement patch-up pointer
static int swtc;           // !0 -> in a switch-stmt context
static int brkc;           // !0 -> in a break-stmt context
static int cntc;           // !0 -> in a continue-stmt context
static int* tsize;         // array (indexed by type) of type sizes
static int tnew;           // next available type
static int tk;             // current token
static union conv {
    int i;
    float f;
} tkv;                    // current token value
static int ty;            // current expression type
                          // bit 0:1 - tensor rank, eg a[4][4][4]
                          // 0=scalar, 1=1d, 2=2d, 3=3d
                          //   1d etype -- bit 0:30)
                          //   2d etype -- bit 0:15,16:30 [32768,65536]
                          //   3d etype -- bit 0:10,11:20,21:30 [1024,1024,2048]
                          // bit 2:9 - type
                          // bit 10:11 - ptr level
static int loc;           // local variable offset
static int line;          // current line number
static int src;           // print source and assembly flag
static int signed_char;   // use `signed char` for `char`
static int* n;            // current position in emitted abstract syntax tree
                          // With an AST, the compiler is not limited to generate
                          // code on the fly with parsing.
                          // This capability allows function parameter code to be
                          // emitted and pushed on the stack in the proper
                          // right-to-left order.
static int ld;            // local variable depth
static int pplev, pplevt; // preprocessor conditional level
static int oline, osize;  // for optimization suggestion

// identifier
#define MAX_IR 256
static struct ident_s {
    int tk; // type-id or keyword
    int hash;
    char* name; // name of this identifier
    int pad;    // pad to multiple of 8 bytes
    /* fields starting with 'h' were designed to save and restore
     * the global class/type/val in order to handle the case if a
     * function declares a local with the same name as a global.
     */
    int class, hclass; // FUNC, GLO (global var), LOC (local var), Syscall
    int type, htype;   // data type such as char and int
    int val, hval;
    int etype, hetype; // extended type info. different meaning for funcs.
} * id,                // currently parsed identifier
    *sym,              // symbol table (simple list of identifiers)
    *oid,              // for array optimization suggestion
    *ir_var[MAX_IR];   // IR information for local vars and parameters
static int ir_count;

// (library) external functions
enum { SYSC_PRINTF = 0, SYSC_MALLOC, SYSC_FREE, SYSC_TIME_US_32 };
static const char* ef_cache[] = {"printf", "malloc", "free", "time_us_32"};
static const int ef_count = sizeof(ef_cache) / sizeof(ef_cache[0]);

static struct member_s {
    struct member_s* next;
    struct ident_s* id;
    int offset;
    int type;
    int etype;
    int pad;
} * *members; // array (indexed by type) of struct member lists

// tokens and classes (operators last and in precedence order)
// ( >= 128 so not to collide with ASCII-valued tokens)
enum {
    Func = 128,
    Syscall,
    Main,
    ClearCache,
    Sqrt,
    Glo,
    Par,
    Loc,
    Keyword,
    Id,
    Load,
    Enter,
    Num,
    NumF,
    Enum,
    Char,
    Int,
    Float,
    Struct,
    Union,
    Sizeof,
    Return,
    Goto,
    Break,
    Continue,
    If,
    DoWhile,
    While,
    For,
    Switch,
    Case,
    Default,
    Else,
    Label,
    Assign, // operator =, keep Assign as highest priority operator
    OrAssign,
    XorAssign,
    AndAssign,
    ShlAssign,
    ShrAssign, // |=, ^=, &=, <<=, >>=
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,
    ModAssign, // +=, -=, *=, /=, %=
    Cond,      // operator: ?
    Lor,
    Lan,
    Or,
    Xor,
    And, // operator: ||, &&, |, ^, &
    Eq,
    Ne,
    Ge,
    Lt,
    Gt,
    Le, // operator: ==, !=, >=, <, >, <=
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod, // operator: <<, >>, +, -, *, /, %
    AddF,
    SubF,
    MulF,
    DivF, // float type operators (hidden)
    EqF,
    NeF,
    GeF,
    LtF,
    GtF,
    LeF,
    CastF,
    Inc,
    Dec,
    Dot,
    Arrow,
    Bracket // operator: ++, --, ., ->, [
};

// opcodes
/* The instruction set is designed for building intermediate representation.
 * Expression 10 + 20 will be translated into the following instructions:
 *    i = 0;
 *    text[i++] = IMM;
 *    text[i++] = 10;
 *    text[i++] = PSH;
 *    text[i++] = IMM;
 *    text[i++] = 20;
 *    text[i++] = ADD;
 *    text[i++] = PSH;
 *    text[i++] = EXIT;
 *    pc = text;
 */
enum {
    LEA, /*  0 */
    /* LEA addressed the problem how to fetch arguments inside sub-function.
     * Let's check out what a calling frame looks like before learning how
     * to fetch arguments (Note that arguments are pushed in its calling
     * order):
     *
     *     sub_function(arg1, arg2, arg3);
     *
     *     |    ....       | high address
     *     +---------------+
     *     | arg: 1        |    new_bp + 4
     *     +---------------+
     *     | arg: 2        |    new_bp + 3
     *     +---------------+
     *     | arg: 3        |    new_bp + 2
     *     +---------------+
     *     |return address |    new_bp + 1
     *     +---------------+
     *     | old BP        | <- new BP
     *     +---------------+
     *     | local var 1   |    new_bp - 1
     *     +---------------+
     *     | local var 2   |    new_bp - 2
     *     +---------------+
     *     |    ....       |  low address
     *
     * If we need to refer to arg1, we need to fetch new_bp + 4, which can not
     * be achieved by restricted ADD instruction. Thus another special
     * instrcution is introduced to do this: LEA <offset>.
     * Together with JSR, ENT, ADJ, LEV, and LEA instruction, we are able to
     * make function calls.
     */

    IMM, /*  1 */
    /* IMM <num> to put immediate <num> into R0 */

    IMMF, /* 2 */
    /* IMM <num> to put immediate <num> into S0 */

    JMP, /*  3 */
    /* JMP <addr> will unconditionally set the value PC register to <addr> */

    JSR, /*  4 */
    /* Jump to address, setting link register for return address */

    BZ,  /*  5 : conditional jump if R0 is zero (jump-if-zero) */
    BNZ, /*  6 : conditional jump if R0 is not zero */

    ENT, /*  7 */
    /* ENT <size> is called when we are about to enter the function call to
     * "make a new calling frame". It will store the current PC value onto
     * the stack, and save some space(<size> bytes) to store the local
     * variables for function.
     */

    ADJ, /*  8 */
    /* ADJ <size> is to adjust the stack, to "remove arguments from frame"
     * The following pseudocode illustrates how ADJ works:
     *     if (op == ADJ) { sp += *pc++; } // add esp, <size>
     */

    LEV, /*  9 */
    /* LEV fetches bookkeeping info to resume previous execution.
     * There is no POP instruction in our design, and the following pseudocode
     * illustrates how LEV works:
     *     if (op == LEV) { sp = bp; bp = (int *) *sp++;
     *                  pc = (int *) *sp++; } // restore call frame and PC
     */

    PSH, /* 10 */
    /* PSH pushes the value in R0 onto the stack */

    PSHF, /* 11 */
    /* PSH pushes the value in R0 onto the stack */

    LC, /* 12 */
    /* LC loads a character into R0 from a given memory
     * address which is stored in R0 before execution.
     */

    LI, /* 13 */
    /* LI loads an integer into R0 from a given memory
     * address which is stored in R0 before execution.
     */

    LF, /* 14 */
    /* LI loads a float into S0 from a given memory
     * address which is stored in R0 before execution.
     */

    SC, /* 15 */
    /* SC stores the character in R0 into the memory whose
     * address is stored on the top of the stack.
     */

    SI, /* 16 */
    /* SI stores the integer in R0 into the memory whose
     * address is stored on the top of the stack.
     */

    SF, /* 17 */
    /* SI stores the float in S0 into the memory whose
     * address is stored on the top of the stack.
     */

    OR,
    /* 18 */ XOR,
    /* 19 */ AND, /* 20 */
    EQ,
    /* 21 */ NE, /* 22 */
    GE,
    /* 23 */ LT,
    /* 24 */ GT,
    /* 25 */ LE, /* 26 */
    SHL,
    /* 27 */ SHR, /* 28 */
    ADD,
    /* 29 */ SUB,
    /* 30 */ MUL,
    /* 31 */ DIV,
    /* 32 */ MOD, /* 33 */
    ADDF,
    /* 34 */ SUBF,
    /* 35 */ MULF,
    /* 36 */ DIVF, /* 37 */
    FTOI,
    /* 38 */ ITOF,
    /* 39 */ EQF,
    /* 40 */ NEF, /* 41 */
    GEF,
    /* 42 */ LTF,
    /* 43 */ GTF,
    /* 44 */ LEF, /* 45 */
    /* arithmetic instructions
     * Each operator has two arguments: the first one is stored on the top
     * of the stack while the second is stored in R0.
     * After the calculation is done, the argument on the stack will be poped
     * off and the result will be stored in R0.
     */

    SQRT, /* 46 float sqrtf(float); */
    SYSC, /* 47 system call */
    CLCA, /* 48 clear cache, used by JIT compilation */

    EXIT,

    INVALID
};

static const char* instr_str[] = {
    "LEA",  "IMM",  "IMMF", "JMP",  "JSR",  "BZ",   "BNZ",    "ENT", "ADJ", "LEV", "PSH",
    "PSHF", "LC",   "LI",   "LF",   "SC",   "SI",   "SF",     "OR",  "XOR", "AND", "EQ",
    "NE",   "GE",   "LT",   "GT",   "LE",   "SHL",  "SHR",    "ADD", "SUB", "MUL", "DIV",
    "MOD",  "ADDF", "SUBF", "MULF", "DIVF", "FTOI", "ITOF",   "EQF", "NEF", "GEF", "LTF",
    "GTF",  "LEF",  "SQRT", "SYSC", "CLCA", "EXIT", "INVALID"};

// types -- 4 scalar types, 1020 aggregate types, 4 tensor ranks, 8 ptr levels
// bits 0-1 = tensor rank, 2-11 = type id, 12-14 = ptr level
// 4 type ids are scalars: 0 = char/void, 1 = int, 2 = float, 3 = reserved
enum { CHAR = 0, INT = 4, FLOAT = 8, ATOM_TYPE = 11, PTR = 0x1000, PTR2 = 0x2000 };

static jmp_buf done_jmp;

static char* append_strtab(char** strtab, char* str) {
    char* s;
    for (s = str; *s && (*s != ' '); ++s)
        ; /* ignore trailing space */
    int nbytes = s - str + 1;
    char* res = *strtab;
    memcpy(res, str, nbytes);
    res[s - str] = 0; // null terminator
    *strtab = res + nbytes;
    return res;
}

static void die(const char* f, ...) {
    va_list ap;
    va_start(ap, f);
    vprintf(f, ap);
    va_end(ap);
    longjmp(done_jmp, 1);
}

static int ef_getaddr(int idx) // get address external function
{
    return idx;
}

static int ef_getidx(char* name) // get cache index of external function
{
    int i, ext_addr = 0x1234;
    for (i = 0; i < ef_count; ++i)
        if (!strcmp(ef_cache[i], name))
            return i;
    return -1;
}

/* parse next token
 * 1. store data into id and then set the id to current lexcial form
 * 2. set tk to appropriate type
 */
static void next() {
    char* pp;
    int t;

    /* using loop to ignore whitespace characters, but characters that
     * cannot be recognized by the lexical analyzer are considered blank
     * characters, such as '@' and '$'.
     */
    while ((tk = *p)) {
        ++p;
        if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || (tk == '_')) {
            pp = p - 1;
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                   (*p >= '0' && *p <= '9') || (*p == '_'))
                tk = tk * 147 + *p++;
            tk = (tk << 6) + (p - pp); // hash plus symbol length
            // hash value is used for fast comparison. Since it is inaccurate,
            // we have to validate the memory content as well.
            for (id = sym; id->tk; ++id) { // find one free slot in table
                if (tk == id->hash &&      // if token is found (hash match), overwrite
                    !memcmp(id->name, pp, p - pp)) {
                    tk = id->tk;
                    return;
                }
            }
            /* At this point, existing symbol name is not found.
             * "id" points to the first unused symbol table entry.
             */
            id->name = pp;
            id->hash = tk;
            tk = id->tk = Id; // token type identifier
            if (memcmp("main", id->name, 4) == 0)
                printf("\n");
            return;
        }
        /* Calculate the constant */
        // first byte is a number, and it is considered a numerical value
        else if (tk >= '0' && tk <= '9') {
            tk = Num;                             // token is char or int
            tkv.i = strtoul((pp = p - 1), &p, 0); // octal, decimal, hex parsing
            if (*p == '.') {
                tkv.f = strtof(pp, &p);
                tk = NumF;
            } // float
            return;
        }
        switch (tk) {
        case '\n':
            if (src) {
                printf("%d: %.*s", line, p - lp, lp);
                lp = p;
            }
            ++line;
        case ' ':
        case '\t':
        case '\v':
        case '\f':
        case '\r':
            break;
        case '/':
            if (*p == '/') { // comment
                while (*p != 0 && *p != '\n')
                    ++p;
            } else if (*p == '*') { // C-style multiline comments
                t = 0;
                for (++p; (*p != 0) && (t == 0); ++p) {
                    pp = p + 1;
                    if (*p == '\n')
                        ++line;
                    else if (*p == '*' && *pp == '/')
                        t = 1;
                }
                ++p;
            } else {
                if (*p == '=') {
                    ++p;
                    tk = DivAssign;
                } else
                    tk = Div;
                return;
            }
            break;
        case '#': // skip include statements, and most preprocessor directives
            if (!strncmp(p, "define", 6)) {
                p += 6;
                next();
                if (tk == Id) {
                    next();
                    if (tk == Num) {
                        id->class = Num;
                        id->type = INT;
                        id->val = tkv.i;
                    }
                }
            } else if ((t = !strncmp(p, "ifdef", 5)) || !strncmp(p, "ifndef", 6)) {
                p += 6;
                next();
                if (tk != Id)
                    die("No identifier");
                ++pplev;
                if ((((id->class != Num) ? 0 : 1) ^ (t ? 1 : 0)) & 1) {
                    t = pplevt;
                    pplevt = pplev - 1;
                    while (*p != 0 && *p != '\n')
                        ++p; // discard until end-of-line
                    do
                        next();
                    while (pplev != pplevt);
                    pplevt = t;
                }
            } else if (!strncmp(p, "if", 2)) {
                // ignore side effects of preprocessor if-statements
                ++pplev;
            } else if (!strncmp(p, "endif", 5)) {
                if (--pplev < 0)
                    die("preprocessor context nesting error");
                if (pplev == pplevt)
                    return;
            }
            while (*p != 0 && *p != '\n')
                ++p; // discard until end-of-line
            break;
        case '\'': // quotes start with character (string)
        case '"':
            pp = data;
            while (*p != 0 && *p != tk) {
                if ((tkv.i = *p++) == '\\') {
                    switch (tkv.i = *p++) {
                    case 'n':
                        tkv.i = '\n';
                        break; // new line
                    case 't':
                        tkv.i = '\t';
                        break; // horizontal tab
                    case 'v':
                        tkv.i = '\v';
                        break; // vertical tab
                    case 'f':
                        tkv.i = '\f';
                        break; // form feed
                    case 'r':
                        tkv.i = '\r';
                        break; // carriage return
                    case '0':
                        tkv.i = '\0';
                        break; // an int with value 0
                    }
                }
                // if it is double quotes (string literal), it is considered as
                // a string, copying characters to data
                if (tk == '"')
                    *data++ = tkv.i;
            }
            ++p;
            if (tk == '"')
                tkv.i = (int)pp;
            else
                tk = Num;
            return;
        case '=':
            if (*p == '=') {
                ++p;
                tk = Eq;
            } else
                tk = Assign;
            return;
        case '*':
            if (*p == '=') {
                ++p;
                tk = MulAssign;
            } else
                tk = Mul;
            return;
        case '+':
            if (*p == '+') {
                ++p;
                tk = Inc;
            } else if (*p == '=') {
                ++p;
                tk = AddAssign;
            } else
                tk = Add;
            return;
        case '-':
            if (*p == '-') {
                ++p;
                tk = Dec;
            } else if (*p == '>') {
                ++p;
                tk = Arrow;
            } else if (*p == '=') {
                ++p;
                tk = SubAssign;
            } else
                tk = Sub;
            return;
        case '[':
            tk = Bracket;
            return;
        case '&':
            if (*p == '&') {
                ++p;
                tk = Lan;
            } else if (*p == '=') {
                ++p;
                tk = AndAssign;
            } else
                tk = And;
            return;
        case '!':
            if (*p == '=') {
                ++p;
                tk = Ne;
            }
            return;
        case '<':
            if (*p == '=') {
                ++p;
                tk = Le;
            } else if (*p == '<') {
                ++p;
                if (*p == '=') {
                    ++p;
                    tk = ShlAssign;
                } else
                    tk = Shl;
            } else
                tk = Lt;
            return;
        case '>':
            if (*p == '=') {
                ++p;
                tk = Ge;
            } else if (*p == '>') {
                ++p;
                if (*p == '=') {
                    ++p;
                    tk = ShrAssign;
                } else
                    tk = Shr;
            } else
                tk = Gt;
            return;
        case '|':
            if (*p == '|') {
                ++p;
                tk = Lor;
            } else if (*p == '=') {
                ++p;
                tk = OrAssign;
            } else
                tk = Or;
            return;
        case '^':
            if (*p == '=') {
                ++p;
                tk = XorAssign;
            } else
                tk = Xor;
            return;
        case '%':
            if (*p == '=') {
                ++p;
                tk = ModAssign;
            } else
                tk = Mod;
            return;
        case '?':
            tk = Cond;
            return;
        case '.':
            tk = Dot;
            return;
        default:
            return;
        }
    }
}

// verify binary operations are legal
static void typecheck(int op, int tl, int tr) {
    int pt = 0, it = 0, st = 0;
    if (tl >= PTR)
        pt += 2; // is pointer?
    if (tr >= PTR)
        pt += 1;

    if (tl < FLOAT)
        it += 2; // is int?
    if (tr < FLOAT)
        it += 1;

    if (tl > ATOM_TYPE && tl < PTR)
        st += 2; // is struct/union?
    if (tr > ATOM_TYPE && tr < PTR)
        st += 1;

    if ((tl ^ tr) & (PTR | PTR2)) { // operation on different pointer levels
        if (op == Add && pt != 3 && (it & ~pt))
            ; // ptr + int or int + ptr ok
        else if (op == Sub && pt == 2 && it == 1)
            ; // ptr - int ok
        else if (op == Assign && pt == 2 && *n == Num && n[1] == 0)
            ; // ok
        else if (op >= Eq && op <= Le && *n == Num && n[1] == 0)
            ; // ok
        else
            die("bad pointer arithmetic");
    } else if (pt == 3 && op != Assign && op != Sub &&
               (op < Eq || op > Le)) // pointers to same type
        die("bad pointer arithmetic");

    if (pt == 0 && op != Assign && (it == 1 || it == 2))
        die("cast operation needed");

    if (pt == 0 && st != 0)
        die("illegal operation with dereferenced struct");
}

static void bitopcheck(int tl, int tr) {
    if (tl >= FLOAT || tr >= FLOAT)
        die("bit operation on non-int types");
}

/* expression parsing
 * lev represents an operator.
 * because each operator `token` is arranged in order of priority,
 * large `lev` indicates a high priority.
 *
 * Operator precedence (lower first):
 * Assign  =
 * Cond   ?
 * Lor    ||
 * Lan    &&
 * Or     |
 * Xor    ^
 * And    &
 * Eq     ==
 * Ne     !=
 * Ge     >=
 * Lt     <
 * Gt     >
 * Le     <=
 * Shl    <<
 * Shr    >>
 * Add    +
 * Sub    -
 * Mul    *
 * Div    /
 * Mod    %
 * Inc    ++
 * Dec    --
 * Bracket [
 */
static void expr(int lev) {
    int t, tc, tt, nf, *b, sz, *c;
    int otk, memsub = 0;
    union conv *c1, *c2;
    struct ident_s* d;
    struct member_s* m;

    switch (tk) {
    case Id:
        d = id;
        next();
        // function call
        if (tk == '(') {
            if (d->class == Func && d->val == 0)
                goto resolve_fnproto;
            if (d->class < Func || d->class > Sqrt) {
                if (d->class != 0)
                    die("bad function call");
                d->type = INT;
                d->etype = 0;
            resolve_fnproto:
                d->class = Syscall;
                int namelen = d->hash & 0x3f;
                char ch = d->name[namelen];
                d->name[namelen] = 0;
                d->val = ef_getidx(d->name);
                if (d->val < 0)
                    die("Unknown external function %s", d->name);
                d->name[namelen] = ch;
            }
            next();
            t = 0;
            b = 0;
            tt = 0;
            nf = 0; // argument count
            while (tk != ')') {
                expr(Assign);
                *--n = (int)b;
                b = n;
                ++t;
                tt = tt * 2;
                if (ty == FLOAT) {
                    ++nf;
                    ++tt;
                }
                if (tk == ',') {
                    next();
                    if (tk == ')')
                        die("unexpected comma in function call");
                } else if (tk != ')')
                    die("missing comma in function call");
            }
            if (t > 22)
                die("maximum of 22 function parameters");
            tt = (tt << 10) + (nf << 5) + t; // func etype not like other etype
            if (d->etype && (d->etype != tt))
                die("argument type mismatch");
            next();
            // function or system call id
            *--n = tt;
            *--n = t;
            *--n = d->val;
            *--n = (int)b;
            *--n = d->class;
            ty = d->type;
        }
        // enumeration, only enums have ->class == Num
        else if (d->class == Num) {
            *--n = d->val;
            *--n = Num;
            ty = INT;
        } else {
            // Variable get offset
            switch (d->class) {
            case Loc:
            case Par:
                *--n = loc - d->val;
                *--n = Loc;
                break;
            case Glo:
                *--n = d->val;
                *--n = Num;
                break;
            default:
                die("undefined variable");
            }
            if ((d->type & 3) && d->class != Par) { // push reference address
                ty = d->type & ~3;
            } else {
                *--n = (ty = d->type & ~3);
                *--n = Load;
            }
        }
        break;
    // directly take an immediate value as the expression value
    // IMM recorded in emit sequence
    case Num:
        *--n = tkv.i;
        *--n = Num;
        next();
        ty = INT;
        break;
    case NumF:
        *--n = tkv.i;
        *--n = NumF;
        next();
        ty = FLOAT;
        break;
    case '"': // string, as a literal in data segment
        *--n = tkv.i;
        *--n = Num;
        next();
        // continuous `"` handles C-style multiline text such as `"abc" "def"`
        while (tk == '"')
            next();
        data = (char*)(((int)data + sizeof(int)) & (~(sizeof(int) - 1)));
        ty = CHAR + PTR;
        break;
    /* SIZEOF_expr -> 'sizeof' '(' 'TYPE' ')'
     * FIXME: not support "sizeof (Id)".
     */
    case Sizeof:
        next();
        if (tk != '(')
            die("open parenthesis expected in sizeof");
        next();
        d = 0;
        if (tk == Id) {
            d = id;
            ty = d->type;
            next();
        } else {
            ty = INT; // Enum
            switch (tk) {
            case Char:
            case Int:
            case Float:
                ty = (tk - Char) << 2;
                next();
                break;
            case Struct:
            case Union:
                next();
                if (tk != Id || id->type <= ATOM_TYPE || id->type >= PTR)
                    die("bad struct/union type");
                ty = id->type;
                next();
                break;
            }
            // multi-level pointers, plus `PTR` for each level
            while (tk == Mul) {
                next();
                ty += PTR;
            }
        }
        if (tk != ')')
            die("close parenthesis expected in sizeof");
        next();
        *--n = (ty & 3) ? (((ty - PTR) >= PTR) ? sizeof(int) : tsize[(ty - PTR) >> 2])
                        : ((ty >= PTR) ? sizeof(int) : tsize[ty >> 2]);
        *--n = Num;
        // just one dimension supported at the moment
        if (d != 0 && (ty & 3))
            n[1] *= (id->etype + 1);
        ty = INT;
        break;
    // Type cast or parenthesis
    case '(':
        next();
        if (tk >= Char && tk <= Union) {
            switch (tk) {
            case Char:
            case Int:
            case Float:
                t = (tk - Char) << 2;
                next();
                break;
            default:
                next();
                if (tk != Id || id->type <= ATOM_TYPE || id->type >= PTR)
                    die("bad struct/union type");
                t = id->type;
                next();
                break;
            }
            // t: pointer
            while (tk == Mul) {
                next();
                t += PTR;
            }
            if (tk != ')')
                die("bad cast");
            next();
            expr(Inc); // cast has precedence as Inc(++)
            if (t != ty && (t == FLOAT || ty == FLOAT)) {
                if (t == FLOAT && ty < FLOAT) { // float : int
                    if (*n == Num) {
                        *n = NumF;
                        c1 = (union conv*)&n[1];
                        c1->f = (float)c1->i;
                    } else {
                        b = n;
                        *--n = ITOF;
                        *--n = (int)b;
                        *--n = CastF;
                    }
                } else if (t < FLOAT && ty == FLOAT) { // int : float
                    if (*n == NumF) {
                        *n = Num;
                        c1 = (union conv*)&n[1];
                        c1->i = (int)c1->f;
                    } else {
                        b = n;
                        *--n = FTOI;
                        *--n = (int)b;
                        *--n = CastF;
                    }
                } else
                    die("explicit cast required");
            }
            ty = t;
        } else {
            expr(Assign);
            while (tk == ',') {
                next();
                b = n;
                expr(Assign);
                *--n = (int)b;
                *--n = '{';
            }
            if (tk != ')')
                die("close parenthesis expected");
            next();
        }
        break;
    case Mul: // "*", dereferencing the pointer operation
        next();
        expr(Inc); // dereference has the same precedence as Inc(++)
        if (ty < PTR)
            die("bad dereference");
        ty -= PTR;
        *--n = ty;
        *--n = Load;
        break;
    case And: // "&", take the address operation
        /* when "token" is a variable, it takes the address first and
         * then LI/LC, so `--e` becomes the address of "a".
         */
        next();
        expr(Inc);
        if (*n != Load)
            die("bad address-of");
        n += 2;
        ty += PTR;
        break;
    case '!': // "!x" is equivalent to "x == 0"
        next();
        expr(Inc);
        if (ty > ATOM_TYPE && ty < PTR)
            die("!(struct/union) is meaningless");
        if (*n == Num)
            n[1] = !n[1];
        else {
            *--n = 0;
            *--n = Num;
            --n;
            *n = (int)(n + 3);
            *--n = Eq;
        }
        ty = INT;
        break;
    case '~': // "~x" is equivalent to "x ^ -1"
        next();
        expr(Inc);
        if (ty > ATOM_TYPE)
            die("~ptr is illegal");
        if (*n == Num)
            n[1] = ~n[1];
        else {
            *--n = -1;
            *--n = Num;
            --n;
            *n = (int)(n + 3);
            *--n = Xor;
        }
        ty = INT;
        break;
    case Add:
        next();
        expr(Inc);
        if (ty > ATOM_TYPE)
            die("unary '+' illegal on ptr");
        break;
    case Sub:
        next();
        expr(Inc);
        if (ty > ATOM_TYPE)
            die("unary '-' illegal on ptr");
        if (*n == Num)
            n[1] = -n[1];
        else if (*n == NumF) {
            n[1] ^= 0x80000000;
        } else if (ty == FLOAT) {
            *--n = 0xbf800000;
            *--n = NumF;
            --n;
            *n = (int)(n + 3);
            *--n = MulF;
        } else {
            *--n = -1;
            *--n = Num;
            --n;
            *n = (int)(n + 3);
            *--n = Mul;
        }
        if (ty != FLOAT)
            ty = INT;
        break;
    case Inc:
    case Dec: // processing ++x and --x. x-- and x++ is handled later
        t = tk;
        next();
        expr(Inc);
        if (ty == FLOAT)
            die("no ++/-- on float");
        if (*n != Load)
            die("bad lvalue in pre-increment");
        *n = t;
        break;
    case 0:
        die("unexpected EOF in expression");
    default:
        die("bad expression");
    }

    // "precedence climbing" or "Top Down Operator Precedence" method
    while (tk >= lev) {
        // tk is ASCII code will not exceed `Num=128`. Its value may be changed
        // during recursion, so back up currently processed expression type
        t = ty;
        b = n;
        switch (tk) {
        case Assign:
            if (t & 3)
                die("Cannot assign to array type lvalue");
            // the left part is processed by the variable part of `tk=ID`
            // and pushes the address
            if (*n != Load)
                die("bad lvalue in assignment");
            // get the value of the right part `expr` as the result of `a=expr`
            n += 2;
            b = n;
            next();
            expr(Assign);
            typecheck(Assign, t, ty);
            *--n = (int)b;
            *--n = (ty << 16) | t;
            *--n = Assign;
            ty = t;
            break;
        case OrAssign: // right associated
        case XorAssign:
        case AndAssign:
        case ShlAssign:
        case ShrAssign:
        case AddAssign:
        case SubAssign:
        case MulAssign:
        case DivAssign:
        case ModAssign:
            if (t & 3)
                die("Cannot assign to array type lvalue");
            if (*n != Load)
                die("bad lvalue in assignment");
            otk = tk;
            n += 2;
            b = n;
            *--n = ';';
            *--n = t;
            *--n = Load;
            sz = (t >= PTR2) ? sizeof(int) : ((t >= PTR) ? tsize[(t - PTR) >> 2] : 1);
            next();
            c = n;
            expr(otk);
            if (*n == Num)
                n[1] *= sz;
            *--n = (int)c;
            if (otk < ShlAssign) {
                *--n = Or + (otk - OrAssign);
            } else {
                *--n = Shl + (otk - ShlAssign);
                // Compound-op bypasses literal const optimizations
                if (ty != FLOAT) {
                    if (otk == DivAssign)
                        ef_getidx("__aeabi_idiv");
                    if (otk == ModAssign)
                        ef_getidx("__aeabi_idivmod");
                }
            }
            if (t == FLOAT && (otk >= AddAssign && otk <= DivAssign))
                *n += 5;
            typecheck(*n, t, ty);
            *--n = (int)b;
            *--n = (ty << 16) | t;
            *--n = Assign;
            ty = t;
            break;
        case Cond: // `x?a:b` is similar to if except that it relies on else
            next();
            expr(Assign);
            tc = ty;
            if (tk != ':')
                die("conditional missing colon");
            next();
            c = n;
            expr(Cond);
            --n;
            if (tc != ty)
                die("both results need same type");
            *n = (int)(n + 1);
            *--n = (int)c;
            *--n = (int)b;
            *--n = Cond;
            break;
        case Lor: // short circuit, the logical or
            next();
            expr(Lan);
            if (*n == Num && *b == Num)
                n[1] = b[1] || n[1];
            else {
                *--n = (int)b;
                *--n = Lor;
            }
            ty = INT;
            break;
        case Lan: // short circuit, logic and
            next();
            expr(Or);
            if (*n == Num && *b == Num)
                n[1] = b[1] && n[1];
            else {
                *--n = (int)b;
                *--n = Lan;
            }
            ty = INT;
            break;
        case Or: // push the current value, calculate the right value
            next();
            expr(Xor);
            bitopcheck(t, ty);
            if (*n == Num && *b == Num)
                n[1] = b[1] | n[1];
            else {
                *--n = (int)b;
                *--n = Or;
            }
            ty = INT;
            break;
        case Xor:
            next();
            expr(And);
            bitopcheck(t, ty);
            if (*n == Num && *b == Num)
                n[1] = b[1] ^ n[1];
            else {
                *--n = (int)b;
                *--n = Xor;
            }
            ty = INT;
            break;
        case And:
            next();
            expr(Eq);
            bitopcheck(t, ty);
            if (*n == Num && *b == Num)
                n[1] = b[1] & n[1];
            else {
                *--n = (int)b;
                *--n = And;
            }
            ty = INT;
            break;
        case Eq:
            next();
            expr(Ge);
            typecheck(Eq, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->i = (c2->f == c1->f);
                    *n = Num;
                } else {
                    *--n = (int)b;
                    *--n = EqF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] = b[1] == n[1];
                else {
                    *--n = (int)b;
                    *--n = Eq;
                }
            }
            ty = INT;
            break;
        case Ne:
            next();
            expr(Ge);
            typecheck(Ne, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->i = (c2->f != c1->f);
                    *n = Num;
                } else {
                    *--n = (int)b;
                    *--n = NeF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] = b[1] != n[1];
                else {
                    *--n = (int)b;
                    *--n = Ne;
                }
            }
            ty = INT;
            break;
        case Ge:
            next();
            expr(Shl);
            typecheck(Ge, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->i = (c2->f >= c1->f);
                    *n = Num;
                } else {
                    *--n = (int)b;
                    *--n = GeF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] = b[1] >= n[1];
                else {
                    *--n = (int)b;
                    *--n = Ge;
                }
            }
            ty = INT;
            break;
        case Lt:
            next();
            expr(Shl);
            typecheck(Lt, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->i = (c2->f < c1->f);
                    *n = Num;
                } else {
                    *--n = (int)b;
                    *--n = LtF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] = b[1] < n[1];
                else {
                    *--n = (int)b;
                    *--n = Lt;
                }
            }
            ty = INT;
            break;
        case Gt:
            next();
            expr(Shl);
            typecheck(Gt, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->i = (c2->f > c1->f);
                    *n = Num;
                } else {
                    *--n = (int)b;
                    *--n = GtF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] = b[1] > n[1];
                else {
                    *--n = (int)b;
                    *--n = Gt;
                }
            }
            ty = INT;
            break;
        case Le:
            next();
            expr(Shl);
            typecheck(Le, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->i = (c2->f <= c1->f);
                    *n = Num;
                } else {
                    *--n = (int)b;
                    *--n = LeF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] = b[1] <= n[1];
                else {
                    *--n = (int)b;
                    *--n = Le;
                }
            }
            ty = INT;
            break;
        case Shl:
            next();
            expr(Add);
            bitopcheck(t, ty);
            if (*n == Num && *b == Num) {
                if (n[1] < 0)
                    n[1] = b[1] >> -n[1];
                else
                    n[1] = b[1] << n[1];
            } else {
                *--n = (int)b;
                *--n = Shl;
            }
            ty = INT;
            break;
        case Shr:
            next();
            expr(Add);
            bitopcheck(t, ty);
            if (*n == Num && *b == Num) {
                if (n[1] < 0)
                    n[1] = b[1] << -n[1];
                else
                    n[1] = b[1] >> n[1];
            } else {
                *--n = (int)b;
                *--n = Shr;
            }
            ty = INT;
            break;
        case Add:
            next();
            expr(Mul);
            typecheck(Add, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->f = c1->f + c2->f;
                } else {
                    *--n = (int)b;
                    *--n = AddF;
                }
            } else { // both terms are either int or "int *"
                tc = ((t | ty) & (PTR | PTR2)) ? (t >= PTR) : (t >= ty);
                c = n;
                if (tc)
                    ty = t;
                sz = (ty >= PTR2) ? sizeof(int) : ((ty >= PTR) ? tsize[(ty - PTR) >> 2] : 1);
                if (*n == Num && tc) {
                    n[1] *= sz;
                    sz = 1;
                } else if (*b == Num && !tc) {
                    b[1] *= sz;
                    sz = 1;
                }
                if (*n == Num && *b == Num)
                    n[1] += b[1];
                else if (sz != 1) {
                    *--n = sz;
                    *--n = Num;
                    *--n = (int)(tc ? c : b);
                    *--n = Mul;
                    *--n = (int)(tc ? b : c);
                    *--n = Add;
                } else {
                    *--n = (int)b;
                    *--n = Add;
                }
            }
            break;
        case Sub:
            next();
            expr(Mul);
            typecheck(Sub, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->f = c2->f - c1->f;
                } else {
                    *--n = (int)b;
                    *--n = SubF;
                }
            } else {            // 4 cases: ptr-ptr, ptr-int, int-ptr (err), int-int
                if (t >= PTR) { // left arg is ptr
                    sz = (t >= PTR2) ? sizeof(int) : tsize[(t - PTR) >> 2];
                    if (ty >= PTR) { // ptr - ptr
                        if (*n == Num && *b == Num)
                            n[1] = (b[1] - n[1]) / sz;
                        else {
                            *--n = (int)b;
                            *--n = Sub;
                            if (sz > 1) {
                                if ((sz & (sz - 1)) == 0) { // 2^n
                                    *--n = __builtin_popcount(sz - 1);
                                    *--n = Num;
                                    --n;
                                    *n = (int)(n + 3);
                                    *--n = Shr;
                                } else {
                                    *--n = sz;
                                    *--n = Num;
                                    --n;
                                    *n = (int)(n + 3);
                                    *--n = Div;
                                    ef_getidx("__aeabi_idiv");
                                }
                            }
                        }
                        ty = INT;
                    } else { // ptr - int
                        if (*n == Num) {
                            n[1] *= sz;
                            if (*b == Num)
                                n[1] = b[1] - n[1];
                            else {
                                *--n = (int)b;
                                *--n = Sub;
                            }
                        } else {
                            if (sz > 1) {
                                if ((sz & (sz - 1)) == 0) { // 2^n
                                    *--n = __builtin_popcount(sz - 1);
                                    *--n = Num;
                                    --n;
                                    *n = (int)(n + 3);
                                    *--n = Shl;
                                } else {
                                    *--n = sz;
                                    *--n = Num;
                                    --n;
                                    *n = (int)(n + 3);
                                    *--n = Mul;
                                }
                            }
                            *--n = (int)b;
                            *--n = Sub;
                        }
                        ty = t;
                    }
                } else { // int - int
                    if (*n == Num && *b == Num)
                        n[1] = b[1] - n[1];
                    else {
                        *--n = (int)b;
                        *--n = Sub;
                    }
                    ty = INT;
                }
            }
            break;
        case Mul:
            next();
            expr(Inc);
            typecheck(Mul, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->f = c1->f * c2->f;
                } else {
                    *--n = (int)b;
                    *--n = MulF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] *= b[1];
                else {
                    *--n = (int)b;
                    if (n[1] == Num && n[2] > 0 && (n[2] & (n[2] - 1)) == 0) {
                        n[2] = __builtin_popcount(n[2] - 1);
                        *--n = Shl; // 2^n
                    } else
                        *--n = Mul;
                }
                ty = INT;
            }
            break;
        case Inc:
        case Dec:
            if (ty & 3)
                die("can't inc/dec an array variable");
            if (ty == FLOAT)
                die("no ++/-- on float");
            sz = (ty >= PTR2) ? sizeof(int) : ((ty >= PTR) ? tsize[(ty - PTR) >> 2] : 1);
            if (*n != Load)
                die("bad lvalue in post-increment");
            *n = tk;
            *--n = sz;
            *--n = Num;
            *--n = (int)b;
            *--n = (tk == Inc) ? Sub : Add;
            next();
            break;
        case Div:
            next();
            expr(Inc);
            typecheck(Div, t, ty);
            if (ty == FLOAT) {
                if (*n == NumF && *b == NumF) {
                    c1 = (union conv*)&n[1];
                    c2 = (union conv*)&b[1];
                    c1->f = c2->f / c1->f;
                } else {
                    *--n = (int)b;
                    *--n = DivF;
                }
            } else {
                if (*n == Num && *b == Num)
                    n[1] = b[1] / n[1];
                else {
                    *--n = (int)b;
                    if (n[1] == Num && n[2] > 0 && (n[2] & (n[2] - 1)) == 0) {
                        n[2] = __builtin_popcount(n[2] - 1);
                        *--n = Shr; // 2^n
                    } else {
                        *--n = Div;
                        ef_getidx("__aeabi_idiv");
                    }
                }
                ty = INT;
            }
            break;
        case Mod:
            next();
            expr(Inc);
            typecheck(Mod, t, ty);
            if (ty == FLOAT)
                die("use fmodf() for float modulo");
            if (*n == Num && *b == Num)
                n[1] = b[1] % n[1];
            else {
                *--n = (int)b;
                if (n[1] == Num && n[2] > 0 && (n[2] & (n[2] - 1)) == 0) {
                    --n[2];
                    *--n = And; // 2^n
                } else {
                    *--n = Mod;
                    ef_getidx("__aeabi_idivmod");
                }
            }
            ty = INT;
            break;
        case Dot:
            t += PTR;
            if (n[0] == Load && n[1] > ATOM_TYPE && n[1] < PTR)
                n += 2; // struct
        case Arrow:
            if (t <= PTR + ATOM_TYPE || t >= PTR2)
                die("structure expected");
            next();
            if (tk != Id)
                die("structure member expected");
            m = members[(t - PTR) >> 2];
            while (m && m->id != id)
                m = m->next;
            if (!m)
                die("structure member not found");
            if (m->offset) {
                *--n = m->offset;
                *--n = Num;
                --n;
                *n = (int)(n + 3);
                *--n = Add;
            }
            ty = m->type;
            next();
            if (!(ty & 3)) {
                *--n = (ty >= PTR) ? INT : ty;
                *--n = Load;
                break;
            }
            memsub = 1;
            int dim = ty & 3, ee = m->etype;
            b = n;
            t = ty & ~3;
        case Bracket:
            if (t < PTR)
                die("pointer type expected");
            if (memsub == 0) {
                dim = id->type & 3, ee = id->etype;
            }
            int sum = 0, ii = dim - 1, *f = 0;
            int doload = 1;
            memsub = 0;
            sz = ((t = t - PTR) >= PTR) ? sizeof(int) : tsize[t >> 2];
            do {
                if (dim && tk != Bracket) { // ptr midway for partial subscripting
                    t += PTR * (ii + 1);
                    doload = 0;
                    break;
                }
                next();
                expr(Assign);
                if (ty >= FLOAT)
                    die("non-int array index");
                if (tk != ']')
                    die("close bracket expected");
                c = n;
                next();
                if (dim) {
                    int factor = ((ii == 2) ? (((ee >> 11) & 0x3ff) + 1) : 1);
                    factor *=
                        ((dim == 3 && ii >= 1) ? ((ee & 0x7ff) + 1)
                                               : ((dim == 2 && ii == 1) ? ((ee & 0xffff) + 1) : 1));
                    if (*n == Num) {
                        // elision with struct offset for efficiency
                        if (*b == Add && b[2] == Num)
                            b[3] += factor * n[1] * sz;
                        else
                            sum += factor * n[1];
                        n += 2; // delete the subscript constant
                    } else {
                        // generate code to add a term
                        if (factor > 1) {
                            *--n = factor;
                            *--n = Num;
                            *--n = (int)c;
                            *--n = Mul;
                        }
                        if (f) {
                            *--n = (int)f;
                            *--n = Add;
                        }
                        f = n;
                    }
                }
            } while (--ii >= 0);
            if (dim) {
                if (sum > 0) {
                    if (f) {
                        *--n = sum;
                        *--n = Num;
                        *--n = (int)f;
                        *--n = Add;
                    } else {
                        sum *= sz;
                        sz = 1;
                        *--n = sum;
                        *--n = Num;
                    }
                } else if (!f)
                    goto add_simple;
            }
            if (sz > 1) {
                if (*n == Num)
                    n[1] *= sz;
                else {
                    *--n = sz;
                    *--n = Num;
                    --n;
                    *n = (int)(n + 3);
                    *--n = Mul;
                }
            }
            if (*n == Num && *b == Num)
                n[1] += b[1];
            else {
                *--n = (int)b;
                *--n = Add;
            }
        add_simple:
            if (doload) {
                *--n = ((ty = t) >= PTR) ? INT : ty;
                *--n = Load;
            }
            break;
        default:
            die("%d: compiler error tk=%d\n", line, tk);
        }
    }
}

static void init_array(struct ident_s* tn, int extent[], int dim) {
    int i, cursor, match, coff = 0, off, *p;
    int inc[3];

    inc[0] = extent[dim - 1];
    for (i = 1; i < dim; ++i)
        inc[i] = inc[i - 1] * extent[dim - (i + 1)];

    // Global is preferred to local.
    // Either suggest global or automatically move to global scope.
    if (tn->class != Glo)
        die("only global array initialization supported");

    switch (tn->type & ~3) {
    case (CHAR | PTR2):
        die("Use extra dim of MAXCHAR length instead");
    case (CHAR | PTR):
        match = '"';
        coff = 1;
        break; // strings
    case (INT | PTR):
        match = Num;
        break;
    case (FLOAT | PTR):
        match = NumF;
        break;
    default:
        die("array-init must be literal ints, floats, or strings");
    }

    p = (int*)tn->val;
    i = 0;
    cursor = (dim - coff);
    do {
        if (tk == Sub) {
            next();
            if (tk == NumF)
                tkv.i |= 0x10000000;
            else if (tk == Num)
                tkv.i = 0 - tkv.i;
            else
                die("non-literal initializer");
        }

        if (tk == '{') {
            next();
            if (cursor)
                --cursor;
            else
                die("overly nested initializer");
            continue;
        } else if (tk == '}') {
            next();
            // skip remainder elements on this level (or set 0 if cmdline opt)
            if ((off = i % inc[cursor + coff]))
                i += (inc[cursor + coff] - off);
            if (++cursor == dim - coff)
                break;
        } else if (tk == '"') {
            if (match == '"') {
                off = strlen((char*)tkv.i) + 1;
                if (off > inc[0]) {
                    off = inc[0];
                    printf("%d: string '%s' truncated to %d chars\n", line, (char*)tkv.i, off);
                }
                memcpy((char*)p + i, (char*)tkv.i, off);
                i += inc[0];
                next();
            } else
                die("can't assign string to scalar");
        } else if (tk == match) {
            p[i++] = tkv.i;
            next();
        } else if (tk == Num) {
            if (match == '"') {
                *((char*)p + i) = tkv.i;
                i += inc[0];
            } else {
                tkv.f = (float)tkv.i;
                p[i++] = tkv.i;
            }
            next();
        } else if (tk == NumF) {
            if (match == Num) {
                p[i++] = (int)tkv.f;
                next();
            } else
                die("illegal char/string initializer");
        } else
            die("non-literal initializer");
        if (tk == ',')
            next();
    } while (1);
}

// AST parsing for IR generatiion
// With a modular code generator, new targets can be easily supported such as
// native Arm machine code.
static void gen(int* n) {
    int i = *n, j, k, l, isPrtf;
    int *a = NULL, *b, *c, *d, *t;
    struct ident_s* label;

    switch (i) {
    case Num:
        *++e = IMM;
        *++e = n[1];
        break; // int value
    case NumF:
        *++e = IMMF;
        *++e = n[1];
        break; // float value
    case Load:
        gen(n + 2);                         // load the value
        if (n[1] > ATOM_TYPE && n[1] < PTR) // unreachable?
            die("struct copies not yet supported");
        *++e = (n[1] >= PTR) ? LI : LC + (n[1] >> 2);
        break;
    case Loc:
        *++e = LEA;
        *++e = n[1];
        break; // get address of variable
    case '{':
        gen((int*)n[1]);
        gen(n + 2);
        break;   // parse AST expr or stmt
    case Assign: // assign the value to variables
        gen((int*)n[2]);
        *++e = PSH;
        gen(n + 3);
        l = n[1] & 0xffff;
        // Add SC/SI instruction to save value in register to variable address
        // held on stack.
        if (l > ATOM_TYPE && l < PTR)
            die("struct assign not yet supported");
        if ((n[1] >> 16) == FLOAT && l == INT)
            *++e = FTOI;
        else if ((n[1] >> 16) == INT && l == FLOAT)
            *++e = ITOF;
        *++e = (l >= PTR) ? SI : SC + (l >> 2);
        break;
    case Inc: // increment or decrement variables
    case Dec:
        gen(n + 2);
        *++e = PSH;
        *++e = (n[1] == CHAR) ? LC : LI;
        *++e = PSH;
        *++e = IMM;
        *++e = (n[1] >= PTR2) ? sizeof(int) : ((n[1] >= PTR) ? tsize[(n[1] - PTR) >> 2] : 1);
        *++e = (i == Inc) ? ADD : SUB;
        *++e = (n[1] == CHAR) ? SC : SI;
        break;
    case Cond:           // if else condition case
        gen((int*)n[1]); // condition
        // Add jump-if-zero instruction "BZ" to jump to false branch.
        // Point "b" to the jump address field to be patched later.
        *++e = BZ;
        b = ++e;
        gen((int*)n[2]); // expression
        // Patch the jump address field pointed to by "b" to hold the address
        // of false branch. "+ 3" counts the "JMP" instruction added below.
        //
        // Add "JMP" instruction after true branch to jump over false branch.
        // Point "b" to the jump address field to be patched later.
        if (n[3]) {
            *b = (int)(e + 3);
            *++e = JMP;
            b = ++e;
            gen((int*)n[3]);
        } // else statment
        // Patch the jump address field pointed to by "d" to hold the address
        // past the false branch.
        *b = (int)(e + 1);
        break;
    // operators
    /* If current token is logical OR operator:
     * Add jump-if-nonzero instruction "BNZ" to implement short circuit.
     * Point "b" to the jump address field to be patched later.
     * Parse RHS expression.
     * Patch the jump address field pointed to by "b" to hold the address past
     * the RHS expression.
     */
    case Lor:
        gen((int*)n[1]);
        *++e = BNZ;
        b = ++e;
        gen(n + 2);
        *b = (int)(e + 1);
        break;
    case Lan:
        gen((int*)n[1]);
        *++e = BZ;
        b = ++e;
        gen(n + 2);
        *b = (int)(e + 1);
        break;
    /* If current token is bitwise OR operator:
     * Add "PSH" instruction to push LHS value in register to stack.
     * Parse RHS expression.
     * Add "OR" instruction to compute the result.
     */
    case Or:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = OR;
        break;
    case Xor:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = XOR;
        break;
    case And:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = AND;
        break;
    case Eq:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = EQ;
        break;
    case Ne:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = NE;
        break;
    case Ge:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = GE;
        break;
    case Lt:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = LT;
        break;
    case Gt:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = GT;
        break;
    case Le:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = LE;
        break;
    case Shl:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = SHL;
        break;
    case Shr:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = SHR;
        break;
    case Add:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = ADD;
        break;
    case Sub:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = SUB;
        break;
    case Mul:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = MUL;
        break;
    case Div:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = DIV;
        break;
    case Mod:
        gen((int*)n[1]);
        *++e = PSH;
        gen(n + 2);
        *++e = MOD;
        break;
    case AddF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = ADDF;
        break;
    case SubF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = SUBF;
        break;
    case MulF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = MULF;
        break;
    case DivF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = DIVF;
        break;
    case EqF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = EQF;
        break;
    case NeF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = NEF;
        break;
    case GeF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = GEF;
        break;
    case LtF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = LTF;
        break;
    case GtF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = GTF;
        break;
    case LeF:
        gen((int*)n[1]);
        *++e = PSHF;
        gen(n + 2);
        *++e = LEF;
        break;
    case CastF:
        gen((int*)n[1]);
        *++e = n[2];
        break;
    case Func:
    case Syscall:
    case ClearCache:
        b = (int*)n[1];
        k = b ? n[3] : 0;
        if (k) {
            l = (i != ClearCache) ? (n[4] >> 10) : 0;
            a = (int*)malloc(sizeof(int) * k);
            for (j = 0; *b; b = (int*)*b)
                a[j++] = (int)b;
            a[j] = (int)b;
            int sj = j;
            while (j >= 0) { // push arguments
                gen(b + 1);
                *++e = (l & (1 << j)) ? PSHF : PSH;
                --j;
                b = (int*)a[j];
            }
            free(a);
            if (i == Syscall) {
                *++e = IMM;
                *++e = sj + 1;
            }
        }
        if (i == Syscall)
            *++e = SYSC;
        if (i == Func)
            *++e = JSR;
        *++e = n[2];
        if (n[3]) {
            *++e = ADJ;
            *++e = (i == Syscall) ? n[4] : n[3];
        }
        break;
    case Sqrt:
        b = (int*)n[1];
        gen(b + 1);
        *++e = n[2];
        break;
    case While:
    case DoWhile:
        if (i == While) {
            *++e = JMP;
            a = ++e;
        }
        d = (e + 1);
        b = brks;
        brks = 0;
        c = cnts;
        cnts = 0;
        gen((int*)n[1]); // loop body
        if (i == While)
            *a = (int)(e + 1);
        while (cnts) {
            t = (int*)*cnts;
            *cnts = (int)(e + 1);
            cnts = t;
        }
        cnts = c;
        gen((int*)n[2]); // condition
        *++e = BNZ;
        *++e = (int)d;
        while (brks) {
            t = (int*)*brks;
            *brks = (int)(e + 1);
            brks = t;
        }
        brks = b;
        break;
    case For:
        gen((int*)n[4]); // init
        *++e = JMP;
        a = ++e;
        d = (e + 1);
        b = brks;
        brks = 0;
        c = cnts;
        cnts = 0;
        gen((int*)n[3]); // loop body
        while (cnts) {
            t = (int*)*cnts;
            *cnts = (int)(e + 1);
            cnts = t;
        }
        cnts = c;
        gen((int*)n[2]); // increment
        *a = (int)(e + 1);
        gen((int*)n[1]); // condition
        *++e = BNZ;
        *++e = (int)d;
        while (brks) {
            t = (int*)*brks;
            *brks = (int)(e + 1);
            brks = t;
        }
        brks = b;
        break;
    case Switch:
        gen((int*)n[1]); // condition
        a = cas;
        *++e = JMP;
        cas = ++e;
        b = brks;
        d = def;
        brks = def = 0;
        gen((int*)n[2]); // case statment
        // deal with no default inside switch case
        *cas = def ? (int)def : (int)(e + 1);
        cas = a;
        while (brks) {
            t = (int*)*brks;
            *brks = (int)(e + 1);
            brks = t;
        }
        brks = b;
        def = d;
        break;
    case Case:
        *++e = JMP;
        ++e;
        a = 0;
        *e = (int)(e + 7);
        *++e = PSH;
        i = *cas;
        *cas = (int)e;
        gen((int*)n[1]); // condition
        if (*(e - 1) != IMM)
            die("case label not a numeric literal");
        *++e = SUB;
        *++e = BNZ;
        cas = ++e;
        *e = i + e[-3];
        if (*(int*)n[2] == Switch)
            a = cas;
        gen((int*)n[2]); // expression
        if (a != 0)
            cas = a;
        break;
    case Break:
        *++e = JMP;
        *++e = (int)brks;
        brks = e;
        break;
    case Continue:
        *++e = JMP;
        *++e = (int)cnts;
        cnts = e;
        break;
    case Goto:
        label = (struct ident_s*)n[1];
        *++e = JMP;
        *++e = label->val;
        if (label->class == 0)
            label->val = (int)e; // Define label address later
        break;
    case Default:
        def = e + 1;
        gen((int*)n[1]);
        break;
    case Return:
        if (n[1])
            gen((int*)n[1]);
        *++e = LEV;
        break;
    case Enter:
        *++e = ENT;
        *++e = n[1];
        gen(n + 2);
        if (*e != LEV)
            *++e = LEV;
        break;
    case Label: // target of goto
        label = (struct ident_s*)n[1];
        if (label->class != 0)
            die("duplicate label definition");
        d = e + 1;
        b = (int*)label->val;
        while (b != 0) {
            t = (int*)*b;
            *b = (int)d;
            b = t;
        }
        label->val = (int)d;
        label->class = Label;
        break;
    default:
        if (i != ';')
            die("%d: compiler error gen=%08x\n", line, i);
    }
}

static void check_label(int** tt) {
    if (tk != Id)
        return;
    char* ss = p;
    while (*ss == ' ' || *ss == '\t')
        ++ss;
    if (*ss == ':') {
        if (id->class != 0 || !(id->type == 0 || id->type == -1))
            die("invalid label");
        id->type = -1; // hack for id->class deficiency
        *--n = (int)id;
        *--n = Label;
        *--n = (int)*tt;
        *--n = '{';
        *tt = n;
        next();
        next();
    }
}

static void loc_array_decl(int ct, int extent[3], int* dims, int* et, int* size) {
    int ii = 0; // keep this to disable frame optimization for now.
    *dims = 0;
    do {
        next();
        if (*dims == 0 && ct == Par && tk == ']') {
            extent[*dims] = 1;
            next();
        } else {
            expr(Cond);
            if (*n != Num)
                die("non-const array size");
            if (n[1] <= 0)
                die("non-positive array dimension");
            if (tk != ']')
                die("missing ]");
            next();
            extent[*dims] = n[1];
            *size *= n[1];
            n += 2;
        }
        ++*dims;
    } while (tk == Bracket && *dims < 3);
    if (tk == Bracket)
        die("three subscript max on decl");
    switch (*dims) {
    case 1:
        *et = (extent[0] - 1);
        break;
    case 2:
        *et = ((extent[0] - 1) << 16) + (extent[1] - 1);
        if (extent[0] > 32768 || extent[1] > 65536)
            die("max bounds [32768][65536]");
        break;
    case 3:
        *et = ((extent[0] - 1) << 21) + ((extent[1] - 1) << 11) + (extent[2] - 1);
        if (extent[0] > 1024 || extent[1] > 1024 || extent[2] > 2048)
            die("max bounds [1024][1024][2048]");
        break;
    }
}

// statement parsing (syntax analysis, except for declarations)
static void stmt(int ctx) {
    struct ident_s* dd;
    int *a, *b, *c, *d;
    int i, j, nf, atk, sz;
    int nd[3];
    int bt;

    if (ctx == Glo && (tk < Enum || tk > Union))
        die("syntax: statement used outside function");

    switch (tk) {
    case Enum:
        next();
        // If current token is not "{", it means having enum type name.
        // Skip the enum type name.
        if (tk == Id)
            next();
        if (tk == '{') {
            next();
            i = 0; // Enum value starts from 0
            while (tk != '}') {
                // Current token should be enum name.
                // If current token is not identifier, stop parsing.
                if (tk != Id)
                    die("bad enum identifier");
                dd = id;
                next();
                if (tk == Assign) {
                    next();
                    expr(Cond);
                    if (*n != Num)
                        die("bad enum initializer");
                    i = n[1];
                    n += 2; // Set enum value
                }
                dd->class = Num;
                dd->type = INT;
                dd->val = i++;
                if (tk == ',')
                    next(); // If current token is ",", skip.
            }
            next(); // Skip "}"
        } else if (tk == Id) {
            if (ctx != Par)
                die("enum can only be declared as parameter");
            id->type = INT;
            id->class = ctx;
            id->val = ld++;
            ir_var[ir_count++] = id;
            next();
        }
        return;
    case Char:
    case Int:
    case Float:
    case Struct:
    case Union:
        switch (tk) {
        case Char:
        case Int:
        case Float:
            bt = (tk - Char) << 2;
            next();
            break;
        case Struct:
        case Union:
            atk = tk;
            next();
            if (tk == Id) {
                if (!id->type)
                    id->type = tnew++ << 2;
                bt = id->type;
                next();
            } else {
                bt = tnew++ << 2;
            }
            if (tk == '{') {
                next();
                if (members[bt >> 2])
                    die("duplicate structure definition");
                tsize[bt >> 2] = 0; // for unions
                i = 0;
                while (tk != '}') {
                    int mbt = INT; // Enum
                    switch (tk) {
                    case Char:
                    case Int:
                    case Float:
                        mbt = (tk - Char) << 2;
                        next();
                        break;
                    case Struct:
                    case Union:
                        next();
                        if (tk != Id || id->type <= ATOM_TYPE || id->type >= PTR)
                            die("bad struct/union declaration");
                        mbt = id->type;
                        next();
                        break;
                    }
                    while (tk != ';') {
                        ty = mbt;
                        // if the beginning of * is a pointer type,
                        // then type plus `PTR` indicates what kind of pointer
                        while (tk == Mul) {
                            next();
                            ty += PTR;
                        }
                        if (tk != Id)
                            die("bad struct member definition");
                        sz = (ty >= PTR) ? sizeof(int) : tsize[ty >> 2];
                        struct member_s* m = (struct member_s*)malloc(sizeof(struct member_s));
                        m->id = id;
                        m->etype = 0;
                        next();
                        if (tk == Bracket) {
                            j = ty;
                            loc_array_decl(0, nd, &nf, &m->etype, &sz);
                            ty = (j + PTR) | nf;
                        }
                        sz = (sz + 3) & -4;
                        m->offset = i;
                        m->type = ty;
                        m->next = members[bt >> 2];
                        members[bt >> 2] = m;
                        i += sz;
                        if (atk == Union) {
                            if (i > tsize[bt >> 2])
                                tsize[bt >> 2] = i;
                            i = 0;
                        }
                        if (tk == ',')
                            next();
                    }
                    next();
                }
                next();
                if (atk != Union)
                    tsize[bt >> 2] = i;
            }
            break;
        }
        /* parse statement such as 'int a, b, c;'
         * "enum" finishes by "tk == ';'", so the code below will be skipped.
         * While current token is not statement end or block end.
         */
        b = 0;
        while (tk != ';' && tk != '}' && tk != ',' && tk != ')') {
            ty = bt;
            // if the beginning of * is a pointer type, then type plus `PTR`
            // indicates what kind of pointer
            while (tk == Mul) {
                next();
                ty += PTR;
            }
            switch (ctx) { // check non-callable identifiers
            case Glo:
                if (tk != Id)
                    die("bad global declaration");
                if (id->class >= ctx)
                    die("duplicate global definition");
                break;
            case Loc:
                if (tk != Id)
                    die("bad local declaration");
                if (id->class >= ctx)
                    die("duplicate local definition");
                break;
            }
            next();
            dd = id;
            dd->type = ty;
            if (tk == '(') { // function
                if (b != 0)
                    die("func decl can't be mixed with var decl(s)");
                if (ctx != Glo)
                    die("nested function");
                if (ty > ATOM_TYPE && ty < PTR)
                    die("return type can't be struct");
                if (id->class == Syscall && id->val)
                    die("forward decl location failed one pass compilation");
                if (id->class == Func && id->val > (int)text && id->val < (int)e)
                    die("duplicate global definition");
                dd->etype = 0;
                dd->class = Func;       // type is function
                dd->val = (int)(e + 1); // function Pointer? offset/address
                next();
                nf = ir_count = ld = 0; // "ld" is parameter's index.
                while (tk != ')') {
                    stmt(Par);
                    dd->etype = dd->etype * 2;
                    if (ty == FLOAT) {
                        ++nf;
                        ++(dd->etype);
                    }
                    if (tk == ',')
                        next();
                }
                if (ld > 22)
                    die("maximum of 22 function parameters");
                // function etype is not like other etypes
                next();
                dd->etype = (dd->etype << 10) + (nf << 5) + ld; // prm info
                if (tk == ';') {
                    dd->val = 0;
                    goto unwind_func;
                } // fn proto
                if (tk != '{')
                    die("bad function definition");
                loc = ++ld;
                next();
                oline = -1;
                osize = -1;
                oid = 0; // optimization hint
                // Not declaration and must not be function, analyze inner block.
                // e represents the address which will store pc
                // (ld - loc) indicates memory size to allocate
                *--n = ';';
                while (tk != '}') {
                    int* t = n;
                    check_label(&t);
                    stmt(Loc);
                    if (t != n) {
                        *--n = (int)t;
                        *--n = '{';
                    }
                }
                *--n = ld - loc;
                *--n = Enter;
                if (oid && n[1] >= 64)
                    printf("--> %d: move %.*s to global scope for performance.\n", oline,
                           (oid->hash & 0x3f), oid->name);
                cas = 0;
                gen(n);
                if (src) {
                    int* base = le;
                    printf("%d: %.*s\n", line, p - lp, lp);
                    lp = p;
                    while (le < e) {
                        int off = le - base; // Func IR instruction memory offset
                        printf("%04d: ", off);
                        printf("%08x ", *++le);
                        if ((*le <= ADJ) || (*le == SYSC))
                            printf("%08x ", le[1]);
                        else
                            printf("         ");
                        printf(" %-4s", instr_str[*le]);
                        if (*le < ADJ) {
                            struct ident_s* scan;
                            ++le;
                            if (*le > (int)base && *le <= (int)e)
                                printf(" %04d\n", off + ((*le - (int)le) >> 2) + 1);
                            else if (*(le - 1) == LEA && src == 2) {
                                int ii = 0;
                                for (scan = ir_var[ii]; scan; scan = ir_var[++ii])
                                    if (loc - scan->val == *le) {
                                        printf(" %.*s (%d)\n", scan->hash & 0x3f, scan->name, *le);
                                        break;
                                    }
                            } else if ((*le & 0xf0000000) && (*le > 0 || -*le > 0x1000000)) {
                                for (scan = sym; scan->tk; ++scan)
                                    if (scan->val == *le) {
                                        printf(" &%.*s", scan->hash & 0x3f, scan->name);
                                        if (src == 2)
                                            printf(" (0x%08x)", *le);
                                        printf("\n");
                                        break;
                                    }
                                if (!scan->tk)
                                    printf(" 0x%08x\n", *le);
                            } else
                                printf(" %d\n", *le);
                        } else if (*le == ADJ) {
                            ++le;
                            printf(" %d\n", *le & 0xf);
                        } else if (*le == SYSC) {
                            printf(" %s\n", ef_cache[*(++le)]);
                        } else
                            printf("\n");
                    }
                }
            unwind_func:
                id = sym;
                if (src)
                    memset(ir_var, 0, sizeof(struct ident_s*) * MAX_IR);
                while (id->tk) { // unwind symbol table locals
                    if (id->class == Loc || id->class == Par) {
                        id->class = id->hclass;
                        id->type = id->htype;
                        id->val = id->hval;
                        id->etype = id->hetype;
                    } else if (id->class == Label) { // clear id for next func
                        id->class = 0;
                        id->val = 0;
                        id->type = 0;
                    } else if (id->class == 0 && id->type == -1)
                        die("%d: label %.*s not defined\n", line, id->hash & 0x3f, id->name);
                    id++;
                }
            } else {
                dd->hclass = dd->class;
                dd->class = ctx;
                dd->htype = dd->type;
                dd->type = ty;
                dd->hval = dd->val;
                dd->hetype = dd->etype;
                sz = (ty >= PTR) ? sizeof(int) : tsize[ty >> 2];
                if (tk == Bracket) {
                    i = ty;
                    loc_array_decl(ctx, nd, &j, &dd->etype, &sz);
                    ty = (i + PTR) | j;
                    dd->type = ty;
                }
                sz = (sz + 3) & -4;
                if (ctx == Loc && sz > osize) {
                    osize = sz;
                    oline = line;
                    oid = dd;
                }
                if (ctx == Glo) {
                    dd->val = (int)data;
                    data += sz;
                } else if (ctx == Loc) {
                    dd->val = (ld += sz / sizeof(int));
                    ir_var[ir_count++] = dd;
                } else if (ctx == Par) {
                    if (ty > ATOM_TYPE && ty < PTR) // local struct decl
                        die("struct parameters must be pointers");
                    dd->val = ld++;
                    ir_var[ir_count++] = dd;
                }
                if (tk == Assign) {
                    next();
                    if (ctx == Par)
                        die("default arguments not supported");
                    if (tk == '{' && (dd->type & 3))
                        init_array(dd, nd, j);
                    else {
                        if (b == 0)
                            *--n = ';';
                        if (ctx != Loc)
                            die("decl assignment for local vars only");
                        b = n;
                        *--n = loc - dd->val;
                        *--n = Loc;
                        a = n;
                        i = ty;
                        expr(Assign);
                        typecheck(Assign, i, ty);
                        *--n = (int)a;
                        *--n = (ty << 16) | i;
                        *--n = Assign;
                        ty = i;
                        *--n = (int)b;
                        *--n = '{';
                    }
                }
            }
            if (ctx != Par && tk == ',')
                next();
        }
        return;
    case If:
        next();
        if (tk != '(')
            die("open parenthesis expected");
        next();
        expr(Assign);
        a = n;
        if (tk != ')')
            die("close parenthesis expected");
        next();
        stmt(ctx);
        b = n;
        if (tk == Else) {
            next();
            stmt(ctx);
            d = n;
        } else
            d = 0;
        *--n = (int)d;
        *--n = (int)b;
        *--n = (int)a;
        *--n = Cond;
        return;
    case While:
        next();
        if (tk != '(')
            die("open parenthesis expected");
        next();
        expr(Assign);
        b = n; // condition
        if (tk != ')')
            die("close parenthesis expected");
        next();
        ++brkc;
        ++cntc;
        stmt(ctx);
        a = n; // parse body of "while"
        --brkc;
        --cntc;
        *--n = (int)b;
        *--n = (int)a;
        *--n = While;
        return;
    case DoWhile:
        next();
        ++brkc;
        ++cntc;
        stmt(ctx);
        a = n; // parse body of "do-while"
        --brkc;
        --cntc;
        if (tk != While)
            die("while expected");
        next();
        if (tk != '(')
            die("open parenthesis expected");
        next();
        *--n = ';';
        expr(Assign);
        b = n;
        if (tk != ')')
            die("close parenthesis expected");
        next();
        *--n = (int)b;
        *--n = (int)a;
        *--n = DoWhile;
        return;
    case Switch:
        i = 0;
        j = 0;
        if (cas)
            j = (int)cas;
        cas = &i;
        next();
        if (tk != '(')
            die("open parenthesis expected");
        next();
        expr(Assign);
        a = n;
        if (tk != ')')
            die("close parenthesis expected");
        next();
        ++swtc;
        ++brkc;
        stmt(ctx);
        --swtc;
        --brkc;
        b = n;
        *--n = (int)b;
        *--n = (int)a;
        *--n = Switch;
        if (j)
            cas = (int*)j;
        return;
    case Case:
        if (!swtc)
            die("case-statement outside of switch");
        i = *cas;
        next();
        expr(Or);
        a = n;
        if (*n != Num)
            die("case label not a numeric literal");
        j = n[1];
        n[1] -= i;
        *cas = j;
        *--n = ';';
        if (tk != ':')
            die("colon expected");
        next();
        stmt(ctx);
        b = n;
        *--n = (int)b;
        *--n = (int)a;
        *--n = Case;
        return;
    case Break:
        if (!brkc)
            die("misplaced break statement");
        next();
        if (tk != ';')
            die("semicolon expected");
        next();
        *--n = Break;
        return;
    case Continue:
        if (!cntc)
            die("misplaced continue statement");
        next();
        if (tk != ';')
            die("semicolon expected");
        next();
        *--n = Continue;
        return;
    case Default:
        if (!swtc)
            die("default-statement outside of switch");
        next();
        if (tk != ':')
            die("colon expected");
        next();
        stmt(ctx);
        a = n;
        *--n = (int)a;
        *--n = Default;
        return;
    // RETURN_stmt -> 'return' expr ';' | 'return' ';'
    case Return:
        a = 0;
        next();
        if (tk != ';') {
            expr(Assign);
            a = n;
        }
        *--n = (int)a;
        *--n = Return;
        if (tk != ';')
            die("semicolon expected");
        next();
        return;
    /* For iteration is implemented as:
     * Init -> Cond -> Bz to end -> Jmp to Body
     * After -> Jmp to Cond -> Body -> Jmp to After
     */
    case For:
        next();
        if (tk != '(')
            die("open parenthesis expected");
        next();
        *--n = ';';
        if (tk != ';')
            expr(Assign);
        while (tk == ',') {
            int* f = n;
            next();
            expr(Assign);
            *--n = (int)f;
            *--n = '{';
        }
        d = n;
        if (tk != ';')
            die("semicolon expected");
        next();
        *--n = ';';
        expr(Assign);
        a = n; // Point to entry of for cond
        if (tk != ';')
            die("semicolon expected");
        next();
        *--n = ';';
        if (tk != ')')
            expr(Assign);
        while (tk == ',') {
            int* g = n;
            next();
            expr(Assign);
            *--n = (int)g;
            *--n = '{';
        }
        b = n;
        if (tk != ')')
            die("close parenthesis expected");
        next();
        ++brkc;
        ++cntc;
        stmt(ctx);
        c = n;
        --brkc;
        --cntc;
        *--n = (int)d;
        *--n = (int)c;
        *--n = (int)b;
        *--n = (int)a;
        *--n = For;
        return;
    case Goto:
        next();
        if (tk != Id || (id->type != 0 && id->type != -1) || (id->class != Label && id->class != 0))
            die("goto expects label");
        id->type = -1; // hack for id->class deficiency
        *--n = (int)id;
        *--n = Goto;
        next();
        if (tk != ';')
            die("semicolon expected");
        next();
        return;
    // stmt -> '{' stmt '}'
    case '{':
        next();
        *--n = ';';
        while (tk != '}') {
            a = n;
            check_label(&a);
            stmt(ctx);
            *--n = (int)a;
            *--n = '{';
        }
        next();
        return;
    // stmt -> ';'
    case ';':
        next();
        *--n = ';';
        return;
    default:
        expr(Assign);
        if (tk != ';' && tk != ',')
            die("semicolon expected");
        next();
    }
}

int time_wrapper() { return time_us_32(); }

int cc(int argc, char** argv) {
    int *freed_ast = NULL, *ast = NULL;
    int i;
    lfs_file_t* fd = malloc(sizeof(lfs_file_t));

    if (setjmp(done_jmp))
        goto done;

    if (!(sym = (struct ident_s*)malloc(BIG_TBL_BYTES))) {
        printf("could not allocate symbol area");
        goto done;
    }
    memset(sym, 0, BIG_TBL_BYTES);

    // Register keywords in symbol stack. Must match the sequence of enum
    p = "enum char int float struct union sizeof return goto break continue "
        "if do while for switch case default else __clear_cache sqrtf void main";

    // call "next" to create symbol table entry.
    // store the keyword's token type in the symbol table entry's "tk" field.
    for (i = Enum; i <= Else; ++i) {
        next();
        id->tk = i;
        id->class = Keyword; // add keywords to symbol table
    }

    // add __clear_cache to symbol table
    next();
    id->class = ClearCache;
    id->type = INT;
    id->val = CLCA;
    next();
    id->class = Sqrt;
    id->type = FLOAT;
    id->val = SQRT;
    id->etype = 1057;

    next();
    id->tk = Char;
    id->class = Keyword; // handle void type
    next();
    struct ident_s* idmain = id;
    id->class = Main; // keep track of main

    if (!(freedata = data = (char*)malloc(BIG_TBL_BYTES)))
        printf("could not allocat data area");
    memset(data, 0, BIG_TBL_BYTES);
    if (!(tsize = (int*)malloc(SMALL_TBL_WRDS * sizeof(int)))) {
        printf("could not allocate tsize area");
        goto done;
    }
    memset(tsize, 0, SMALL_TBL_WRDS * sizeof(int)); // not strictly necessary
    if (!(freed_ast = ast = (int*)malloc(BIG_TBL_BYTES))) {
        printf("could not allocate abstract syntax tree area");
        goto done;
    }
    memset(ast, 0, BIG_TBL_BYTES);          // not strictly necessary
    ast = (int*)((int)ast + BIG_TBL_BYTES); // AST is built as a stack
    n = ast;

    // add primitive types
    tsize[tnew++] = sizeof(char);
    tsize[tnew++] = sizeof(int);
    tsize[tnew++] = sizeof(float);
    tsize[tnew++] = 0; // reserved for another scalar type

    --argc;
    ++argv;
    while (argc > 0 && **argv == '-') {
        if ((*argv)[1] == 's') {
            src = ((*argv)[2] == 'i') ? 2 : 1;
            --argc;
            ++argv;
        } else if (!strcmp(*argv, "-fsigned-char")) {
            signed_char = 1;
            --argc;
            ++argv;
        } else if ((*argv)[1] == 'D') {
            p = &(*argv)[2];
            next();
            if (tk != Id)
                die("bad -D identifier");
            struct ident_s* dd = id;
            next();
            i = 0;
            if (tk == Assign) {
                next();
                expr(Cond);
                if (*n != Num)
                    die("bad -D initializer");
                i = n[1];
                n += 2;
            }
            dd->class = Num;
            dd->type = INT;
            dd->val = i;
            --argc;
            ++argv;
        } else
            argc = 0; // bad compiler option. Force exit.
    }
    if (argc < 1) {
        printf("usage: mc [-s] [-D [symbol[=value]]] file");
        goto done;
    }

    char* fp = full_path(*argv);
    if (!fp) {
        printf("could not allocate file name area");
        goto done;
    }
    if (fs_file_open(fd, fp, LFS_O_RDONLY) < 0) {
        printf("could not open(%s)\n", fp);
        goto done;
    }

    int siz = fs_file_seek(fd, 0, LFS_SEEK_END);
    fs_file_rewind(fd);

    if (!(text = le = e = (int*)malloc(BIG_TBL_BYTES))) {
        printf("could not allocate text area");
        goto done;
    }
    memset(e, 0, BIG_TBL_BYTES);
    if (!(members = (struct member_s**)malloc(SMALL_TBL_WRDS * sizeof(struct member_s*)))) {
        printf("could not malloc() members area");
        goto done;
    }
    memset(members, 0, SMALL_TBL_WRDS * sizeof(struct member_s*));

    if (!(freep = lp = p = (char*)malloc(siz + 1))) {
        printf("could not allocate source area");
        goto done;
    }
    if (fs_file_read(fd, p, siz) < 0) {
        printf("unable to read from source file");
        //      fs_file_close(&fd);
        goto done;
    }
    p[siz] = 0;
    fs_file_close(fd);
    fd = NULL;

    // real C parser begins 00a3c214
    // parse the program
    line = 1;
    pplevt = -1;
    next();
    while (tk) {
        stmt(Glo);
        next();
    }
    int *bp, *sp, *pc;
    if (!(pc = (int*)idmain->val)) {
        printf("main() not defined\n");
        goto done;
    }
    if (src)
        goto done;

    // setup stack
    if (!(free_sp = bp = sp = (int*)malloc(BIG_TBL_BYTES))) {
        printf("could not allocate text area");
        goto done;
    }
    bp = sp = (int*)((int)sp + BIG_TBL_BYTES);
    *--sp = EXIT; // call exit if main returns
    *--sp = PSH;
    int* t = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)t;

    // run...
    int cycle = 0, a = 0;
    while (1) {
        i = *pc++;
        ++cycle;
        if (src > 1) {
            printf("%d> %.4s", cycle, instr_str[i]);
            if (i <= ADJ)
                printf(" %d\n", *pc);
            else
                printf("\n");
        }
        if (i == LEA)
            a = (int)(bp + *pc++); // load local address
        else if (i == IMM)
            a = *pc++; // load global address or immediate
        else if (i == JMP)
            pc = (int*)*pc; // jump
        else if (i == JSR) {
            *--sp = (int)(pc + 1);
            pc = (int*)*pc;
        } // jump to subroutine
        else if (i == BZ)
            pc = a ? pc + 1 : (int*)*pc; // branch if zero
        else if (i == BNZ)
            pc = a ? (int*)*pc : pc + 1; // branch if not zero
        else if (i == ENT) {
            *--sp = (int)bp;
            bp = sp;
            sp = sp - *pc++;
        } // enter subroutine
        else if (i == ADJ)
            sp = sp + *pc++; // stack adjust
        else if (i == LEV) {
            sp = bp;
            bp = (int*)*sp++;
            pc = (int*)*sp++;
        } // leave subroutine
        else if (i == LI)
            a = *(int*)a; // load int
        else if (i == LC)
            a = *(char*)a; // load char
        else if (i == SI)
            *(int*)*sp++ = a; // store int
        else if (i == SC)
            a = *(char*)* sp++ = a; // store char
        else if (i == PSH)
            *--sp = a; // push

        else if (i == OR)
            a = *sp++ | a;
        else if (i == XOR)
            a = *sp++ ^ a;
        else if (i == AND)
            a = *sp++ & a;
        else if (i == EQ)
            a = *sp++ == a;
        else if (i == NE)
            a = *sp++ != a;
        else if (i == LT)
            a = *sp++ < a;
        else if (i == GT)
            a = *sp++ > a;
        else if (i == LE)
            a = *sp++ <= a;
        else if (i == GE)
            a = *sp++ >= a;
        else if (i == SHL)
            a = *sp++ << a;
        else if (i == SHR)
            a = *sp++ >> a;
        else if (i == ADD)
            a = *sp++ + a;
        else if (i == SUB)
            a = *sp++ - a;
        else if (i == MUL)
            a = *sp++ * a;
        else if (i == DIV)
            a = *sp++ / a;
        else if (i == MOD)
            a = *sp++ % a;
        else if (i == SYSC) {
            int sysc = *pc++;
            if (sysc == SYSC_PRINTF) {
                int* hi = sp;
                int extra = a;
                asm volatile("l1: cmp  %[extra], #0 \n"
                             "    beq  l2           \n"
                             "    ldr  r0, [%[hi]] \n"
                             "    push {r0}         \n"
                             "    add  %[hi], $4    \n"
                             "    sub  %[extra], #1 \n"
                             "    b    l1           \n"
                             "l2:                   \n"
                             :
                             : [extra] "r"(extra), [hi] "r"(hi)
                             : "r0");
                asm volatile("    cmp  %[extra], #1 \n"
                             "    blt  l3           \n"
                             "    pop  {r0}         \n"
                             "    cmp  %[extra], #2 \n"
                             "    blt  l3           \n"
                             "    pop  {r1}         \n"
                             "    cmp  %[extra], #3 \n"
                             "    blt  l3           \n"
                             "    pop  {r2}         \n"
                             "    cmp  %[extra], #4 \n"
                             "    blt  l3           \n"
                             "    pop  {r3}         \n"
                             "l3:                   \n"
                             :
                             : [extra] "r"(a)
                             : "r0", "r1", "r2", "r3");
                asm volatile("bl __wrap_printf\n");
                if (a > 4) {
                    a = (a - 4) * 4;
                    asm volatile("mov r0, sp\n"
                                 "add r0, %[extra]\n"
                                 "mov sp, r0\n"
                                 :
                                 : [extra] "r"(a)
                                 : "r0");
                }
                fflush(stdout);
            } else if (sysc == SYSC_MALLOC) {
                asm volatile("    ldr  r0, [sp]     \n"
                             "    push {r0}         \n"
                             "    bl   malloc       \n"
                             "    add  sp, #4       \n"
                             :
                             :
                             : "r0");
                int* ap = &a;
                asm volatile("    str  r0, [%[rslt]]\n" : : [rslt] "r"(ap) : "r0");
            } else if (sysc == SYSC_FREE) {
                asm volatile("    ldr  r0, [sp]     \n"
                             "    bl   free         \n"
                             :
                             :
                             : "r0");
            } else if (sysc == SYSC_TIME_US_32) {
                asm volatile("    bl   time_wrapper \n");
                int* ap = &a;
                asm volatile("    str  r0, [%[rslt]]\n" : : [rslt] "r"(ap) : "r0");
            }
        } else if (i == EXIT)
            goto done;
        else {
            printf("unknown instruction = %d %s! cycle = %d\n", i, instr_str[i], cycle);
            return -1;
        }
    }
done:
    if (fd)
        free(fd);
    if (free_sp)
        free(free_sp);
    if (freep)
        free(freep);
    if (freed_ast)
        free(freed_ast);
    if (tsize)
        free(tsize);
    if (freedata)
        free(freedata);
    if (sym)
        free(sym);
    if (text)
        free(text);

    return 0;
}
