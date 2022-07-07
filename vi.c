/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

/*
 * tiny vi.c: A small 'vi' clone
 * Copyright (C) 2000, 2001 Sterling Huxley <sterling@europa.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* Adapted for Raspberry Pi, 2021 lurk101 */

#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "fs.h"
#include "vi.h"

extern char* full_path(const char* name);

#define ARRAY_SIZE(x) ((uint32_t)(sizeof(x) / sizeof((x)[0])))

static int argc, optind;
static jmp_buf die_jmp;

static inline void puts_no_eol(const char* s) {
    while (*s)
        putchar_raw(*s++);
}

static int index_in_strings(const char* strings, const char* key) {
    int j, idx = 0;

    while (*strings) {
        /* Do we see "key\0" at current position in strings? */
        for (j = 0; *strings == key[j]; ++j) {
            if (*strings++ == '\0') {
                // bb_error_msg("found:'%s' i:%u", key, idx);
                return idx; /* yes */
            }
        }
        /* No.  Move to the start of the next string. */
        while (*strings++ != '\0')
            continue;
        idx++;
    }
    return -1;
}

/* "Keycodes" that report an escape sequence.
 * We use something which fits into signed char,
 * yet doesn't represent any valid Unicode character.
 * Also, -1 is reserved for error indication and we don't use it. */

static void* memrchr(const void* s, int c, size_t n) {
    char* cp = (char*)s;
    for (int i = n - 1; i >= 0; i--)
        if (cp[i] == c)
            return cp + i;
    return NULL;
}

static void* zalloc(size_t bytes) {
    char* cp = malloc(bytes);
    if (cp)
        memset(cp, 0, bytes);
    return cp;
}

static char* strchrnul(const char* s, int c) {
    while (*s != '\0' && *s != c)
        s++;
    return (char*)s;
}

static uint64_t handle_errors(uint64_t v, char** endp) {
    char next_ch = **endp;

    /* errno is already set to ERANGE by strtoXXX if value overflowed */
    if (next_ch) {
        /* "1234abcg" or out-of-range? */
        if (isalnum(next_ch) || errno)
            return -1;
        /* good number, just suspicious terminator */
        errno = EINVAL;
    }
    return v;
}

static const char* msg_memory_exhausted = "out of memory";

static void error_msg_and_die(const char* s, ...) {
    va_list p;

    va_start(p, s);
    vprintf(s, p);
    putchar_raw('\n');
    fflush(stdout);
    va_end(p);
    longjmp(die_jmp, 1);
}

static char* xvsnprintf(const char* format, ...) {
    char c;
    va_list va;
    va_start(va, format);
    int n = vsnprintf(&c, sizeof c, format, va);
    va_end(va);
    va_list p;
    char* buf = malloc(n + 1);
    va_start(va, format);
    vsnprintf(buf, n + 1, format, va);
    va_end(va);
    return buf;
}

/* clang-format off */
enum {
    KEYCODE_UP = -2,
    KEYCODE_DOWN = -3,
    KEYCODE_RIGHT = -4,
    KEYCODE_LEFT = -5,
    KEYCODE_HOME = -6,
    KEYCODE_END = -7,
    KEYCODE_INSERT = -8,
    KEYCODE_DELETE = -9,
    KEYCODE_PAGEUP = -10,
    KEYCODE_PAGEDOWN = -11,
    KEYCODE_BACKSPACE = -12, /* Used only if Alt/Ctrl/Shifted */
    KEYCODE_D = -13,         /* Used only if Alted */
    KEYCODE_CTRL_RIGHT = KEYCODE_RIGHT & ~0x40,
    KEYCODE_CTRL_LEFT = KEYCODE_LEFT & ~0x40,
    KEYCODE_ALT_RIGHT = KEYCODE_RIGHT & ~0x20,
    KEYCODE_ALT_LEFT = KEYCODE_LEFT & ~0x20,
    KEYCODE_ALT_BACKSPACE = KEYCODE_BACKSPACE & ~0x20,
    KEYCODE_ALT_D = KEYCODE_D & ~0x20,

    KEYCODE_CURSOR_POS = -0x100, /* 0xfff..fff00 */
    KEYCODE_BUFFER_SIZE = 16
};
/* clang-format on */

#define VI_MAX_SCREEN_LEN 4096
#define VI_UNDO_QUEUE_MAX 32

#define is_asciionly(a) ((uint32_t)((a)-0x20) <= 0x7e - 0x20)

enum {
    MAX_TABSTOP = 32, // sanity limit
    // User input len. Need not be extra big.
    // Lines in file being edited *can* be bigger than this.
    MAX_INPUT_LEN = 128,
    // Sanity limits. We have only one buffer of this size.
    MAX_SCR_COLS = VI_MAX_SCREEN_LEN,
    MAX_SCR_ROWS = VI_MAX_SCREEN_LEN,
};

// VT102 ESC sequences.
// See "Xterm Control Sequences"
#define ESC "\033"
// Inverse/Normal text
#define ESC_BOLD_TEXT ESC "[7m"
#define ESC_NORM_TEXT ESC "[m"
// Bell
#define ESC_BELL "\007"
// Clear-to-end-of-line
#define ESC_CLEAR2EOL ESC "[K"
// Clear-to-end-of-screen.
// (We use default param here.
// Full sequence is "ESC [ <num> J",
// <num> is 0/1/2 = "erase below/above/all".)
#define ESC_CLEAR2EOS ESC "[J"
// Cursor to given coordinate (1,1: top left)
#define ESC_SET_CURSOR_POS ESC "[%u;%uH"
#define ESC_SET_CURSOR_TOPLEFT ESC "[H"

// cmds modifying text[]
static const char modifying_cmds[] = "aAcCdDiIJoOpPrRsxX<>~";

enum {
    YANKONLY = false,
    YANKDEL = true,
    FORWARD = 1, // code depends on "1"  for array index
    BACK = -1,   // code depends on "-1" for array index
    LIMITED = 0, // char_search() only current line
    FULL = 1,    // char_search() to the end/beginning of entire text
    PARTIAL = 0, // buffer contains partial line
    WHOLE = 1,   // buffer contains whole lines
    MULTI = 2,   // buffer may include newlines

    S_BEFORE_WS = 1, // used in skip_thing() for moving "dot"
    S_TO_WS = 2,     // used in skip_thing() for moving "dot"
    S_OVER_WS = 3,   // used in skip_thing() for moving "dot"
    S_END_PUNCT = 4, // used in skip_thing() for moving "dot"
    S_END_ALNUM = 5, // used in skip_thing() for moving "dot"

    C_END = -1, // cursor is at end of line due to '$' command
};

struct globals {
    // many references - keep near the top of globals
    char *text, *end; // pointers to the user data in memory
    char* dot;        // where all the action takes place
    int text_size;    // size of the allocated buffer

    // the rest
    int16_t vi_setops; // set by setops()
#define VI_AUTOINDENT (1 << 0)
#define VI_EXPANDTAB (1 << 1)
#define VI_ERR_METHOD (1 << 2)
#define VI_IGNORECASE (1 << 3)
#define VI_SHOWMATCH (1 << 4)
#define VI_TABSTOP (1 << 5)
#define autoindent (vi_setops & VI_AUTOINDENT)
#define expandtab (vi_setops & VI_EXPANDTAB)
#define err_method (vi_setops & VI_ERR_METHOD) // indicate error with beep or flash
#define ignorecase (vi_setops & VI_IGNORECASE)
#define showmatch (vi_setops & VI_SHOWMATCH)
    // order of constants and strings must match
#define OPTS_STR                                                                                   \
    "ai\0"                                                                                         \
    "autoindent\0"                                                                                 \
    "et\0"                                                                                         \
    "expandtab\0"                                                                                  \
    "fl\0"                                                                                         \
    "flash\0"                                                                                      \
    "ic\0"                                                                                         \
    "ignorecase\0"                                                                                 \
    "sm\0"                                                                                         \
    "showmatch\0"                                                                                  \
    "ts\0"                                                                                         \
    "tabstop\0"

#define SET_READONLY_FILE(flags) ((void)0)
#define SET_READONLY_MODE(flags) ((void)0)
#define UNSET_READONLY_FILE(flags) ((void)0)

    int16_t editing;         // >0 while we are editing a file
                             // [code audit says "can be 0, 1 or 2 only"]
    int16_t cmd_mode;        // 0=command  1=insert 2=replace
    int modified_count;      // buffer contents changed if !0
    int last_modified_count; // = -1;
    int cmdcnt;              // repetition count
    uint32_t rows, columns;  // the terminal screen is this size
    int crow, ccol;        // cursor is on Crow x Ccol
    int offset;            // chars scrolled off the screen to the left
    int have_status_msg;   // is default edit status needed?
                           // [don't make int16_t!]
    int last_status_cksum; // hash of current status line
    char* current_filename;
    char* screenbegin; // index into text[], of top line on the screen
    char* screen;      // pointer to the virtual screen buffer
    int screensize;    //            and its size
    int tabstop;
    int last_search_char;    // last char searched for (int because of Unicode)
    int16_t last_search_cmd; // command used to invoke last char search
    char last_input_char; // last char read from user
    char undo_queue_state; // One of UNDO_INS, UNDO_DEL, UNDO_EMPTY

    int16_t adding2q;      // are we currently adding user input to q
    int lmc_len;           // length of last_modifying_cmd
    char *ioq, *ioq_start; // pointer to string for get_one_char to "read"
    int dotcnt;            // number of times to repeat '.' command
    char* last_search_pattern; // last pattern from a '/' or '?' search
    int indentcol; // column of recently autoindent, 0 or -1
    int16_t cmd_error;

    // former statics
    char* edit_file_cur_line;
    int refresh_old_offset;
    int format_edit_status_tot;

    // a few references only
    uint16_t YDreg; //,Ureg;// default delete register and orig line for "U"
#define Ureg 27
    char* reg[28];    // named register a-z, "D", and "U" 0-25,26,27
    char regtype[28]; // buffer type: WHOLE, MULTI or PARTIAL
    char* mark[28];   // user marks points somewhere in text[]-  a-z and previous context ''
    int cindex;         // saved character index for up/down motion
    int16_t keep_index; // retain saved character index
    char readbuffer[KEYCODE_BUFFER_SIZE];
#define STATUS_BUFFER_LEN 200
    char status_buffer[STATUS_BUFFER_LEN]; // messages to the user
    char last_modifying_cmd[MAX_INPUT_LEN]; // last modifying cmd for "."
    char get_input_line_buf[MAX_INPUT_LEN]; // former static

    char scr_out_buf[MAX_SCR_COLS + MAX_TABSTOP * 2];

// undo_push() operations
#define UNDO_INS 0
#define UNDO_DEL 1
#define UNDO_INS_CHAIN 2
#define UNDO_DEL_CHAIN 3
#define UNDO_INS_QUEUED 4
#define UNDO_DEL_QUEUED 5

// Pass-through flags for functions that can be undone
#define NO_UNDO 0
#define ALLOW_UNDO 1
#define ALLOW_UNDO_CHAIN 2
#define ALLOW_UNDO_QUEUED 3

    struct undo_object {
        struct undo_object* prev; // Linking back avoids list traversal (LIFO)
        int start;                // Offset where the data should be restored/deleted
        int length;               // total data size
        uint8_t u_type;           // 0=deleted, 1=inserted, 2=swapped
        char undo_text[1];        // text that was deleted (if deletion)
    } * undo_stack_tail;
#define UNDO_USE_SPOS 32
#define UNDO_EMPTY 64
    char* undo_queue_spos; // Start position of queued operation
    int undo_q;
    char undo_queue[VI_UNDO_QUEUE_MAX];
};

#define text (G.text)
#define text_size (G.text_size)
#define end (G.end)
#define dot (G.dot)
#define reg (G.reg)

#define vi_setops (G.vi_setops)
#define editing (G.editing)
#define cmd_mode (G.cmd_mode)
#define modified_count (G.modified_count)
#define last_modified_count (G.last_modified_count)
#define cmdcnt (G.cmdcnt)
#define rows (G.rows)
#define columns (G.columns)
#define crow (G.crow)
#define ccol (G.ccol)
#define offset (G.offset)
#define status_buffer (G.status_buffer)
#define have_status_msg (G.have_status_msg)
#define last_status_cksum (G.last_status_cksum)
#define current_filename (G.current_filename)
#define screen (G.screen)
#define screensize (G.screensize)
#define screenbegin (G.screenbegin)
#define tabstop (G.tabstop)
#define last_search_char (G.last_search_char)
#define last_search_cmd (G.last_search_cmd)
#define readonly_mode 0
#define adding2q (G.adding2q)
#define lmc_len (G.lmc_len)
#define ioq (G.ioq)
#define ioq_start (G.ioq_start)
#define dotcnt (G.dotcnt)
#define last_search_pattern (G.last_search_pattern)
#define indentcol (G.indentcol)
#define cmd_error (G.cmd_error)

#define edit_file_cur_line (G.edit_file_cur_line)
#define refresh_old_offset (G.refresh_old_offset)
#define format_edit_status_tot (G.format_edit_status_tot)

#define YDreg (G.YDreg)
#define regtype (G.regtype)
#define mark (G.mark)
#define restart (G.restart)
#define term_orig (G.term_orig)
#define cindex (G.cindex)
#define keep_index (G.keep_index)
#define initial_cmds (G.initial_cmds)
#define readbuffer (G.readbuffer)
#define scr_out_buf (G.scr_out_buf)
#define last_modifying_cmd (G.last_modifying_cmd)
#define get_input_line_buf (G.get_input_line_buf)

#define undo_stack_tail (G.undo_stack_tail)
#define undo_queue_state (G.undo_queue_state)
#define undo_q (G.undo_q)
#define undo_queue (G.undo_queue)
#define undo_queue_spos (G.undo_queue_spos)

static struct globals G;

// sleep for 'h' 1/100 seconds, return 1/0 if stdin is (ready for read)/(not ready)
static int sleep(int ms) {
    if (ms)
        busy_wait_us_32(ms * 1000);
    return uart_is_readable(uart_default) ? 1 : 0;
}

//----- Terminal Drawing ---------------------------------------
// The terminal is made up of 'rows' line of 'columns' columns.
// classically this would be 24 x 80.
//  screen coordinates
//  0,0     ...     0,79
//  1,0     ...     1,79
//  .       ...     .
//  .       ...     .
//  22,0    ...     22,79
//  23,0    ...     23,79   <- status line

//----- Move the cursor to row x col (count from 0, not 1) -------
static void place_cursor(int row, int col) {
    char cm1[sizeof(ESC_SET_CURSOR_POS) + sizeof(int) * 3 * 2];

    if (row < 0)
        row = 0;
    if (row >= rows)
        row = rows - 1;
    if (col < 0)
        col = 0;
    if (col >= columns)
        col = columns - 1;

    sprintf(cm1, ESC_SET_CURSOR_POS, row + 1, col + 1);
    puts_no_eol(cm1);
}

//----- Erase from cursor to end of line -----------------------
static void clear_to_eol(void) { puts_no_eol(ESC_CLEAR2EOL); }

static void go_bottom_and_clear_to_eol(void) {
    place_cursor(rows - 1, 0);
    clear_to_eol();
}

