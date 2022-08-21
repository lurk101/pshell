#include "cc_dmpast.h"

#include "stdio.h"

#include "pico/stdlib.h"

#include "cc_tokns.h"

#ifndef NDEBUG
static const char* tk_names[] = {
    "Func",      "Syscall",   "Main",      "Glo",       "Par",       "Loc",       "Keyword",
    "Id",        "Load",      "Enter",     "Num",       "NumF",      "Enum",      "Char",
    "Int",       "Float",     "Struct",    "Union",     "Sizeof",    "Return",    "Goto",
    "Break",     "Continue",  "If",        "DoWhile",   "While",     "For",       "Switch",
    "Case",      "Default",   "Else",      "Label",     "Assign",    "OrAssign",  "XorAssign",
    "AndAssign", "ShlAssign", "ShrAssign", "AddAssign", "SubAssign", "MulAssign", "DivAssign",
    "ModAssign", "Cond",      "Lor",       "Lan",       "Or",        "Xor",       "And",
    "Eq",        "Ne",        "Ge",        "Lt",        "Gt",        "Le",        "Shl",
    "Shr",       "Add",       "Sub",       "Mul",       "Div",       "Mod",       "AddF",
    "SubF",      "MulF",      "DivF",      "EqF",       "NeF",       "GeF",       "LtF",
    "GtF",       "LeF",       "CastF",     "Inc",       "Dec",       "Dot",       "Arrow",
    "Bracket"};

static const int n_tks = sizeof(tk_names) / sizeof(tk_names[0]);

static int lvl;

static int dump(int n) {
	for (;;) {
		int* ip = (int*)n;
    for (int i = 0; i < lvl * 4; i++)
        printf(" ");
    int tk = *ip;
    switch (tk) {
	case Enter:
		printf("Enter %d\n", *(ip + 1));
		ip += 2;
		break;
	case '{':
		printf("{\n");
		lvl++;
		if (dump(*(ip + 1)) == 0)
			return 0;
		ip += 2;
		break;
	case ';':
		printf(";\n");
		++ip;
		break;
	case Assign:
		printf("Assign type = %08x\n", *(ip + 1));
		dump(*(ip + 2));
		ip += 3;
		break;
	case Add:
		printf("Add\n");
		dump(*(ip + 1));
		ip += 2;
		break;
	case Num:
		printf("Num val = %08x\n", *(ip + 1));
		ip += 3;
		break;
	case NumF:
		printf("NumF val = %f\n", *((float*)ip + 1));
		ip += 3;
		break;
	case Loc:
		printf("Loc addr = %08x\n", *(ip + 1));
		ip += 2;
		break;
	case Syscall:
		printf("Syscall addr=%08x\n", *(ip + 1));
		ip += 2;
		break;
	case Keyword:
		break;
    default:
        if (tk >= 128) {
            tk -= 128;
            if (tk < n_tks)
                printf("Unknow token %s %d\n", tk_names[tk], tk+ 128);
            else
                printf("Unknow token %d\n", tk+ 128);
        } else
            printf("Unknow token %d '%c'\n", tk, tk);
        return 0;
    }
	n = (int)ip;
	}
    return 1;
}

int ast_dump(int n) {
	lvl = 0;
	dump(n);
}
#endif