//----- Start standout mode ------------------------------------
static void standout_start(void) { puts_no_eol(ESC_BOLD_TEXT); }

//----- End standout mode --------------------------------------
static void standout_end(void) { puts_no_eol(ESC_NORM_TEXT); }

//----- Text Movement Routines ---------------------------------
static char* begin_line(char* p) // return pointer to first char cur line
{
    if (p > text) {
        p = memrchr(text, '\n', p - text);
        if (!p)
            return text;
        return p + 1;
    }
    return p;
}

static char* end_line(char* p) // return pointer to NL of cur line
{
    if (p < end - 1) {
        p = memchr(p, '\n', end - p - 1);
        if (!p)
            return end - 1;
    }
    return p;
}

static char* dollar_line(char* p) // return pointer to just before NL line
{
    p = end_line(p);
    // Try to stay off of the Newline
    if (*p == '\n' && (p - begin_line(p)) > 0)
        p--;
    return p;
}

static char* prev_line(char* p) // return pointer first char prev line
{
    p = begin_line(p); // goto beginning of cur line
    if (p > text && p[-1] == '\n')
        p--;           // step to prev line
    p = begin_line(p); // goto beginning of prev line
    return p;
}

static char* next_line(char* p) // return pointer first char next line
{
    p = end_line(p);
    if (p < end - 1 && *p == '\n')
        p++; // step to next line
    return p;
}

//----- Text Information Routines ------------------------------
static char* end_screen(void) {
    char* q;
    int cnt;

    // find new bottom line
    q = screenbegin;
    for (cnt = 0; cnt < rows - 2; cnt++)
        q = next_line(q);
    q = end_line(q);
    return q;
}

// count line from start to stop
static int count_lines(char* start, char* stop) {
    char* q;
    int cnt;

    if (stop < start) { // start and stop are backwards- reverse them
        q = start;
        start = stop;
        stop = q;
    }
    cnt = 0;
    stop = end_line(stop);
    while (start <= stop && start <= end - 1) {
        start = end_line(start);
        if (*start == '\n')
            cnt++;
        start++;
    }
    return cnt;
}

static char* find_line(int li) // find beginning of line #li
{
    char* q;

    for (q = text; li > 1; li--) {
        q = next_line(q);
    }
    return q;
}

static int next_tabstop(int col) { return col + ((tabstop - 1) - (col % tabstop)); }

static int prev_tabstop(int col) { return col - ((col % tabstop) ?: tabstop); }

static int next_column(char c, int co) {
    if (c == '\t')
        co = next_tabstop(co);
    else if ((uint8_t)c < ' ' || c == 0x7f)
        co++; // display as ^X, use 2 columns
    return co + 1;
}

static int get_column(char* p) {
    const char* r;
    int co = 0;

    for (r = begin_line(p); r < p; r++)
        co = next_column(*r, co);
    return co;
}

//----- Erase the Screen[] memory ------------------------------
static void screen_erase(void) {
    memset(screen, ' ', screensize); // clear new screen
}

static void new_screen(int ro, int co) {
    char* s;

    if (screen)
        free(screen);
    screensize = ro * co + 8;
    s = screen = malloc(screensize);
    // initialize the new screen. assume this will be a empty file.
    screen_erase();
    // non-existent text[] lines start with a tilde (~).
    // screen[(1 * co) + 0] = '~';
    // screen[(2 * co) + 0] = '~';
    //..
    // screen[((ro-2) * co) + 0] = '~';
    ro -= 2;
    while (--ro >= 0) {
        s += co;
        *s = '~';
    }
}

//----- Synchronize the cursor to Dot --------------------------
static void sync_cursor(char* d, int* row, int* col) {
    char* beg_cur; // begin and end of "d" line
    char* tp;
    int cnt, ro, co;

    beg_cur = begin_line(d); // first char of cur line

    if (beg_cur < screenbegin) {
        // "d" is before top line on screen
        // how many lines do we have to move
        cnt = count_lines(beg_cur, screenbegin);
    sc1:
        screenbegin = beg_cur;
        if (cnt > (rows - 1) / 2) {
            // we moved too many lines. put "dot" in middle of screen
            for (cnt = 0; cnt < (rows - 1) / 2; cnt++) {
                screenbegin = prev_line(screenbegin);
            }
        }
    } else {
        char* end_scr;          // begin and end of screen
        end_scr = end_screen(); // last char of screen
        if (beg_cur > end_scr) {
            // "d" is after bottom line on screen
            // how many lines do we have to move
            cnt = count_lines(end_scr, beg_cur);
            if (cnt > (rows - 1) / 2)
                goto sc1; // too many lines
            for (ro = 0; ro < cnt - 1; ro++) {
                // move screen begin the same amount
                screenbegin = next_line(screenbegin);
                // now, move the end of screen
                end_scr = next_line(end_scr);
                end_scr = end_line(end_scr);
            }
        }
    }
    // "d" is on screen- find out which row
    tp = screenbegin;
    for (ro = 0; ro < rows - 1; ro++) { // drive "ro" to correct row
        if (tp == beg_cur)
            break;
        tp = next_line(tp);
    }

    // find out what col "d" is on
    co = 0;
    do {                 // drive "co" to correct column
        if (*tp == '\n') // vda || *tp == '\0')
            break;
        co = next_column(*tp, co) - 1;
        // inserting text before a tab, don't include its position
        if (cmd_mode && tp == d - 1 && *d == '\t') {
            co++;
            break;
        }
    } while (tp++ < d && ++co);

    // "co" is the column where "dot" is.
    // The screen has "columns" columns.
    // The currently displayed columns are  0+offset -- columns+ofset
    // |-------------------------------------------------------------|
    //               ^ ^                                ^
    //        offset | |------- columns ----------------|
    //
    // If "co" is already in this range then we do not have to adjust offset
    //      but, we do have to subtract the "offset" bias from "co".
    // If "co" is outside this range then we have to change "offset".
    // If the first char of a line is a tab the cursor will try to stay
    //  in column 7, but we have to set offset to 0.

    if (co < 0 + offset) {
        offset = co;
    }
    if (co >= columns + offset) {
        offset = co - columns + 1;
    }
    // if the first char of the line is a tab, and "dot" is sitting on it
    //  force offset to 0.
    if (d == beg_cur && *d == '\t') {
        offset = 0;
    }
    co -= offset;

    *row = ro;
    *col = co;
}

//----- Format a text[] line into a buffer ---------------------
static char* format_line(char* src /*, int li*/) {
    uint8_t c;
    int co;
    int ofs = offset;
    char* dest = scr_out_buf; // [MAX_SCR_COLS + MAX_TABSTOP * 2]

    c = '~'; // char in col 0 in non-existent lines is '~'
    co = 0;
    while (co < columns + tabstop) {
        // have we gone past the end?
        if (src < end) {
            c = *src++;
            if (c == '\n')
                break;
            if ((c & 0x80) && !is_asciionly(c)) {
                c = '.';
            }
            if (c < ' ' || c == 0x7f) {
                if (c == '\t') {
                    c = ' ';
                    //      co %    8     !=     7
                    while ((co % tabstop) != (tabstop - 1)) {
                        dest[co++] = c;
                    }
                } else {
                    dest[co++] = '^';
                    if (c == 0x7f)
                        c = '?';
                    else
                        c += '@'; // Ctrl-X -> 'X'
                }
            }
        }
        dest[co++] = c;
        // discard scrolled-off-to-the-left portion,
        // in tabstop-sized pieces
        if (ofs >= tabstop && co >= tabstop) {
            memmove(dest, dest + tabstop, co);
            co -= tabstop;
            ofs -= tabstop;
        }
        if (src >= end)
            break;
    }
    // check "short line, gigantic offset" case
    if (co < ofs)
        ofs = co;
    // discard last scrolled off part
    co -= ofs;
    dest += ofs;
    // fill the rest with spaces
    if (co < columns)
        memset(&dest[co], ' ', columns - co);
    return dest;
}

//----- Refresh the changed screen lines -----------------------
// Copy the source line from text[] into the buffer and note
// if the current screenline is different from the new buffer.
// If they differ then that line needs redrawing on the terminal.
//
static void refresh(int full_screen) {

    int li, changed;
    char *tp, *sp; // pointer into text[] and screen[]

    sync_cursor(dot, &crow, &ccol); // where cursor will be (on "dot")
    tp = screenbegin;               // index into text[] of top line

    // compare text[] to screen[] and mark screen[] lines that need updating
    for (li = 0; li < rows - 1; li++) {
        int cs, ce; // column start & end
        char* out_buf;
        // format current text line
        out_buf = format_line(tp /*, li*/);

        // skip to the end of the current text[] line
        if (tp < end) {
            char* t = memchr(tp, '\n', end - tp);
            if (!t)
                t = end - 1;
            tp = t + 1;
        }

        // see if there are any changes between virtual screen and out_buf
        changed = false; // assume no change
        cs = 0;
        ce = columns - 1;
        sp = &screen[li * columns]; // start of screen line
        if (full_screen) {
            // force re-draw of every single column from 0 - columns-1
            goto re0;
        }
        // compare newly formatted buffer with virtual screen
        // look forward for first difference between buf and screen
        for (; cs <= ce; cs++) {
            if (out_buf[cs] != sp[cs]) {
                changed = true; // mark for redraw
                break;
            }
        }

        // look backward for last difference between out_buf and screen
        for (; ce >= cs; ce--) {
            if (out_buf[ce] != sp[ce]) {
                changed = true; // mark for redraw
                break;
            }
        }
        // now, cs is index of first diff, and ce is index of last diff

        // if horz offset has changed, force a redraw
        if (offset != refresh_old_offset) {
        re0:
            changed = true;
        }

        // make a sanity check of columns indexes
        if (cs < 0)
            cs = 0;
        if (ce > columns - 1)
            ce = columns - 1;
        if (cs > ce) {
            cs = 0;
            ce = columns - 1;
        }
        // is there a change between virtual screen and out_buf
        if (changed) {
            // copy changed part of buffer to virtual screen
            memcpy(sp + cs, out_buf + cs, ce - cs + 1);
            place_cursor(li, cs);
            // write line out to terminal
            fwrite(&sp[cs], ce - cs + 1, 1, stdout);
            fflush(stdout);
        }
    }

    place_cursor(crow, ccol);

    if (!keep_index)
        cindex = ccol + offset;

    refresh_old_offset = offset;
}

static int safe_poll(uint8_t* buffer, int ms) {
    int c;
    absolute_time_t t;
    if (ms < 0)
        c = getchar();
    else {
        c = getchar_timeout_us(ms * 1000);
        if (c == PICO_ERROR_TIMEOUT)
            return 0;
    }
    *buffer = c;
    return 1;
}

/* Known escape sequences for cursor and function keys.
 * See "Xterm Control Sequences"
 * http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * Array should be sorted from shortest to longest.
 */
static const char esccmds[] = {
    '\x7f' | 0x80, KEYCODE_ALT_BACKSPACE, '\b' | 0x80, KEYCODE_ALT_BACKSPACE, 'd' | 0x80,
    KEYCODE_ALT_D,
    /* lineedit mimics bash: Alt-f and Alt-b are forward/backward
     * word jumps. We cheat here and make them return ALT_LEFT/RIGHT
     * keycodes. This way, lineedit need no special code to handle them.
     * If we'll need to distinguish them, introduce new ALT_F/B keycodes,
     * and update lineedit to react to them.
     */
    'f' | 0x80, KEYCODE_ALT_RIGHT, 'b' | 0x80, KEYCODE_ALT_LEFT, 'O', 'A' | 0x80, KEYCODE_UP, 'O',
    'B' | 0x80, KEYCODE_DOWN, 'O', 'C' | 0x80, KEYCODE_RIGHT, 'O', 'D' | 0x80, KEYCODE_LEFT, 'O',
    'H' | 0x80, KEYCODE_HOME, 'O', 'F' | 0x80, KEYCODE_END,
    '[', 'A' | 0x80, KEYCODE_UP, '[', 'B' | 0x80, KEYCODE_DOWN, '[', 'C' | 0x80, KEYCODE_RIGHT, '[',
    'D' | 0x80, KEYCODE_LEFT,
    /* ESC [ 1 ; 2 x, where x = A/B/C/D: Shift-<arrow> */
    /* ESC [ 1 ; 3 x, where x = A/B/C/D: Alt-<arrow> - implemented below */
    /* ESC [ 1 ; 4 x, where x = A/B/C/D: Alt-Shift-<arrow> */
    /* ESC [ 1 ; 5 x, where x = A/B/C/D: Ctrl-<arrow> - implemented below */
    /* ESC [ 1 ; 6 x, where x = A/B/C/D: Ctrl-Shift-<arrow> */
    /* ESC [ 1 ; 7 x, where x = A/B/C/D: Ctrl-Alt-<arrow> */
    /* ESC [ 1 ; 8 x, where x = A/B/C/D: Ctrl-Alt-Shift-<arrow> */
    '[', 'H' | 0x80, KEYCODE_HOME, /* xterm */
    '[', 'F' | 0x80, KEYCODE_END,  /* xterm */
    /* [ESC] ESC [ [2] H - [Alt-][Shift-]Home (End similarly?) */
    /* '[','Z'        |0x80,KEYCODE_SHIFT_TAB, */
    '[', '1', '~' | 0x80, KEYCODE_HOME, /* vt100? linux vt? or what? */
    '[', '2', '~' | 0x80, KEYCODE_INSERT,
    /* ESC [ 2 ; 3 ~ - Alt-Insert */
    '[', '3', '~' | 0x80, KEYCODE_DELETE,
    /* [ESC] ESC [ 3 [;2] ~ - [Alt-][Shift-]Delete */
    /* ESC [ 3 ; 3 ~ - Alt-Delete */
    /* ESC [ 3 ; 5 ~ - Ctrl-Delete */
    '[', '4', '~' | 0x80, KEYCODE_END, /* vt100? linux vt? or what? */
    '[', '5', '~' | 0x80, KEYCODE_PAGEUP,
    /* ESC [ 5 ; 3 ~ - Alt-PgUp */
    /* ESC [ 5 ; 5 ~ - Ctrl-PgUp */
    /* ESC [ 5 ; 7 ~ - Ctrl-Alt-PgUp */
    '[', '6', '~' | 0x80, KEYCODE_PAGEDOWN, '[', '7', '~' | 0x80,
    KEYCODE_HOME,                      /* vt100? linux vt? or what? */
    '[', '8', '~' | 0x80, KEYCODE_END, /* vt100? linux vt? or what? */
    /* '[','1',';','5','A' |0x80,KEYCODE_CTRL_UP   , - unused */
    /* '[','1',';','5','B' |0x80,KEYCODE_CTRL_DOWN , - unused */
    '[', '1', ';', '5', 'C' | 0x80, KEYCODE_CTRL_RIGHT, '[', '1', ';', '5', 'D' | 0x80,
    KEYCODE_CTRL_LEFT,
    /* '[','1',';','3','A' |0x80,KEYCODE_ALT_UP    , - unused */
    /* '[','1',';','3','B' |0x80,KEYCODE_ALT_DOWN  , - unused */
    '[', '1', ';', '3', 'C' | 0x80, KEYCODE_ALT_RIGHT, '[', '1', ';', '3', 'D' | 0x80,
    KEYCODE_ALT_LEFT,
    /* '[','3',';','3','~' |0x80,KEYCODE_ALT_DELETE, - unused */
    0};

int64_t read_key(char* buffer, int timeout) {
    const char* seq;
    int n, c;

    buffer++; /* saved chars counter is in buffer[-1] now */

start_over:
    errno = 0;
    n = (unsigned char)buffer[-1];
    if (n == 0) {
        /* If no data, wait for input.
         * If requested, wait TIMEOUT ms. TIMEOUT = -1 is useful
         * if fd can be in non-blocking mode.
         *
         * It is tempting to read more than one byte here,
         * but it breaks pasting. Example: at shell prompt,
         * user presses "c","a","t" and then pastes "\nline\n".
         * When we were reading 3 bytes here, we were eating
         * "li" too, and cat was getting wrong input.
         */
        n = safe_poll(buffer, timeout);
        if (n <= 0) {
            return -1;
        }
    }

    {
        unsigned char c = buffer[0];
        n--;
        if (n)
            memmove(buffer, buffer + 1, n);
        /* Only ESC starts ESC sequences */
        if (c != 27) {
            buffer[-1] = n;
            return c;
        }
    }

    /* Loop through known ESC sequences */
    seq = esccmds;
    while (*seq != '\0') {
        /* n - position in sequence we did not read yet */
        int i = 0; /* position in sequence to compare */

        /* Loop through chars in this sequence */
        while (1) {
            /* So far escape sequence matched up to [i-1] */
            if (n <= i) {
                /* Need more chars, read another one if it wouldn't block.
                 * Note that escape sequences come in as a unit,
                 * so if we block for long it's not really an escape sequence.
                 * Timeout is needed to reconnect escape sequences
                 * split up by transmission over a serial console. */
                errno = 0;
                if (safe_poll(buffer + n, 2) <= 0) {
                    /* No more data!
                     * Array is sorted from shortest to longest,
                     * we can't match anything later in array -
                     * anything later is longer than this seq.
                     * Break out of both loops. */
                    if (n == 0)
                        return 27;
                    return -1;
                }
                n++;
            }
            if (buffer[i] != (seq[i] & 0x7f)) {
                /* This seq doesn't match, go to next */
                seq += i;
                /* Forward to last char */
                while (!(*seq & 0x80))
                    seq++;
                /* Skip it and the keycode which follows */
                seq += 2;
                break;
            }
            if (seq[i] & 0x80) {
                /* Entire seq matched */
                n = 0;
                /* n -= i; memmove(...);
                 * would be more correct,
                 * but we never read ahead that much,
                 * and n == i here. */
                buffer[-1] = 0;
                return (signed char)seq[i + 1];
            }
            i++;
        }
    }
    /* We did not find matching sequence.
     * We possibly read and stored more input in buffer[] by now.
     * n = bytes read. Try to read more until we time out.
     */
got_all:

    if (n <= 1) {
        /* Alt-x is usually returned as ESC x.
         * Report ESC, x is remembered for the next call.
         */
        buffer[-1] = n;
        return 27;
    }

    /* We were doing "buffer[-1] = n; return c;" here, but this results
     * in unknown key sequences being interpreted as ESC + garbage.
     * This was not useful. Pretend there was no key pressed,
     * go and wait for a new keypress:
     */
    buffer[-1] = 0;
    goto start_over;
}

static int readit(void) // read (maybe cursor) key from stdin
{
    fflush(stdout);
    return read_key(readbuffer, -1);
}

static int get_one_char(void) {
    int c;

    if (!adding2q) {
        // we are not adding to the q.
        // but, we may be reading from a saved q.
        // (checking "ioq" for NULL is wrong, it's not reset to NULL
        // when done - "ioq_start" is reset instead).
        if (ioq_start != NULL) {
            // there is a queue to get chars from.
            // careful with correct sign expansion!
            c = (uint8_t)*ioq++;
            if (c != '\0')
                return c;
            // the end of the q
            free(ioq_start);
            ioq_start = NULL;
            // read from STDIN:
        }
        return readit();
    }
    // we are adding STDIN chars to q.
    c = readit();
    if (lmc_len >= ARRAY_SIZE(last_modifying_cmd) - 2) {
        // last_modifying_cmd[] is too small, can't remember the cmd
        // - drop it
        adding2q = 0;
        lmc_len = 0;
    } else {
        last_modifying_cmd[lmc_len++] = c;
    }
    return c;
}

// Get type of thing to operate on and adjust count
static int get_motion_char(void) {
    int c, cnt;

    c = get_one_char();
    if (isdigit(c)) {
        if (c != '0') {
            // get any non-zero motion count
            for (cnt = 0; isdigit(c); c = get_one_char())
                cnt = cnt * 10 + (c - '0');
            cmdcnt = (cmdcnt ?: 1) * cnt;
        } else {
            // ensure standalone '0' works
            cmdcnt = 0;
        }
    }

    return c;
}

// Get input line (uses "status line" area)
static char* get_input_line(const char* prompt) {
    // char [MAX_INPUT_LEN]

    int c;
    int i;

    strcpy(get_input_line_buf, prompt);
    last_status_cksum = 0; // force status update
    go_bottom_and_clear_to_eol();
    puts_no_eol(get_input_line_buf); // write out the :, /, or ? prompt

    i = strlen(get_input_line_buf);
    while (i < MAX_INPUT_LEN - 1) {
        c = get_one_char();
        if (c == '\n' || c == '\r' || c == 27)
            break; // this is end of input
        if (c == 8 || c == 127) {
            // user wants to erase prev char
            puts_no_eol("\b \b"); // erase char on screen
            get_input_line_buf[--i] = '\0';
            if (i <= 0) // user backs up before b-o-l, exit
                break;
        } else if (c > 0 && c < 256) { // exclude Unicode
            get_input_line_buf[i] = c;
            get_input_line_buf[++i] = '\0';
            putchar_raw(c);
        }
    }
    refresh(false);
    return get_input_line_buf;
}

// show file status on status line
static int format_edit_status(void) {
    static const char cmd_mode_indicator[] = "-IR-";

    int cur, percent, ret, trunc_at;

    // modified_count is now a counter rather than a flag.  this
    // helps reduce the amount of line counting we need to do.
    // (this will cause a mis-reporting of modified status
    // once every MAXINT editing operations.)

    // it would be nice to do a similar optimization here -- if
    // we haven't done a motion that could have changed which line
    // we're on, then we shouldn't have to do this count_lines()
    cur = count_lines(text, dot);

    // count_lines() is expensive.
    // Call it only if something was changed since last time
    // we were here:
    if (modified_count != last_modified_count) {
        format_edit_status_tot = cur + count_lines(dot, end - 1) - 1;
        last_modified_count = modified_count;
    }

    //    current line         percent
    //   -------------    ~~ ----------
    //    format_edit_status_total lines            100
    if (format_edit_status_tot > 0) {
        percent = (100 * cur) / format_edit_status_tot;
    } else {
        cur = format_edit_status_tot = 0;
        percent = 100;
    }

    trunc_at = columns < STATUS_BUFFER_LEN - 1 ? columns : STATUS_BUFFER_LEN - 1;

    ret = snprintf(status_buffer, trunc_at + 1, "%c %s%s %d/%d %d%%",
                   cmd_mode_indicator[cmd_mode & 3],
                   (current_filename != NULL ? current_filename : "No file"),
                   (modified_count ? " [Modified]" : ""), cur, format_edit_status_tot, percent);

    if (ret >= 0 && ret < trunc_at)
        return ret; // it all fit

    return trunc_at; // had to truncate
}

static int bufsum(char* buf, int count) {
    int sum = 0;
    char* e = buf + count;
    while (buf < e)
        sum += (uint8_t)*buf++;
    return sum;
}

static void redraw(int full_screen);

static void Hit_Return(void) {
    int c;

    standout_start();
    puts_no_eol("[Hit return to continue]");
    standout_end();
    while ((c = get_one_char()) != '\n' && c != '\r')
        continue;
    redraw(true); // force redraw all
}

static void show_status_line(void) {
    int cnt = 0, cksum = 0;

    // either we already have an error or status message, or we
    // create one.
    if (!have_status_msg) {
        cnt = format_edit_status();
        cksum = bufsum(status_buffer, cnt);
    }
    if (have_status_msg || ((cnt > 0 && last_status_cksum != cksum))) {
        last_status_cksum = cksum; // remember if we have seen this line
        go_bottom_and_clear_to_eol();
        puts_no_eol(status_buffer);
        if (have_status_msg) {
            if (((int)strlen(status_buffer) - (have_status_msg - 1)) > (columns - 1)) {
                have_status_msg = 0;
                Hit_Return();
            }
            have_status_msg = 0;
        }
        place_cursor(crow, ccol); // put cursor back in correct place
    }
    fflush(stdout);
}

//----- Force refresh of all Lines -----------------------------
static void redraw(int full_screen) {
    // cursor to top,left; clear to the end of screen
    puts_no_eol(ESC_SET_CURSOR_TOPLEFT ESC_CLEAR2EOS);
    screen_erase();        // erase the internal screen buffer
    last_status_cksum = 0; // force status update
    refresh(full_screen);  // this will redraw the entire display
    show_status_line();
}

//----- Draw the status line at bottom of the screen -------------
//----- Flash the screen  --------------------------------------
static void flash(int ms) {
    standout_start();
    redraw(true);
    sleep(ms);
    standout_end();
    redraw(true);
}

static void indicate_error(void) {
    cmd_error = true;
    if (!err_method) {
        puts_no_eol(ESC_BELL);
    } else {
        flash(100);
    }
}

//----- format the status buffer, the bottom line of screen ------
static void status_line(const char* format, ...) {
    va_list args;

    va_start(args, format);
    vsnprintf(status_buffer, STATUS_BUFFER_LEN, format, args);
    va_end(args);

    have_status_msg = 1;
}

static void status_line_bold(const char* format, ...) {
    va_list args;

    va_start(args, format);
    strcpy(status_buffer, ESC_BOLD_TEXT);
    vsnprintf(status_buffer + (sizeof(ESC_BOLD_TEXT) - 1),
              STATUS_BUFFER_LEN - sizeof(ESC_BOLD_TEXT) - sizeof(ESC_NORM_TEXT), format, args);
    strcat(status_buffer, ESC_NORM_TEXT);
    va_end(args);

    have_status_msg = 1 + (sizeof(ESC_BOLD_TEXT) - 1) + (sizeof(ESC_NORM_TEXT) - 1);
}

static void status_line_bold_errno(const char* fn) {
    status_line_bold("'%s' %s", fn, strerror(errno));
}

// copy s to buf, convert unprintable
static void print_literal(char* buf, const char* s) {
    char* d;
    uint8_t c;

    if (!s[0])
        s = "(NULL)";

    d = buf;
    for (; *s; s++) {
        c = *s;
        if ((c & 0x80) && !is_asciionly(c))
            c = '?';
        if (c < ' ' || c == 0x7f) {
            *d++ = '^';
            c |= '@'; // 0x40
            if (c == 0x7f)
                c = '?';
        }
        *d++ = c;
        *d = '\0';
        if (d - buf > MAX_INPUT_LEN - 10) // paranoia
            break;
    }
}
static void not_implemented(const char* s) {
    char buf[MAX_INPUT_LEN];
    print_literal(buf, s);
    status_line_bold("'%s' is not implemented", buf);
}

//----- Block insert/delete, undo ops --------------------------
// copy text into a register
static char* text_yank(char* p, char* q, int dest, int buftype) {
    char* oldreg = reg[dest];
    int cnt = q - p;
    if (cnt < 0) { // they are backwards- reverse them
        p = q;
        cnt = -cnt;
    }
    // Don't free register yet.  This prevents the memory allocator
    // from reusing the free block so we can detect if it's changed.
    reg[dest] = strndup(p, cnt + 1);
    regtype[dest] = buftype;
    free(oldreg);
    return p;
}

static char what_reg(void) {
    char c;

    c = 'D'; // default to D-reg
    if (YDreg <= 25)
        c = 'a' + (char)YDreg;
    if (YDreg == 26)
        c = 'D';
    if (YDreg == 27)
        c = 'U';
    return c;
}

static void check_context(char cmd) {
    // Certain movement commands update the context.
    if (strchr(":%{}'GHLMz/?Nn", cmd) != NULL) {
        mark[27] = mark[26]; // move cur to prev
        mark[26] = dot;      // move local to cur
    }
}

static char* swap_context(char* p) // goto new context for '' command make this the current context
{
    char* tmp;

    // the current context is in mark[26]
    // the previous context is in mark[27]
    // only swap context if other context is valid
    if (text <= mark[27] && mark[27] <= end - 1) {
        tmp = mark[27];
        mark[27] = p;
        mark[26] = p = tmp;
    }
    return p;
}

static void yank_status(const char* op, const char* p, int cnt) {
    int lines, chars;

    lines = chars = 0;
    while (*p) {
        ++chars;
        if (*p++ == '\n')
            ++lines;
    }
    status_line("%s %d lines (%d chars) from [%c]", op, lines * cnt, chars * cnt, what_reg());
}

// open a hole in text[]
// might reallocate text[]! use p += text_hole_make(p, ...),
// and be careful to not use pointers into potentially freed text[]!
static uintptr_t text_hole_make(char* p, int size) // at "p", make a 'size' byte hole
{
    uintptr_t bias = 0;

    if (size <= 0)
        return bias;
    end += size; // adjust the new END
    if (end >= (text + text_size)) {
        char* new_text;
        text_size += end - (text + text_size) + 10240;
        new_text = realloc(text, text_size);
        bias = (new_text - text);
        screenbegin += bias;
        dot += bias;
        end += bias;
        p += bias;
        {
            int i;
            for (i = 0; i < ARRAY_SIZE(mark); i++)
                if (mark[i])
                    mark[i] += bias;
        }
        text = new_text;
    }
    memmove(p + size, p, end - size - p);
    memset(p, ' ', size); // clear new hole
    return bias;
}

static void undo_push(char* src, uint32_t length, int u_type);

// close a hole in text[] - delete "p" through "q", inclusive
// "undo" value indicates if this operation should be undo-able
static char* text_hole_delete(char* p, char* q, int undo) {
    char *src, *dest;
    int cnt, hole_size;

    // move forwards, from beginning
    // assume p <= q
    src = q + 1;
    dest = p;
    if (q < p) { // they are backward- swap them
        src = p + 1;
        dest = q;
    }
    hole_size = q - p + 1;
    cnt = end - src;
    switch (undo) {
    case NO_UNDO:
        break;
    case ALLOW_UNDO:
        undo_push(p, hole_size, UNDO_DEL);
        break;
    case ALLOW_UNDO_CHAIN:
        undo_push(p, hole_size, UNDO_DEL_CHAIN);
        break;
    case ALLOW_UNDO_QUEUED:
        undo_push(p, hole_size, UNDO_DEL_QUEUED);
        break;
    }
    modified_count--;
    if (src < text || src > end)
        goto thd0;
    if (dest < text || dest >= end)
        goto thd0;
    modified_count++;
    if (src >= end)
        goto thd_atend; // just delete the end of the buffer
    memmove(dest, src, cnt);
thd_atend:
    end = end - hole_size; // adjust the new END
    if (dest >= end)
        dest = end - 1; // make sure dest in below end-1
    if (end <= text)
        dest = end = text; // keep pointers valid
thd0:
    return dest;
}

// Flush any queued objects to the undo stack
static void undo_queue_commit(void) {
    // Pushes the queue object onto the undo stack
    if (undo_q > 0) {
        // Deleted character undo events grow from the end
        undo_push(undo_queue + VI_UNDO_QUEUE_MAX - undo_q, undo_q,
                  (undo_queue_state | UNDO_USE_SPOS));
        undo_queue_state = UNDO_EMPTY;
        undo_q = 0;
    }
}

static void undo_push(char* src, uint32_t length, int u_type) {
    struct undo_object* undo_entry;
    int use_spos = u_type & UNDO_USE_SPOS;

    // "u_type" values
    // UNDO_INS: insertion, undo will remove from buffer
    // UNDO_DEL: deleted text, undo will restore to buffer
    // UNDO_{INS,DEL}_CHAIN: Same as above but also calls undo_pop() when complete
    // The CHAIN operations are for handling multiple operations that the user
    // performs with a single action, i.e. REPLACE mode or find-and-replace commands
    // UNDO_{INS,DEL}_QUEUED: If queuing feature is enabled, allow use of the queue
    // for the INS/DEL operation.
    // UNDO_{INS,DEL} ORed with UNDO_USE_SPOS: commit the undo queue

    // This undo queuing functionality groups multiple character typing or backspaces
    // into a single large undo object. This greatly reduces calls to malloc() for
    // single-character operations while typing and has the side benefit of letting
    // an undo operation remove chunks of text rather than a single character.
    switch (u_type) {
    case UNDO_EMPTY: // Just in case this ever happens...
        return;
    case UNDO_DEL_QUEUED:
        if (length != 1)
            return; // Only queue single characters
        switch (undo_queue_state) {
        case UNDO_EMPTY:
            undo_queue_state = UNDO_DEL;
        case UNDO_DEL:
            undo_queue_spos = src;
            undo_q++;
            undo_queue[VI_UNDO_QUEUE_MAX - undo_q] = *src;
            // If queue is full, dump it into an object
            if (undo_q == VI_UNDO_QUEUE_MAX)
                undo_queue_commit();
            return;
        case UNDO_INS:
            // Switch from storing inserted text to deleted text
            undo_queue_commit();
            undo_push(src, length, UNDO_DEL_QUEUED);
            return;
        }
        break;
    case UNDO_INS_QUEUED:
        if (length < 1)
            return;
        switch (undo_queue_state) {
        case UNDO_EMPTY:
            undo_queue_state = UNDO_INS;
            undo_queue_spos = src;
        case UNDO_INS:
            while (length--) {
                undo_q++; // Don't need to save any data for insertions
                if (undo_q == VI_UNDO_QUEUE_MAX)
                    undo_queue_commit();
            }
            return;
        case UNDO_DEL:
            // Switch from storing deleted text to inserted text
            undo_queue_commit();
            undo_push(src, length, UNDO_INS_QUEUED);
            return;
        }
        break;
    }
    u_type &= ~UNDO_USE_SPOS;

    // Allocate a new undo object
    if (u_type == UNDO_DEL || u_type == UNDO_DEL_CHAIN) {
        // For UNDO_DEL objects, save deleted text
        if ((text + length) == end)
            length--;
        // If this deletion empties text[], strip the newline. When the buffer becomes
        // zero-length, a newline is added back, which requires this to compensate.
        undo_entry = zalloc(offsetof(struct undo_object, undo_text) + length);
        memcpy(undo_entry->undo_text, src, length);
    } else {
        undo_entry = zalloc(sizeof(*undo_entry));
    }
    undo_entry->length = length;
    if (use_spos) {
        undo_entry->start = undo_queue_spos - text; // use start position from queue
    } else {
        undo_entry->start = src - text; // use offset from start of text buffer
    }
    undo_entry->u_type = u_type;

    // Push it on undo stack
    undo_entry->prev = undo_stack_tail;
    undo_stack_tail = undo_entry;
    modified_count++;
}

static void flush_undo_data(void) {
    struct undo_object* undo_entry;

    while (undo_stack_tail) {
        undo_entry = undo_stack_tail;
        undo_stack_tail = undo_entry->prev;
        free(undo_entry);
    }
}

static void undo_push_insert(char* p, int len, int undo) {
    switch (undo) {
    case ALLOW_UNDO:
        undo_push(p, len, UNDO_INS);
        break;
    case ALLOW_UNDO_CHAIN:
        undo_push(p, len, UNDO_INS_CHAIN);
        break;
    case ALLOW_UNDO_QUEUED:
        undo_push(p, len, UNDO_INS_QUEUED);
        break;
    }
}

static uintptr_t string_insert(char* p, const char* s, int undo) // insert the string at 'p'
{
    uintptr_t bias;
    int i;

    i = strlen(s);
    undo_push_insert(p, i, undo);
    bias = text_hole_make(p, i);
    p += bias;
    memcpy(p, s, i);
    return bias;
}

// Undo the last operation
static void undo_pop(void) {
    int repeat;
    char *u_start, *u_end;
    struct undo_object* undo_entry;

    // Commit pending undo queue before popping (should be unnecessary)
    undo_queue_commit();

    undo_entry = undo_stack_tail;
    // Check for an empty undo stack
    if (!undo_entry) {
        status_line("Already at oldest change");
        return;
    }

    switch (undo_entry->u_type) {
    case UNDO_DEL:
    case UNDO_DEL_CHAIN:
        // make hole and put in text that was deleted; deallocate text
        u_start = text + undo_entry->start;
        text_hole_make(u_start, undo_entry->length);
        memcpy(u_start, undo_entry->undo_text, undo_entry->length);
        status_line("Undo [%d] %s %d chars at position %d", modified_count, "restored",
                    undo_entry->length, undo_entry->start);
        break;
    case UNDO_INS:
    case UNDO_INS_CHAIN:
        // delete what was inserted
        u_start = undo_entry->start + text;
        u_end = u_start - 1 + undo_entry->length;
        text_hole_delete(u_start, u_end, NO_UNDO);
        status_line("Undo [%d] %s %d chars at position %d", modified_count, "deleted",
                    undo_entry->length, undo_entry->start);
        break;
    }
    repeat = 0;
    switch (undo_entry->u_type) {
    // If this is the end of a chain, lower modification count and refresh display
    case UNDO_DEL:
    case UNDO_INS:
        dot = (text + undo_entry->start);
        refresh(false);
        break;
    case UNDO_DEL_CHAIN:
    case UNDO_INS_CHAIN:
        repeat = 1;
        break;
    }
    // Deallocate the undo object we just processed
    undo_stack_tail = undo_entry->prev;
    free(undo_entry);
    modified_count--;
    // For chained operations, continue popping all the way down the chain.
    if (repeat) {
        undo_pop(); // Follow the undo chain if one exists
    }
}

//----- Dot Movement Routines ----------------------------------
static void dot_left(void) {
    undo_queue_commit();
    if (dot > text && dot[-1] != '\n')
        dot--;
}

static void dot_right(void) {
    undo_queue_commit();
    if (dot < end - 1 && *dot != '\n')
        dot++;
}

static void dot_begin(void) {
    undo_queue_commit();
    dot = begin_line(dot); // return pointer to first char cur line
}

static void dot_end(void) {
    undo_queue_commit();
    dot = end_line(dot); // return pointer to last char cur line
}

static char* move_to_col(char* p, int l) {
    int co;

    p = begin_line(p);
    co = 0;
    do {
        if (*p == '\n') // vda || *p == '\0')
            break;
        co = next_column(*p, co);
    } while (co <= l && p++ < end);
    return p;
}

static void dot_next(void) {
    undo_queue_commit();
    dot = next_line(dot);
}

static void dot_prev(void) {
    undo_queue_commit();
    dot = prev_line(dot);
}

static void dot_skip_over_ws(void) {
    // skip WS
    while (isspace(*dot) && *dot != '\n' && dot < end - 1)
        dot++;
}

static void dot_to_char(int cmd) {
    char* q = dot;
    int dir = islower(cmd) ? FORWARD : BACK;

    if (last_search_char == 0)
        return;

    do {
        do {
            q += dir;
            if ((dir == FORWARD ? q > end - 1 : q < text) || *q == '\n') {
                indicate_error();
                return;
            }
        } while (*q != last_search_char);
    } while (--cmdcnt > 0);

    dot = q;

    // place cursor before/after char as required
    if (cmd == 't')
        dot_left();
    else if (cmd == 'T')
        dot_right();
}

static void dot_scroll(int cnt, int dir) {
    char* q;

    undo_queue_commit();
    for (; cnt > 0; cnt--) {
        if (dir < 0) {
            // scroll Backwards
            // ctrl-Y scroll up one line
            screenbegin = prev_line(screenbegin);
        } else {
            // scroll Forwards
            // ctrl-E scroll down one line
            screenbegin = next_line(screenbegin);
        }
    }
    // make sure "dot" stays on the screen so we dont scroll off
    if (dot < screenbegin)
        dot = screenbegin;
    q = end_screen(); // find new bottom line
    if (dot > q)
        dot = begin_line(q); // is dot is below bottom line?
    dot_skip_over_ws();
}

static char* bound_dot(char* p) // make sure  text[0] <= P < "end"
{
    if (p >= end && end > text) {
        p = end - 1;
        indicate_error();
    }
    if (p < text) {
        p = text;
        indicate_error();
    }
    return p;
}

static void start_new_cmd_q(char c) {
    // get buffer for new cmd
    dotcnt = cmdcnt ?: 1;
    last_modifying_cmd[0] = c;
    lmc_len = 1;
    adding2q = 1;
}
static void end_cmd_q(void) {
    YDreg = 26; // go back to default Yank/Delete reg
    adding2q = 0;
}

// copy text into register, then delete text.
//
static char* yank_delete(char* start, char* stop, int buftype, int yf, int undo) {
    char* p;

    // make sure start <= stop
    if (start > stop) {
        // they are backwards, reverse them
        p = start;
        start = stop;
        stop = p;
    }
    if (buftype == PARTIAL && *start == '\n')
        return start;
    p = start;
    text_yank(start, stop, YDreg, buftype);
    if (yf == YANKDEL) {
        p = text_hole_delete(start, stop, undo);
    } // delete lines
    return p;
}

// might reallocate text[]!
static int file_insert(const char* fn, char* p, int initial) {
    if (fn == NULL)
        return -1;
    int cnt = -1;
    lfs_file_t fd;
    int size;
    struct lfs_info statbuf;

    if (p < text)
        p = text;
    if (p > end)
        p = end;

    if (fs_stat(fn, &statbuf) < 0) {
        if (!initial)
            status_line_bold_errno(fn);
        return cnt;
    }

    if (statbuf.type != LFS_TYPE_REG) {
        status_line_bold("'%s' is not a regular file", fn);
        goto fi;
    }
    size = (statbuf.size < 0x7fffffff ? statbuf.size : 0x7fffffff);
    p += text_hole_make(p, size);
    if (fs_file_open(&fd, fn, LFS_O_RDONLY) < 0) {
        if (!initial)
            status_line_bold_errno(fn);
        return cnt;
    }
    cnt = fs_file_read(&fd, p, size);
    if (cnt < 0) {
        status_line_bold_errno(fn);
        p = text_hole_delete(p, p + size - 1, NO_UNDO); // un-do buffer insert
    } else if (cnt < size) {
        // There was a partial read, shrink unused space
        p = text_hole_delete(p + cnt, p + size - 1, NO_UNDO);
        status_line_bold("can't read '%s'", fn);
    }
    else {
        undo_push_insert(p, size, ALLOW_UNDO);
    }
fi:
    fs_file_close(&fd);

    return cnt;
}

// find matching char of pair  ()  []  {}
// will crash if c is not one of these
static char* find_pair(char* p, const char c) {
    const char* braces = "()[]{}";
    char match;
    int dir, level;

    dir = strchr(braces, c) - braces;
    dir ^= 1;
    match = braces[dir];
    dir = ((dir & 1) << 1) - 1; // 1 for ([{, -1 for )\}

    // look for match, count levels of pairs  (( ))
    level = 1;
    for (;;) {
        p += dir;
        if (p < text || p >= end)
            return NULL;
        if (*p == c)
            level++; // increase pair levels
        if (*p == match) {
            level--; // reduce pair level
            if (level == 0)
                return p; // found matching pair
        }
    }
}

// show the matching char of a pair,  ()  []  {}
static void showmatching(char* p) {
    char *q, *save_dot;

    // we found half of a pair
    q = find_pair(p, *p); // get loc of matching char
    if (q == NULL) {
        indicate_error(); // no matching char
    } else {
        // "q" now points to matching pair
        save_dot = dot; // remember where we are
        dot = q;        // go to new loc
        refresh(false); // let the user see it
        sleep(1000);    // give user some time
        dot = save_dot; // go back to old loc
        refresh(false);
    }
}

// might reallocate text[]! use p += stupid_insert(p, ...),
// and be careful to not use pointers into potentially freed text[]!
static uintptr_t stupid_insert(char* p, char c) // stupidly insert the char c at 'p'
{
    uintptr_t bias;
    bias = text_hole_make(p, 1);
    p += bias;
    *p = c;
    return bias;
}

// find number of characters in indent, p must be at beginning of line
static size_t indent_len(char* p) {
    char* r = p;

    while (r < (end - 1) && isblank(*r))
        r++;
    return r - p;
}

static char* char_insert(char* p, char c, int undo) // insert the char c at 'p'
{
    size_t len;
    int col, ntab, nspc;
    char* bol = begin_line(p);

    if (c == 22) {                  // Is this an ctrl-V?
        p += stupid_insert(p, '^'); // use ^ to indicate literal next
        refresh(false);             // show the ^
        c = get_one_char();
        *p = c;
        undo_push_insert(p, 1, undo);
        p++;
    } else if (c == 27) { // Is this an ESC?
        cmd_mode = 0;
        undo_queue_commit();
        cmdcnt = 0;
        end_cmd_q();           // stop adding to q
        last_status_cksum = 0; // force status update
        if ((dot > text) && (p[-1] != '\n')) {
            p--;
        }
        if (autoindent) {
            len = indent_len(bol);
            if (len && get_column(bol + len) == indentcol && bol[len] == '\n') {
                // remove autoindent from otherwise empty line
                text_hole_delete(bol, bol + len - 1, undo);
                p = bol;
            }
        }
    } else if (c == 4) { // ctrl-D reduces indentation
        char* r = bol + indent_len(bol);
        int prev = prev_tabstop(get_column(r));
        while (r > bol && get_column(r) > prev) {
            if (p > bol)
                p--;
            r--;
            r = text_hole_delete(r, r, ALLOW_UNDO_QUEUED);
        }

        if (autoindent && indentcol && r == end_line(p)) {
            // record changed size of autoindent
            indentcol = get_column(p);
            return p;
        }
    } else if (c == '\t' && expandtab) { // expand tab
        col = get_column(p);
        col = next_tabstop(col) - col + 1;
        while (col--) {
            undo_push_insert(p, 1, undo);
            p += 1 + stupid_insert(p, ' ');
        }
    } else if (c == 8 || c == 127) { // Is this a BS
        if (p > text) {
            p--;
            p = text_hole_delete(p, p, ALLOW_UNDO_QUEUED); // shrink buffer 1 char
        }
    } else {
        // insert a char into text[]
        if (c == 13)
            c = '\n'; // translate \r to \n
        if (c == '\n')
            undo_queue_commit();
        p += 1 + stupid_insert(p, c); // insert the char
        if (showmatch && strchr(")]}", c) != NULL) {
            showmatching(p - 1);
        }
        if (autoindent && c == '\n') { // auto indent the new line
            // use indent of current/previous line
            bol = indentcol < 0 ? p : prev_line(p);
            len = indent_len(bol);
            col = get_column(bol + len);

            if (len && col == indentcol) {
                // previous line was empty except for autoindent
                // move the indent to the current line
                memmove(bol + 1, bol, len);
                *bol = '\n';
                return p;
            }

            if (indentcol < 0)
                p--; // open above, indent before newly inserted NL

            if (len) {
                indentcol = col;
                if (expandtab) {
                    ntab = 0;
                    nspc = col;
                } else {
                    ntab = col / tabstop;
                    nspc = col % tabstop;
                }
                p += text_hole_make(p, ntab + nspc);
                undo_push_insert(p, ntab + nspc, undo);
                memset(p, '\t', ntab);
                p += ntab;
                memset(p, ' ', nspc);
                return p + nspc;
            }
        }
    }
    indentcol = 0;
    return p;
}

static void init_filename(char* fn) {
    if (current_filename == NULL) {
        current_filename = strdup(fn);
    }
}

static void update_filename(char* fn) {
    if (fn != current_filename) {
        if (current_filename)
            free(current_filename);
        current_filename = strdup(fn);
    }
}

// read text from file or create an empty buf
// will also update current_filename
static int init_text_buffer(char* fn) {
    int rc;

    // allocate/reallocate text buffer
    if (text)
        free(text);
    text_size = 10240;
    screenbegin = dot = end = text = zalloc(text_size);

    update_filename(fn);
    rc = file_insert(fn, text, 1);
    if (rc < 0) {
        // file doesnt exist. Start empty buf with dummy line
        char_insert(text, '\n', NO_UNDO);
    }

    flush_undo_data();
    modified_count = 0;
    last_modified_count = -1;
    // init the marks
    memset(mark, 0, sizeof(mark));
    return rc;
}

static int file_write(char* fn, char* first, char* last) {
    lfs_file_t fd;
    int cnt, charcnt;

    if (fn == 0) {
        status_line_bold("No current filename");
        return -2;
    }
    // By popular request we do not open file with O_TRUNC,
    // but instead ftruncate() it _after_ successful write.
    // Might reduce amount of data lost on power fail etc.
    if (fs_file_open(&fd, fn, LFS_O_WRONLY | LFS_O_CREAT) < 0)
        return -1;
    cnt = last - first + 1;
    charcnt = fs_file_write(&fd, first, cnt);
    fs_file_truncate(&fd, charcnt);
    if (charcnt == cnt) {
        // good write
        // modified_count = false;
    } else {
        charcnt = 0;
    }
    fs_file_close(&fd);
    return charcnt;
}

static int mycmp(const char* s1, const char* s2, int len) {
    if (ignorecase) {
        return strncasecmp(s1, s2, len);
    }
    return strncmp(s1, s2, len);
}

static char* char_search(char* p, const char* pat, int dir_and_range) {
    char *start, *stop;
    int len;
    int range;

    len = strlen(pat);
    range = (dir_and_range & 1);
    if (dir_and_range > 0) { // FORWARD?
        stop = end - 1;      // assume range is p..end-1
        if (range == LIMITED)
            stop = next_line(p); // range is to next line
        for (start = p; start < stop; start++) {
            if (mycmp(start, pat, len) == 0) {
                return start;
            }
        }
    } else {         // BACK
        stop = text; // assume range is text..p
        if (range == LIMITED)
            stop = prev_line(p); // range is to prev line
        for (start = p - len; start >= stop; start--) {
            if (mycmp(start, pat, len) == 0) {
                return start;
            }
        }
    }
    // pattern not found
    return NULL;
}

//----- The Colon commands -------------------------------------
// Evaluate colon address expression.  Returns a pointer to the
// next character or NULL on error.  If 'result' contains a valid
// address 'valid' is true.
static char* get_one_address(char* p, int* result, int* valid) {
    int num, sign, addr, got_addr;
    char *q, c;
    int dir;

    got_addr = false;
    addr = count_lines(text, dot); // default to current line
    sign = 0;
    for (;;) {
        if (isblank(*p)) {
            if (got_addr) {
                addr += sign;
                sign = 0;
            }
            p++;
        } else if (!got_addr && *p == '.') { // the current line
            p++;
            // addr = count_lines(text, dot);
            got_addr = true;
        } else if (!got_addr && *p == '$') { // the last line in file
            p++;
            addr = count_lines(text, end - 1);
            got_addr = true;
        }
        else if (!got_addr && *p == '\'') { // is this a mark addr
            p++;
            c = tolower(*p);
            p++;
            q = NULL;
            if (c >= 'a' && c <= 'z') {
                // we have a mark
                c = c - 'a';
                q = mark[(uint8_t)c];
            }
            if (q == NULL) { // is mark valid
                status_line_bold("Mark not set");
                return NULL;
            }
            addr = count_lines(text, q);
            got_addr = true;
        }
        else if (!got_addr && (*p == '/' || *p == '?')) { // a search pattern
            c = *p;
            q = strchrnul(p + 1, c);
            if (p + 1 != q) {
                // save copy of new pattern
                free(last_search_pattern);
                last_search_pattern = strndup(p, q - p);
            }
            p = q;
            if (*p == c)
                p++;
            if (c == '/') {
                q = next_line(dot);
                dir = (FORWARD << 1) | FULL;
            } else {
                q = begin_line(dot);
                dir = ((uint32_t)BACK << 1) | FULL;
            }
            q = char_search(q, last_search_pattern + 1, dir);
            if (q == NULL) {
                // no match, continue from other end of file
                q = char_search(dir > 0 ? text : end - 1, last_search_pattern + 1, dir);
                if (q == NULL) {
                    status_line_bold("Pattern not found");
                    return NULL;
                }
            }
            addr = count_lines(text, q);
            got_addr = true;
        }
        else if (isdigit(*p)) {
            num = 0;
            while (isdigit(*p))
                num = num * 10 + *p++ - '0';
            if (!got_addr) { // specific line number
                addr = num;
                got_addr = true;
            } else { // offset from current addr
                addr += sign >= 0 ? num : -num;
            }
            sign = 0;
        } else if (*p == '-' || *p == '+') {
            if (!got_addr) { // default address is dot
                // addr = count_lines(text, dot);
                got_addr = true;
            } else {
                addr += sign;
            }
            sign = *p++ == '-' ? -1 : 1;
        } else {
            addr += sign; // consume unused trailing sign
            break;
        }
    }
    *result = addr;
    *valid = got_addr;
    return p;
}

#define GET_ADDRESS 0
#define GET_SEPARATOR 1

// Read line addresses for a colon command.  The user can enter as
// many as they like but only the last two will be used.
static char* get_address(char* p, int* b, int* e, uint32_t* got) {
    int state = GET_ADDRESS;
    int valid;
    int addr;
    char* save_dot = dot;

    //----- get the address' i.e., 1,3   'a,'b  -----
    for (;;) {
        if (isblank(*p)) {
            p++;
        } else if (state == GET_ADDRESS && *p == '%') { // alias for 1,$
            p++;
            *b = 1;
            *e = count_lines(text, end - 1);
            *got = 3;
            state = GET_SEPARATOR;
        } else if (state == GET_ADDRESS) {
            valid = false;
            p = get_one_address(p, &addr, &valid);
            // Quit on error or if the address is invalid and isn't of
            // the form ',$' or '1,' (in which case it defaults to dot).
            if (p == NULL || !(valid || *p == ',' || *p == ';' || *got & 1))
                break;
            *b = *e;
            *e = addr;
            *got = (*got << 1) | 1;
            state = GET_SEPARATOR;
        } else if (state == GET_SEPARATOR && (*p == ',' || *p == ';')) {
            if (*p == ';')
                dot = find_line(*e);
            p++;
            state = GET_ADDRESS;
        } else {
            break;
        }
    }
    dot = save_dot;
    return p;
}

static void setops(char* args, int flg_no) {
    char* eq;
    int index;

    eq = strchr(args, '=');
    if (eq)
        *eq = '\0';
    index = index_in_strings(OPTS_STR, args + flg_no);
    if (eq)
        *eq = '=';
    if (index < 0) {
    bad:
        status_line_bold("bad option: %s", args);
        return;
    }

    index = 1 << (index >> 1); // convert to VI_bit

    if (index & VI_TABSTOP) {
        int t;
        if (!eq || flg_no) // no "=NNN" or it is "notabstop"?
            goto bad;
        errno = 0;
        t = strtoul(eq + 1, NULL, 10);
        if (errno == ERANGE)
            t = -1;
        if (t <= 0 || t > MAX_TABSTOP)
            goto bad;
        tabstop = t;
        return;
    }
    if (eq)
        goto bad; // boolean option has "="?
    if (flg_no) {
        vi_setops &= ~index;
    } else {
        vi_setops |= index;
    }
}

static char* skip_whitespace(const char* s) {
    /* In POSIX/C locale (the only locale we care about: do we REALLY want
     * to allow Unicode whitespace in, say, .conf files? nuts!)
     * isspace is only these chars: "\t\n\v\f\r" and space.
     * "\t\n\v\f\r" happen to have ASCII codes 9,10,11,12,13.
     * Use that.
     */
    while (*s == ' ' || (uint8_t)(*s - 9) <= (13 - 9))
        s++;

    return (char*)s;
}

static char* skip_non_whitespace(const char* s) {
    while (*s != '\0' && *s != ' ' && (uint8_t)(*s - 9) > (13 - 9))
        s++;

    return (char*)s;
}

#define strchr_backslash(s, c) strchr(s, c)

// buf must be no longer than MAX_INPUT_LEN!
static void colon(char* buf) {

// check how many addresses we got
#define GOT_ADDRESS (got & 1)
#define GOT_RANGE ((got & 3) == 3)

    char c, *buf1, *q, *r;
    char *fn, cmd[MAX_INPUT_LEN], *cmdend, *args, *exp = NULL;
    int i, l, li, b, e;
    uint32_t got;
    int useforce;

    // :3154    // if (-e line 3154) goto it  else stay put
    // :4,33w! foo    // write a portion of buffer to file "foo"
    // :w        // write all of buffer to current file
    // :q        // quit
    // :q!        // quit- dont care about modified file
    // :'a,'z!sort -u   // filter block through sort
    // :'f        // goto mark "f"
    // :'fl        // list literal the mark "f" line
    // :.r bar    // read file "bar" into buffer before dot
    // :/123/,/abc/d    // delete lines from "123" line to "abc" line
    // :/xyz/    // goto the "xyz" line
    // :s/find/replace/ // substitute pattern "find" with "replace"
    // :!<cmd>    // run <cmd> then return
    //

    while (*buf == ':')
        buf++; // move past leading colons
    while (isblank(*buf))
        buf++; // move past leading blanks
    if (!buf[0] || buf[0] == '"')
        goto ret; // ignore empty lines or those starting with '"'

    li = i = 0;
    b = e = -1;
    got = 0;
    li = count_lines(text, end - 1);
    fn = current_filename;

    // look for optional address(es)  :.  :1  :1,9   :'q,'a   :%
    buf = get_address(buf, &b, &e, &got);
    if (buf == NULL) {
        goto ret;
    }

    // get the COMMAND into cmd[]
    strcpy(cmd, buf);
    buf1 = cmd;
    while (!isspace(*buf1) && *buf1 != '\0') {
        buf1++;
    }
    cmdend = buf1;
    // get any ARGuments
    while (isblank(*buf1))
        buf1++;
    args = buf1;
    *cmdend = '\0';
    useforce = false;
    if (cmdend > cmd && cmdend[-1] == '!') {
        useforce = true;
        cmdend[-1] = '\0'; // get rid of !
    }
    // assume the command will want a range, certain commands
    // (read, substitute) need to adjust these assumptions
    if (!GOT_ADDRESS) {
        q = text; // no addr, use 1,$ for the range
        r = end - 1;
    } else {
        // at least one addr was given, get its details
        if (e < 0 || e > li) {
            status_line_bold("Invalid range");
            goto ret;
        }
        q = r = find_line(e);
        if (!GOT_RANGE) {
            // if there is only one addr, then it's the line
            // number of the single line the user wants.
            // Reset the end pointer to the end of that line.
            r = end_line(q);
            li = 1;
        } else {
            // we were given two addrs.  change the
            // start pointer to the addr given by user.
            if (b < 0 || b > li || b > e) {
                status_line_bold("Invalid range");
                goto ret;
            }
            q = find_line(b); // what line is #b
            r = end_line(r);
            li = e - b + 1;
        }
    }
    // ------------ now look for the command ------------
    i = strlen(cmd);
    if (i == 0) { // :123CR goto line #123
        if (e >= 0) {
            dot = find_line(e); // what line is #e
            dot_skip_over_ws();
        }
    }
    else if (cmd[0] == '=' && !cmd[1]) { // where is the address
        if (!GOT_ADDRESS) {              // no addr given- use defaults
            e = count_lines(text, dot);
        }
        status_line("%d", e);
    } else if (strncmp(cmd, "delete", i) == 0) { // delete lines
        if (!GOT_ADDRESS) {                      // no addr given- use defaults
            q = begin_line(dot);                 // assume .,. for the range
            r = end_line(dot);
        }
        dot = yank_delete(q, r, WHOLE, YANKDEL, ALLOW_UNDO); // save, then delete lines
        dot_skip_over_ws();
    } else if (strncmp(cmd, "edit", i) == 0) { // Edit a file
        int size;

        // don't edit, if the current file has been modified
        if (modified_count && !useforce) {
            status_line_bold("No write since last change (:%s! overrides)", cmd);
            goto ret;
        }
        if (args[0]) {
            // the user supplied a file name
            fn = exp = full_path(args);
        } else if (current_filename == NULL) {
            // no user file name, no current name- punt
            status_line_bold("No current filename");
            goto ret;
        }

        size = init_text_buffer(fn);

        if (Ureg >= 0 && Ureg < 28) {
            free(reg[Ureg]); //   free orig line reg- for 'U'
            reg[Ureg] = NULL;
        }
        /*if (YDreg < 28) - always true*/ {
            free(reg[YDreg]); //   free default yank/delete register
            reg[YDreg] = NULL;
        }
        // how many lines in text[]?
        li = count_lines(text, end - 1);
        status_line("'%s'%s"
                        " %uL, %uC",
                    fn, (size < 0 ? " [New file]" : ""),
                        li,
                    (int)(end - text));
    } else if (strncmp(cmd, "file", i) == 0) { // what File is this
        if (e >= 0) {
            status_line_bold("No address allowed on this command");
            goto ret;
        }
        if (args[0]) {
            // user wants a new filename
            exp = full_path(args);
            update_filename(exp);
        } else {
            // user wants file status info
            last_status_cksum = 0; // force status update
        }
    } else if (strncmp(cmd, "list", i) == 0) { // literal print line
        if (!GOT_ADDRESS) {                    // no addr given- use defaults
            q = begin_line(dot);               // assume .,. for the range
            r = end_line(dot);
        }
        go_bottom_and_clear_to_eol();
        puts("\r");
        for (; q <= r; q++) {
            int c_is_no_print;

            c = *q;
            c_is_no_print = (c & 0x80) && !is_asciionly(c);
            if (c_is_no_print) {
                c = '.';
                standout_start();
            }
            if (c == '\n') {
                puts_no_eol("$\r");
            } else if (c < ' ' || c == 127) {
                putchar_raw('^');
                if (c == 127)
                    c = '?';
                else
                    c += '@';
            }
            putchar_raw(c);
            if (c_is_no_print)
                standout_end();
        }
        Hit_Return();
    } else if (strncmp(cmd, "quit", i) == 0    // quit
               || strncmp(cmd, "next", i) == 0 // edit next file
               || strncmp(cmd, "prev", i) == 0 // edit previous file
    ) {
        int n;
        if (useforce) {
            editing = 0;
            goto ret;
        }
        // don't exit if the file been modified
        if (modified_count) {
            status_line_bold("No write since last change (:%s! overrides)", cmd);
            goto ret;
        }
        // are there other file to edit
        n = argc - optind - 1;
        if (*cmd == 'q' && n > 0) {
            status_line_bold("%u more file(s) to edit", n);
            goto ret;
        }
        if (*cmd == 'n' && n <= 0) {
            status_line_bold("No more files to edit");
            goto ret;
        }
        if (*cmd == 'p') {
            // are there previous files to edit
            if (optind < 2) {
                status_line_bold("No previous files to edit");
                goto ret;
            }
            optind -= 2;
        }
        editing = 0;
    } else if (strncmp(cmd, "read", i) == 0) { // read file into text[]
        int size, num;

        if (args[0]) {
            // the user supplied a file name
            fn = exp = full_path(args);
            if (exp == NULL)
                goto ret;
            init_filename(fn);
        } else if (current_filename == NULL) {
            // no user file name, no current name- punt
            status_line_bold("No current filename");
            goto ret;
        }
        if (e == 0) { // user said ":0r foo"
            q = text;
        } else { // read after given line or current line if none given
            q = next_line(GOT_ADDRESS ? find_line(e) : dot);
            // read after last line
            if (q == end - 1)
                ++q;
        }
        num = count_lines(text, q);
        if (q == end)
            num++;
        { // dance around potentially-reallocated text[]
            uintptr_t ofs = q - text;
            size = file_insert(fn, q, 0);
            q = text + ofs;
        }
        if (size < 0)
            goto ret; // nothing was inserted
        // how many lines in text[]?
        li = count_lines(q, q + size - 1);
        status_line("'%s' %uL, %uC", fn, li, size);
        dot = find_line(num);
    } else if (strncmp(cmd, "rewind", i) == 0) { // rewind cmd line args
        if (modified_count && !useforce) {
            status_line_bold("No write since last change (:%s! overrides)", cmd);
        } else {
            // reset the filenames to edit
            optind = 0; // start from 0th file
            editing = 0;
        }
    } else if (strncmp(cmd, "set", i) == 0) { // set or clear features
        char *argp, *argn, oldch;
        // only blank is regarded as args delimiter. What about tab '\t'?
        if (!args[0] || strcmp(args, "all") == 0) {
            // print out values of all options
            status_line_bold("%sautoindent "
                             "%sexpandtab "
                             "%sflash "
                             "%signorecase "
                             "%sshowmatch "
                             "tabstop=%u",
                             autoindent ? "" : "no", expandtab ? "" : "no", err_method ? "" : "no",
                             ignorecase ? "" : "no", showmatch ? "" : "no", tabstop);
            goto ret;
        }
        argp = args;
        while (*argp) {
            i = 0;
            if (argp[0] == 'n' && argp[1] == 'o') // "noXXX"
                i = 2;
            argn = skip_non_whitespace(argp);
            oldch = *argn;
            *argn = '\0';
            setops(argp, i);
            *argn = oldch;
            argp = skip_whitespace(argn);
        }
    } else if (cmd[0] == 's') { // substitute a pattern with a replacement pattern
        char *F, *R, *flags;
        size_t len_F, len_R;
        int gflag = 0; // global replace flag
        int subs = 0;  // number of substitutions
        int last_line = 0, lines = 0;

        // F points to the "find" pattern
        // R points to the "replace" pattern
        // replace the cmd line delimiters "/" with NULs
        c = buf[1];                 // what is the delimiter
        F = buf + 2;                // start of "find"
        R = strchr_backslash(F, c); // middle delimiter
        if (!R)
            goto colon_s_fail;
        len_F = R - F;
        *R++ = '\0'; // terminate "find"
        flags = strchr_backslash(R, c);
        if (flags) {
            *flags++ = '\0'; // terminate "replace"
            gflag = *flags;
        }

        if (len_F) { // save "find" as last search pattern
            free(last_search_pattern);
            last_search_pattern = strdup(F - 1);
            last_search_pattern[0] = '/';
        } else if (last_search_pattern[1] == '\0') {
            status_line_bold("No previous search");
            goto ret;
        } else {
            F = last_search_pattern + 1;
            len_F = strlen(F);
        }

        if (!GOT_ADDRESS) {      // no addr given
            q = begin_line(dot); // start with cur line
            r = end_line(dot);
            b = e = count_lines(text, q); // cur line number
        } else if (!GOT_RANGE) {          // one addr given
            b = e;
        }

        len_R = strlen(R);

        for (i = b; i <= e; i++) { // so, :20,23 s \0 find \0 replace \0
            char* ls = q;          // orig line start
            char* found;
        vc4:
            found = char_search(q, F, (FORWARD << 1) | LIMITED); // search cur line only for "find"
            if (found) {
                uintptr_t bias;
                // we found the "find" pattern - delete it
                // For undo support, the first item should not be chained
                // This needs to be handled differently depending on
                // whether or not regex support is enabled.
#define TEST_LEN_F 1 // len_F is never zero
#define TEST_UNDO1 subs
#define TEST_UNDO2 1
                if (TEST_LEN_F) // match can be empty, no delete needed
                    text_hole_delete(found, found + len_F - 1,
                                     TEST_UNDO1 ? ALLOW_UNDO_CHAIN : ALLOW_UNDO);
                if (len_R != 0) { // insert the "replace" pattern, if required
                    bias = string_insert(found, R, TEST_UNDO2 ? ALLOW_UNDO_CHAIN : ALLOW_UNDO);
                    found += bias;
                    ls += bias;
                    // q += bias; - recalculated anyway
                }
                if (TEST_LEN_F || len_R != 0) {
                    dot = ls;
                    subs++;
                    if (last_line != i) {
                        last_line = i;
                        ++lines;
                    }
                }
                // check for "global"  :s/foo/bar/g
                if (gflag == 'g') {
                    if ((found + len_R) < end_line(ls)) {
                        q = found + len_R;
                        goto vc4; // don't let q move past cur line
                    }
                }
            }
            q = next_line(ls);
        }
        if (subs == 0) {
            status_line_bold("No match");
        } else {
            dot_skip_over_ws();
            if (subs > 1)
                status_line("%d substitutions on %d lines", subs, lines);
        }
    } else if (strncmp(cmd, "version", i) == 0) { // show software version
        status_line(BB_VER);
    } else if (strncmp(cmd, "write", i) == 0 // write text to file
               || strcmp(cmd, "wq") == 0 || strcmp(cmd, "wn") == 0 || (cmd[0] == 'x' && !cmd[1])) {
        int size;
        // int forced = false;

        // is there a file name to write to?
        if (args[0]) {
            struct lfs_info statbuf;

            exp = full_path(args);
            if (!useforce && (fn == NULL || strcmp(fn, exp) != 0) && fs_stat(exp, &statbuf) >= 0) {
                status_line_bold("File exists (:w! overrides)");
                goto ret;
            }
            fn = exp;
            init_filename(fn);
        }
        // if (useforce) {
        // if "fn" is not write-able, chmod u+w
        // sprintf(syscmd, "chmod u+w %s", fn);
        // system(syscmd);
        // forced = true;
        //}
        if (modified_count != 0 || cmd[0] != 'x') {
            size = r - q + 1;
            l = file_write(fn, q, r);
        } else {
            size = 0;
            l = 0;
        }
        // if (useforce && forced) {
        // chmod u-w
        // sprintf(syscmd, "chmod u-w %s", fn);
        // system(syscmd);
        // forced = false;
        //}
        if (l < 0) {
            if (l == -1)
                status_line_bold_errno(fn);
        } else {
            // how many lines written
            li = count_lines(q, q + l - 1);
            status_line("'%s' %uL, %uC", fn, li, l);
            if (l == size) {
                if (q == text && q + l == end) {
                    modified_count = 0;
                    last_modified_count = -1;
                }
                if (cmd[1] == 'n') {
                    editing = 0;
                } else if (cmd[0] == 'x' || cmd[1] == 'q') {
                    // are there other files to edit?
                    int n = argc - optind - 1;
                    if (n > 0) {
                        if (useforce) {
                            // force end of argv list
                            optind = argc;
                        } else {
                            status_line_bold("%u more file(s) to edit", n);
                            goto ret;
                        }
                    }
                    editing = 0;
                }
            }
        }
    } else if (strncmp(cmd, "yank", i) == 0) { // yank lines
        if (!GOT_ADDRESS) {                    // no addr given- use defaults
            q = begin_line(dot);               // assume .,. for the range
            r = end_line(dot);
        }
        text_yank(q, r, YDreg, WHOLE);
        li = count_lines(q, r);
        status_line("Yank %d lines (%d chars) into [%c]", li, strlen(reg[YDreg]), what_reg());
    } else {
        // cmd unknown
        not_implemented(cmd);
    }
ret:
    dot = bound_dot(dot); // make sure "dot" is valid
    return;
colon_s_fail:
    status_line(":s expression missing delimiters");
}

//----- Char Routines --------------------------------------------
// Chars that are part of a word-
//    0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
// Chars that are Not part of a word (stoppers)
//    !"#$%&'()*+,-./:;<=>?@[\]^`{|}~
// Chars that are WhiteSpace
//    TAB NEWLINE VT FF RETURN SPACE
// DO NOT COUNT NEWLINE AS WHITESPACE

static int st_test(char* p, int type, int dir, char* tested) {
    char c, c0, ci;
    int test, inc;

    inc = dir;
    c = c0 = p[0];
    ci = p[inc];
    test = 0;

    if (type == S_BEFORE_WS) {
        c = ci;
        test = (!isspace(c) || c == '\n');
    }
    if (type == S_TO_WS) {
        c = c0;
        test = (!isspace(c) || c == '\n');
    }
    if (type == S_OVER_WS) {
        c = c0;
        test = isspace(c);
    }
    if (type == S_END_PUNCT) {
        c = ci;
        test = ispunct(c);
    }
    if (type == S_END_ALNUM) {
        c = ci;
        test = (isalnum(c) || c == '_');
    }
    *tested = c;
    return test;
}

static char* skip_thing(char* p, int linecnt, int dir, int type) {
    char c;

    while (st_test(p, type, dir, &c)) {
        // make sure we limit search to correct number of lines
        if (c == '\n' && --linecnt < 1)
            break;
        if (dir >= 0 && p >= end - 1)
            break;
        if (dir < 0 && p <= text)
            break;
        p += dir; // move to next char
    }
    return p;
}

static void do_cmd(int c);

static int at_eof(const char* s) {
    // does 's' point to end of file, even with no terminating newline?
    return ((s == end - 2 && s[1] == '\n') || s == end - 1);
}

static int find_range(char** start, char** stop, int cmd) {
    char *p, *q, *t;
    int buftype = -1;
    int c;

    p = q = dot;

    if (cmd == 'Y') {
        c = 'y';
    } else
    {
        c = get_motion_char();
    }

    if ((cmd == 'Y' || cmd == c) && strchr("cdy><", c)) {
        // these cmds operate on whole lines
        buftype = WHOLE;
        if (--cmdcnt > 0) {
            do_cmd('j');
            if (cmd_error)
                buftype = -1;
        }
    } else if (strchr("^%$0bBeEfFtThnN/?|{}\b\177", c)) {
        // Most operate on char positions within a line.  Of those that
        // don't '%' needs no special treatment, search commands are
        // marked as MULTI and  "{}" are handled below.
        buftype = strchr("nN/?", c) ? MULTI : PARTIAL;
        do_cmd(c);    // execute movement cmd
        if (p == dot) // no movement is an error
            buftype = -1;
    } else if (strchr("wW", c)) {
        buftype = MULTI;
        do_cmd(c); // execute movement cmd
        // step back one char, but not if we're at end of file,
        // or if we are at EOF and search was for 'w' and we're at
        // the start of a 'W' word.
        if (dot > p && (!at_eof(dot) || (c == 'w' && ispunct(*dot))))
            dot--;
        t = dot;
        // don't include trailing WS as part of word
        while (dot > p && isspace(*dot)) {
            if (*dot-- == '\n')
                t = dot;
        }
        // for non-change operations WS after NL is not part of word
        if (cmd != 'c' && dot != t && *dot != '\n')
            dot = t;
    } else if (strchr("GHL+-gjk'\r\n", c)) {
        // these operate on whole lines
        buftype = WHOLE;
        do_cmd(c); // execute movement cmd
        if (cmd_error)
            buftype = -1;
    } else if (c == ' ' || c == 'l') {
        // forward motion by character
        int tmpcnt = (cmdcnt ?: 1);
        buftype = PARTIAL;
        do_cmd(c); // execute movement cmd
        // exclude last char unless range isn't what we expected
        // this indicates we've hit EOL
        if (tmpcnt == dot - p)
            dot--;
    }

    if (buftype == -1) {
        if (c != 27)
            indicate_error();
        return buftype;
    }

    q = dot;
    if (q < p) {
        t = q;
        q = p;
        p = t;
    }

    // movements which don't include end of range
    if (q > p) {
        if (strchr("^0bBFThnN/?|\b\177", c)) {
            q--;
        } else if (strchr("{}", c)) {
            buftype = (p == begin_line(p) && (*q == '\n' || at_eof(q))) ? WHOLE : MULTI;
            if (!at_eof(q)) {
                q--;
                if (q > p && p != begin_line(p))
                    q--;
            }
        }
    }

    *start = p;
    *stop = q;
    return buftype;
}

//---------------------------------------------------------------------
//----- the Ascii Chart -----------------------------------------------
//  00 nul   01 soh   02 stx   03 etx   04 eot   05 enq   06 ack   07 bel
//  08 bs    09 ht    0a nl    0b vt    0c np    0d cr    0e so    0f si
//  10 dle   11 dc1   12 dc2   13 dc3   14 dc4   15 nak   16 syn   17 etb
//  18 can   19 em    1a sub   1b esc   1c fs    1d gs    1e rs    1f us
//  20 sp    21 !     22 "     23 #     24 $     25 %     26 &     27 '
//  28 (     29 )     2a *     2b +     2c ,     2d -     2e .     2f /
//  30 0     31 1     32 2     33 3     34 4     35 5     36 6     37 7
//  38 8     39 9     3a :     3b ;     3c <     3d =     3e >     3f ?
//  40 @     41 A     42 B     43 C     44 D     45 E     46 F     47 G
//  48 H     49 I     4a J     4b K     4c L     4d M     4e N     4f O
//  50 P     51 Q     52 R     53 S     54 T     55 U     56 V     57 W
//  58 X     59 Y     5a Z     5b [     5c \     5d ]     5e ^     5f _
//  60 `     61 a     62 b     63 c     64 d     65 e     66 f     67 g
//  68 h     69 i     6a j     6b k     6c l     6d m     6e n     6f o
//  70 p     71 q     72 r     73 s     74 t     75 u     76 v     77 w
//  78 x     79 y     7a z     7b {     7c |     7d }     7e ~     7f del
//---------------------------------------------------------------------

//----- Execute a Vi Command -----------------------------------
static void do_cmd(int c) {
    char *p, *q, *save_dot;
    char buf[12];
    int dir;
    int cnt, i, j;
    int c1;
    char* orig_dot = dot;
    int allow_undo = ALLOW_UNDO;
    int undo_del = UNDO_DEL;

    //    c1 = c; // quiet the compiler
    //    cnt = yf = 0; // quiet the compiler
    //    p = q = save_dot = buf; // quiet the compiler
    memset(buf, 0, sizeof(buf));
    keep_index = false;
    cmd_error = false;

    show_status_line();

    // if this is a cursor key, skip these checks
    switch (c) {
    case KEYCODE_UP:
    case KEYCODE_DOWN:
    case KEYCODE_LEFT:
    case KEYCODE_RIGHT:
    case KEYCODE_HOME:
    case KEYCODE_END:
    case KEYCODE_PAGEUP:
    case KEYCODE_PAGEDOWN:
    case KEYCODE_DELETE:
        goto key_cmd_mode;
    }

    if (cmd_mode == 2) {
        //  flip-flop Insert/Replace mode
        if (c == KEYCODE_INSERT)
            goto dc_i;
        // we are 'R'eplacing the current *dot with new char
        if (*dot == '\n') {
            // don't Replace past E-o-l
            cmd_mode = 1; // convert to insert
            undo_queue_commit();
        } else {
            if (1 <= c || is_asciionly(c)) {
                if (c != 27)
                    dot = yank_delete(dot, dot, PARTIAL, YANKDEL, ALLOW_UNDO); // delete char
                dot = char_insert(dot, c, ALLOW_UNDO_CHAIN);                   // insert new char
            }
            goto dc1;
        }
    }
    if (cmd_mode == 1) {
        // hitting "Insert" twice means "R" replace mode
        if (c == KEYCODE_INSERT)
            goto dc5;
        // insert the char c at "dot"
        if (1 <= c || is_asciionly(c)) {
            dot = char_insert(dot, c, ALLOW_UNDO_QUEUED);
        }
        goto dc1;
    }

key_cmd_mode:
    switch (c) {
    default: // unrecognized command
        buf[0] = c;
        buf[1] = '\0';
        not_implemented(buf);
        end_cmd_q(); // stop adding to q
    case 0x00:       // nul- ignore
        break;
    case 2:              // ctrl-B  scroll up   full screen
    case KEYCODE_PAGEUP: // Cursor Key Page Up
        dot_scroll(rows - 2, -1);
        break;
    case 4: // ctrl-D  scroll down half screen
        dot_scroll((rows - 2) / 2, 1);
        break;
    case 5: // ctrl-E  scroll down one line
        dot_scroll(1, 1);
        break;
    case 6:                // ctrl-F  scroll down full screen
    case KEYCODE_PAGEDOWN: // Cursor Key Page Down
        dot_scroll(rows - 2, 1);
        break;
    case 7:                    // ctrl-G  show current status
        last_status_cksum = 0; // force status update
        break;
    case 'h':          // h- move left
    case KEYCODE_LEFT: // cursor key Left
    case 8:            // ctrl-H- move left    (This may be ERASE char)
    case 0x7f:         // DEL- move left   (This may be ERASE char)
        do {
            dot_left();
        } while (--cmdcnt > 0);
        break;
    case 10:           // Newline ^J
    case 'j':          // j- goto next line, same col
    case KEYCODE_DOWN: // cursor key Down
    case 13:           // Carriage Return ^M
    case '+':          // +- goto next line
        q = dot;
        do {
            p = next_line(q);
            if (p == end_line(q)) {
                indicate_error();
                goto dc1;
            }
            q = p;
        } while (--cmdcnt > 0);
        dot = q;
        if (c == 13 || c == '+') {
            dot_skip_over_ws();
        } else {
            // try to stay in saved column
            dot = cindex == C_END ? end_line(dot) : move_to_col(dot, cindex);
            keep_index = true;
        }
        break;
    case 12:          // ctrl-L  force redraw whole screen
    case 18:          // ctrl-R  force redraw
        redraw(true); // this will redraw the entire display
        break;
    case 21: // ctrl-U  scroll up half screen
        dot_scroll((rows - 2) / 2, -1);
        break;
    case 25: // ctrl-Y  scroll up one line
        dot_scroll(1, -1);
        break;
    case 27: // esc
        if (cmd_mode == 0)
            indicate_error();
        cmd_mode = 0; // stop inserting
        undo_queue_commit();
        end_cmd_q();
        last_status_cksum = 0; // force status update
        break;
    case ' ':           // move right
    case 'l':           // move right
    case KEYCODE_RIGHT: // Cursor Key Right
        do {
            dot_right();
        } while (--cmdcnt > 0);
        break;
    case '"':                               // "- name a register to use for Delete/Yank
        c1 = (get_one_char() | 0x20) - 'a'; // | 0x20 is tolower()
        if ((uint32_t)c1 <= 25) {           // a-z?
            YDreg = c1;
        } else {
            indicate_error();
        }
        break;
    case '\'': // '- goto a specific mark
        c1 = (get_one_char() | 0x20);
        if ((uint32_t)(c1 - 'a') <= 25) { // a-z?
            c1 = (c1 - 'a');
            // get the b-o-l
            q = mark[c1];
            if (text <= q && q < end) {
                dot = q;
                dot_begin(); // go to B-o-l
                dot_skip_over_ws();
            } else {
                indicate_error();
            }
        } else if (c1 == '\'') {     // goto previous context
            dot = swap_context(dot); // swap current and previous context
            dot_begin();             // go to B-o-l
            dot_skip_over_ws();
            orig_dot = dot; // this doesn't update stored contexts
        } else {
            indicate_error();
        }
        break;
    case 'm': // m- Mark a line
        // this is really stupid.  If there are any inserts or deletes
        // between text[0] and dot then this mark will not point to the
        // correct location! It could be off by many lines!
        // Well..., at least its quick and dirty.
        c1 = (get_one_char() | 0x20) - 'a';
        if ((uint32_t)c1 <= 25) { // a-z?
            // remember the line
            mark[c1] = dot;
        } else {
            indicate_error();
        }
        break;
    case 'P': // P- Put register before
    case 'p': // p- put register after
        p = reg[YDreg];
        if (p == NULL) {
            status_line_bold("Nothing in register %c", what_reg());
            break;
        }
        cnt = 0;
        i = cmdcnt ?: 1;
        // are we putting whole lines or strings
        if (regtype[YDreg] == WHOLE) {
            if (c == 'P') {
                dot_begin(); // putting lines- Put above
            } else /* if ( c == 'p') */ {
                // are we putting after very last line?
                if (end_line(dot) == (end - 1)) {
                    dot = end; // force dot to end of text[]
                } else {
                    dot_next(); // next line, then put before
                }
            }
        } else {
            if (c == 'p')
                dot_right(); // move to right, can move to NL
            // how far to move cursor if register doesn't have a NL
            if (strchr(p, '\n') == NULL)
                cnt = i * strlen(p) - 1;
        }
        do {
            // dot is adjusted if text[] is reallocated so we don't have to
            string_insert(dot, p, allow_undo); // insert the string
            allow_undo = ALLOW_UNDO_CHAIN;
        } while (--cmdcnt > 0);
        dot += cnt;
        dot_skip_over_ws();
        yank_status("Put", p, i);
        end_cmd_q(); // stop adding to q
        break;
    case 'U': // U- Undo; replace current line with original version
        if (reg[Ureg] != NULL) {
            p = begin_line(dot);
            q = end_line(dot);
            p = text_hole_delete(p, q, ALLOW_UNDO);             // delete cur line
            p += string_insert(p, reg[Ureg], ALLOW_UNDO_CHAIN); // insert orig line
            dot = p;
            dot_skip_over_ws();
            yank_status("Undo", reg[Ureg], 1);
        }
        break;
    case 'u': // u- undo last operation
        undo_pop();
        break;
    case '$':         // $- goto end of line
    case KEYCODE_END: // Cursor Key End
        for (;;) {
            dot = end_line(dot);
            if (--cmdcnt <= 0)
                break;
            dot_next();
        }
        cindex = C_END;
        keep_index = true;
        break;
    case '%': // %- find matching char of pair () [] {}
        for (q = dot; q < end && *q != '\n'; q++) {
            if (strchr("()[]{}", *q) != NULL) {
                // we found half of a pair
                p = find_pair(q, *q);
                if (p == NULL) {
                    indicate_error();
                } else {
                    dot = p;
                }
                break;
            }
        }
        if (*q == '\n')
            indicate_error();
        break;
    case 'f':                              // f- forward to a user specified char
    case 'F':                              // F- backward to a user specified char
    case 't':                              // t- move to char prior to next x
    case 'T':                              // T- move to char after previous x
        last_search_char = get_one_char(); // get the search char
        last_search_cmd = c;
        // fall through
    case ';': // ;- look at rest of line for last search char
    case ',': // ,- repeat latest search in opposite direction
        dot_to_char(c != ',' ? last_search_cmd : last_search_cmd ^ 0x20);
        break;
    case '.': // .- repeat the last modifying command
        // Stuff the last_modifying_cmd back into stdin
        // and let it be re-executed.
        if (lmc_len != 0) {
            if (cmdcnt) // update saved count if current count is non-zero
                dotcnt = cmdcnt;
            last_modifying_cmd[lmc_len] = '\0';
            ioq = ioq_start = xvsnprintf("%u%s", dotcnt, last_modifying_cmd);
        }
        break;
    case 'N': // N- backward search for last pattern
        dir = last_search_pattern[0] == '/' ? BACK : FORWARD;
        goto dc4; // now search for pattern
        break;
    case '?': // ?- backward search for a pattern
    case '/': // /- forward search for a pattern
        buf[0] = c;
        buf[1] = '\0';
        q = get_input_line(buf); // get input line- use "status line"
        if (!q[0])               // user changed mind and erased the "/"-  do nothing
            break;
        if (!q[1]) { // if no pat re-use old pat
            if (last_search_pattern[0])
                last_search_pattern[0] = c;
        } else { // strlen(q) > 1: new pat- save it and find
            free(last_search_pattern);
            last_search_pattern = strdup(q);
        }
        // fall through
    case 'n': // n- repeat search for last pattern
        // search rest of text[] starting at next char
        // if search fails "dot" is unchanged
        dir = last_search_pattern[0] == '/' ? FORWARD : BACK;
    dc4:
        if (last_search_pattern[1] == '\0') {
            status_line_bold("No previous search");
            break;
        }
        do {
            q = char_search(dot + dir, last_search_pattern + 1, (dir << 1) | FULL);
            if (q != NULL) {
                dot = q; // good search, update "dot"
            } else {
                // no pattern found between "dot" and top/bottom of file
                // continue from other end of file
                const char* msg;
                q = char_search(dir == FORWARD ? text : end - 1, last_search_pattern + 1,
                                (dir << 1) | FULL);
                if (q != NULL) { // found something
                    dot = q;     // found new pattern- goto it
                    msg = "search hit %s, continuing at %s";
                } else {        // pattern is nowhere in file
                    cmdcnt = 0; // force exit from loop
                    msg = "Pattern not found";
                }
                if (dir == FORWARD)
                    status_line_bold(msg, "BOTTOM", "TOP");
                else
                    status_line_bold(msg, "TOP", "BOTTOM");
            }
        } while (--cmdcnt > 0);
        break;
    case '{': // {- move backward paragraph
    case '}': // }- move forward paragraph
        dir = c == '}' ? FORWARD : BACK;
        do {
            int skip = true; // initially skip consecutive empty lines
            while (dir == FORWARD ? dot < end - 1 : dot > text) {
                if (*dot == '\n' && dot[dir] == '\n') {
                    if (!skip) {
                        if (dir == FORWARD)
                            ++dot; // move to next blank line
                        goto dc2;
                    }
                } else {
                    skip = false;
                }
                dot += dir;
            }
            goto dc6; // end of file
        dc2:
            continue;
        } while (--cmdcnt > 0);
        break;
    case '0': // 0- goto beginning of line
    case '1': // 1-
    case '2': // 2-
    case '3': // 3-
    case '4': // 4-
    case '5': // 5-
    case '6': // 6-
    case '7': // 7-
    case '8': // 8-
    case '9': // 9-
        if (c == '0' && cmdcnt < 1) {
            dot_begin(); // this was a standalone zero
        } else {
            cmdcnt = cmdcnt * 10 + (c - '0'); // this 0 is part of a number
        }
        break;
    case ':':                    // :- the colon mode commands
        p = get_input_line(":"); // get input line- use "status line"
        colon(p);                // execute the command
        show_status_line();
        break;
    case '<':                         // <- Left  shift something
    case '>':                         // >- Right shift something
        cnt = count_lines(text, dot); // remember what line we are on
        if (find_range(&p, &q, c) == -1)
            goto dc6;
        i = count_lines(p, q); // # of lines we are shifting
        for (p = begin_line(p); i > 0; i--, p = next_line(p)) {
            if (c == '<') {
                // shift left- remove tab or tabstop spaces
                if (*p == '\t') {
                    // shrink buffer 1 char
                    text_hole_delete(p, p, allow_undo);
                } else if (*p == ' ') {
                    // we should be calculating columns, not just SPACE
                    for (j = 0; *p == ' ' && j < tabstop; j++) {
                        text_hole_delete(p, p, allow_undo);
                        allow_undo = ALLOW_UNDO_CHAIN;
                    }
                }
            } else if (/* c == '>' && */ p != end_line(p)) {
                // shift right -- add tab or tabstop spaces on non-empty lines
                char_insert(p, '\t', allow_undo);
            }
            allow_undo = ALLOW_UNDO_CHAIN;
        }
        dot = find_line(cnt); // what line were we on
        dot_skip_over_ws();
        end_cmd_q(); // stop adding to q
        break;
    case 'A':      // A- append at e-o-l
        dot_end(); // go to e-o-l
                   //**** fall through to ... 'a'
    case 'a':      // a- append after current char
        if (*dot != '\n')
            dot++;
        goto dc_i;
        break;
    case 'B': // B- back a blank-delimited Word
    case 'E': // E- end of a blank-delimited word
    case 'W': // W- forward a blank-delimited word
        dir = FORWARD;
        if (c == 'B')
            dir = BACK;
        do {
            if (c == 'W' || isspace(dot[dir])) {
                dot = skip_thing(dot, 1, dir, S_TO_WS);
                dot = skip_thing(dot, 2, dir, S_OVER_WS);
            }
            if (c != 'W')
                dot = skip_thing(dot, 1, dir, S_BEFORE_WS);
        } while (--cmdcnt > 0);
        break;
    case 'C': // C- Change to e-o-l
    case 'D': // D- delete to e-o-l
        save_dot = dot;
        dot = dollar_line(dot); // move to before NL
        // copy text into a register and delete
        dot = yank_delete(save_dot, dot, PARTIAL, YANKDEL, ALLOW_UNDO); // delete to e-o-l
        if (c == 'C')
            goto dc_i; // start inserting
        if (c == 'D')
            end_cmd_q(); // stop adding to q
        break;
    case 'g': // 'gg' goto a line number (vim) (default: very first line)
        c1 = get_one_char();
        if (c1 != 'g') {
            buf[0] = 'g';
            // c1 < 0 if the key was special. Try "g<up-arrow>"
            buf[1] = (c1 >= 0 ? c1 : '*');
            buf[2] = '\0';
            not_implemented(buf);
            cmd_error = true;
            break;
        }
        if (cmdcnt == 0)
            cmdcnt = 1;
        // fall through
    case 'G':          // G- goto to a line number (default= E-O-F)
        dot = end - 1; // assume E-O-F
        if (cmdcnt > 0) {
            dot = find_line(cmdcnt); // what line is #cmdcnt
        }
        dot_begin();
        dot_skip_over_ws();
        break;
    case 'H': // H- goto top line on screen
        dot = screenbegin;
        if (cmdcnt > (rows - 1)) {
            cmdcnt = (rows - 1);
        }
        while (--cmdcnt > 0) {
            dot_next();
        }
        dot_begin();
        dot_skip_over_ws();
        break;
    case 'I':        // I- insert before first non-blank
        dot_begin(); // 0
        dot_skip_over_ws();
        //**** fall through to ... 'i'
    case 'i':            // i- insert before current char
    case KEYCODE_INSERT: // Cursor Key Insert
    dc_i:
        cmd_mode = 1;        // start inserting
        undo_queue_commit(); // commit queue when cmd_mode changes
        break;
    case 'J': // J- join current and next lines together
        do {
            dot_end();           // move to NL
            if (dot < end - 1) { // make sure not last char in text[]
                undo_push(dot, 1, UNDO_DEL);
                *dot++ = ' '; // replace NL with space
                undo_push((dot - 1), 1, UNDO_INS_CHAIN);
                while (isblank(*dot)) { // delete leading WS
                    text_hole_delete(dot, dot, ALLOW_UNDO_CHAIN);
                }
            }
        } while (--cmdcnt > 0);
        end_cmd_q(); // stop adding to q
        break;
    case 'L': // L- goto bottom line on screen
        dot = end_screen();
        if (cmdcnt > (rows - 1)) {
            cmdcnt = (rows - 1);
        }
        while (--cmdcnt > 0) {
            dot_prev();
        }
        dot_begin();
        dot_skip_over_ws();
        break;
    case 'M': // M- goto middle line on screen
        dot = screenbegin;
        for (cnt = 0; cnt < (rows - 1) / 2; cnt++)
            dot = next_line(dot);
        dot_skip_over_ws();
        break;
    case 'O': // O- open an empty line above
        dot_begin();
        indentcol = -1;
        goto dc3;
    case 'o': // o- open an empty line below
        dot_end();
    dc3:
        dot = char_insert(dot, '\n', ALLOW_UNDO);
        if (c == 'O' && !autoindent) {
            // done in char_insert() for 'O'+autoindent
            dot_prev();
        }
        goto dc_i;
        break;
    case 'R': // R- continuous Replace char
    dc5:
        cmd_mode = 2;
        undo_queue_commit();
        break;
    case KEYCODE_DELETE:
        if (dot < end - 1)
            dot = yank_delete(dot, dot, PARTIAL, YANKDEL, ALLOW_UNDO);
        break;
    case 'X': // X- delete char before dot
    case 'x': // x- delete the current char
    case 's': // s- substitute the current char
        dir = 0;
        if (c == 'X')
            dir = -1;
        do {
            if (dot[dir] != '\n') {
                if (c == 'X')
                    dot--;                                                 // delete prev char
                dot = yank_delete(dot, dot, PARTIAL, YANKDEL, allow_undo); // delete char
                allow_undo = ALLOW_UNDO_CHAIN;
            }
        } while (--cmdcnt > 0);
        end_cmd_q(); // stop adding to q
        if (c == 's')
            goto dc_i; // start inserting
        break;
    case 'Z': // Z- if modified, {write}; exit
        // ZZ means to save file (if necessary), then exit
        c1 = get_one_char();
        if (c1 != 'Z') {
            indicate_error();
            break;
        }
        if (modified_count) {
            cnt = file_write(current_filename, text, end - 1);
            if (cnt < 0) {
                if (cnt == -1)
                    status_line_bold("Write error: %s", strerror(errno));
            } else if (cnt == (end - 1 - text + 1)) {
                editing = 0;
            }
        } else {
            editing = 0;
        }
        // are there other files to edit?
        j = argc - optind - 1;
        if (editing == 0 && j > 0) {
            editing = 1;
            modified_count = 0;
            last_modified_count = -1;
            status_line_bold("%u more file(s) to edit", j);
        }
        break;
    case '^': // ^- move to first non-blank on line
        dot_begin();
        dot_skip_over_ws();
        break;
    case 'b': // b- back a word
    case 'e': // e- end of word
        dir = FORWARD;
        if (c == 'b')
            dir = BACK;
        do {
            if ((dot + dir) < text || (dot + dir) > end - 1)
                break;
            dot += dir;
            if (isspace(*dot)) {
                dot = skip_thing(dot, (c == 'e') ? 2 : 1, dir, S_OVER_WS);
            }
            if (isalnum(*dot) || *dot == '_') {
                dot = skip_thing(dot, 1, dir, S_END_ALNUM);
            } else if (ispunct(*dot)) {
                dot = skip_thing(dot, 1, dir, S_END_PUNCT);
            }
        } while (--cmdcnt > 0);
        break;
    case 'c': // c- change something
    case 'd': // d- delete something
    case 'y': // y- yank   something
    case 'Y': // Y- Yank a line
    {
        int yf = YANKDEL; // assume either "c" or "d"
        int buftype;
        char* savereg = reg[YDreg];
        if (c == 'y' || c == 'Y')
            yf = YANKONLY;
        // determine range, and whether it spans lines
        buftype = find_range(&p, &q, c);
        if (buftype == -1) // invalid range
            goto dc6;
        if (buftype == WHOLE) {
            save_dot = p; // final cursor position is start of range
            p = begin_line(p);
            q = end_line(q);
        }
        dot = yank_delete(p, q, buftype, yf, ALLOW_UNDO); // delete word
        if (buftype == WHOLE) {
            if (c == 'c') {
                dot = char_insert(dot, '\n', ALLOW_UNDO_CHAIN);
                // on the last line of file don't move to prev line
                if (dot != (end - 1)) {
                    dot_prev();
                }
            } else if (c == 'd') {
                dot_begin();
                dot_skip_over_ws();
            } else {
                dot = save_dot;
            }
        }
        // if CHANGING, not deleting, start inserting after the delete
        if (c == 'c') {
            goto dc_i; // start inserting
        }
        // only update status if a yank has actually happened
        if (reg[YDreg] != savereg)
            yank_status(c == 'd' ? "Delete" : "Yank", reg[YDreg], 1);
    dc6:
        end_cmd_q(); // stop adding to q
        break;
    }
    case 'k':        // k- goto prev line, same col
    case KEYCODE_UP: // cursor key Up
    case '-':        // -- goto prev line
        q = dot;
        do {
            p = prev_line(q);
            if (p == begin_line(q)) {
                indicate_error();
                goto dc1;
            }
            q = p;
        } while (--cmdcnt > 0);
        dot = q;
        if (c == '-') {
            dot_skip_over_ws();
        } else {
            // try to stay in saved column
            dot = cindex == C_END ? end_line(dot) : move_to_col(dot, cindex);
            keep_index = true;
        }
        break;
    case 'r':                // r- replace the current char with user input
        c1 = get_one_char(); // get the replacement char
        if (c1 != 27) {
            if (end_line(dot) - dot < (cmdcnt ?: 1)) {
                indicate_error();
                goto dc6;
            }
            do {
                dot = text_hole_delete(dot, dot, allow_undo);
                allow_undo = ALLOW_UNDO_CHAIN;
                dot = char_insert(dot, c1, allow_undo);
            } while (--cmdcnt > 0);
            dot_left();
        }
        end_cmd_q(); // stop adding to q
        break;
    case 'w': // w- forward a word
        do {
            if (isalnum(*dot) || *dot == '_') { // we are on ALNUM
                dot = skip_thing(dot, 1, FORWARD, S_END_ALNUM);
            } else if (ispunct(*dot)) { // we are on PUNCT
                dot = skip_thing(dot, 1, FORWARD, S_END_PUNCT);
            }
            if (dot < end - 1)
                dot++; // move over word
            if (isspace(*dot)) {
                dot = skip_thing(dot, 2, FORWARD, S_OVER_WS);
            }
        } while (--cmdcnt > 0);
        break;
    case 'z':                // z-
        c1 = get_one_char(); // get the replacement char
        cnt = 0;
        if (c1 == '.')
            cnt = (rows - 2) / 2; // put dot at center
        if (c1 == '-')
            cnt = rows - 2;            // put dot at bottom
        screenbegin = begin_line(dot); // start dot at top
        dot_scroll(cnt, -1);
        break;
    case '|':                               // |- move to column "cmdcnt"
        dot = move_to_col(dot, cmdcnt - 1); // try to move to column
        break;
    case '~': // ~- flip the case of letters   a-z -> A-Z
        do {
            if (isalpha(*dot)) {
                undo_push(dot, 1, undo_del);
                *dot = islower(*dot) ? toupper(*dot) : tolower(*dot);
                undo_push(dot, 1, UNDO_INS_CHAIN);
                undo_del = UNDO_DEL_CHAIN;
            }
            dot_right();
        } while (--cmdcnt > 0);
        end_cmd_q(); // stop adding to q
        break;
        //----- The Cursor and Function Keys -----------------------------
    case KEYCODE_HOME: // Cursor Key Home
        dot_begin();
        break;
        // The Fn keys could point to do_macro which could translate them
    }

dc1:
    // if text[] just became empty, add back an empty line
    if (end == text) {
        char_insert(text, '\n', NO_UNDO); // start empty buf with dummy line
        dot = text;
    }
    // it is OK for dot to exactly equal to end, otherwise check dot validity
    if (dot != end) {
        dot = bound_dot(dot); // make sure "dot" is valid
    }
    if (dot != orig_dot)
        check_context(c); // update the current context

    if (!isdigit(c))
        cmdcnt = 0; // cmd was not a number, reset cmdcnt
    cnt = dot - begin_line(dot);
    // Try to stay off of the Newline
    if (*dot == '\n' && cnt > 0 && cmd_mode == 0)
        dot--;
}

static void run_cmds(char* p) {
    while (p) {
        char* q = p;
        p = strchr(q, '\n');
        if (p)
            while (*p == '\n')
                *p++ = '\0';
        if (strlen(q) < MAX_INPUT_LEN)
            colon(q);
    }
}

static void edit_file(char* fn) {
    int c;

    editing = 1; // 0 = exit, 1 = one file, 2 = multiple files
    new_screen(rows, columns); // get memory for virtual screen
    init_text_buffer(fn);

    YDreg = 26;                 // default Yank/Delete reg
                                //    Ureg = 27; - const        // hold orig line for "U" cmd
    mark[26] = mark[27] = text; // init "previous context"

    crow = 0;
    ccol = 0;

    cmd_mode = 0; // 0=command  1=insert  2='R'eplace
    cmdcnt = 0;
    offset = 0; // no horizontal offset
    c = '\0';
    if (ioq_start)
        free(ioq_start);
    ioq_start = NULL;
    adding2q = 0;

    redraw(false); // dont force every col re-draw
    //------This is the main Vi cmd handling loop -----------------------
    while (editing > 0) {
        c = get_one_char(); // get a cmd from user
        // save a copy of the current line- for the 'U" command
        if (begin_line(dot) != edit_file_cur_line) {
            edit_file_cur_line = begin_line(dot);
            text_yank(begin_line(dot), end_line(dot), Ureg, PARTIAL);
        }
        // If c is a command that changes text[],
        // (re)start remembering the input for the "." command.
        if (!adding2q && ioq_start == NULL && cmd_mode == 0 // command mode
            && c > '\0'                                     // exclude NUL and non-ASCII chars
            && c < 0x7f                                     // (Unicode and such)
            && strchr(modifying_cmds, c)) {
            start_new_cmd_q(c);
        }
        do_cmd(c); // execute the user command

        // poll to see if there is input already waiting. if we are
        // not able to display output fast enough to keep up, skip
        // the display update until we catch up with input.
        if (!readbuffer[0] && sleep(0) == 0) {
            // no input pending - so update output
            refresh(false);
            show_status_line();
        }
    }
    //-------------------------------------------------------------------

    go_bottom_and_clear_to_eol();
}

#define VI_OPTSTR "c:*"

static void* xmalloc_open_read_close(const char* filename) {
    lfs_file_t fd;

    if (fs_file_open(&fd, filename, LFS_O_RDONLY) < 0)
        return NULL;

    int l = fs_file_size(&fd);
    char* buf = malloc(l + 1);
    if (buf == NULL) {
        fs_file_close(&fd);
        return NULL;
    }
    fs_file_read(&fd, buf, l);
    fs_file_close(&fd);
    buf[l] = 0;
    return buf;
}

int vi(int x, int y, int ac, char* argv[]) {
    int opts;
    argc = ac;

    memset(&G, 0, sizeof G);
    rows = y;
    columns = x;
    last_modified_count = -1;
    /* "" but has space for 2 chars: */
    last_search_pattern = zalloc(2);
    tabstop = 8;

    // undo_stack_tail = NULL; - already is
    undo_queue_state = UNDO_EMPTY;
    // undo_q = 0; - already is

    if (setjmp(die_jmp))
        goto shell;

    {
        char* cmds = NULL;
        char* exrc = "/.exrc";
        struct lfs_info st;

        if (fs_stat(exrc, &st) >= 0)
            cmds = xmalloc_open_read_close(exrc);

        if (cmds) {
            init_text_buffer(NULL);
            run_cmds(cmds);
            free(cmds);
        }
    }
    // "Save cursor, use alternate screen buffer, clear screen"
    puts_no_eol(ESC "[?1049h");
    fflush(stdout);
    // This is the main file handling loop
    if (argc == 0)
        argc++;
    for (optind = 0; optind < argc; optind++)
        edit_file(full_path(argv[optind])); // might be NULL on 1st iteration
shell:
    if (text)
        free(text);
    if (screen)
        free(screen);
    if (last_search_pattern)
        free(last_search_pattern);
    // "Use normal screen buffer, restore cursor"
    puts_no_eol(ESC "[?1049l");
    return 0;
}
