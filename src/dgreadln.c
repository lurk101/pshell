/*  Fido's own command-line history and tab completion
    Written 2022 by Eric Olson */

#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgreadln.h"
#include "fs.h"

extern char* full_path(const char* name);

static int dgleft() {
    int dgpush = getchar_timeout_us(0);
    if (dgpush != PICO_ERROR_TIMEOUT) {
        ungetc(dgpush, stdin);
        return 1;
    }
    return 0;
}

static int dgputs(char* s) {
    int n = 0;
    while (s[n])
        if (putchar(s[n++]) < 0)
            return -1;
    return n;
}

static int cookputc(int c) {
    if (c < '\040' || c >= '\177')
        return putchar('^');
    return putchar(c);
}

#define DOGSIZE 33
#define CMDFULL (128 - DOGSIZE - 8)
#define HSTSIZE (3 * CMDFULL)

static char history[HSTSIZE], escbuf[8], namebuf[DOGSIZE + 1];
static char *cmdline, *dgprom;
static int twotabs, hista, histb, esceb, cmdlb, cmdli, dgmnt;

static const char* esckeys[] = {
    "\b",        // 1 backspace
    "\177",      // 2 rubout
    "\025",      // 3 erase line
    "\033[A",    // 4 cursor up
    "\033[B",    // 5 cursor down
    "\033[C",    // 6 cursor right
    "\033[D",    // 7 cursor left
    "\033[H",    // 8 beginning
    "\001",      // 9 beginning2
    "\033[F",    // 10 end
    "\005",      // 11 end2
    "\033[5~",   // 12 back word
    "\033[1;5D", // 13 back word2
    "\033[6~",   // 14 next word
    "\033[1;5C", // 15 next word2
    "\033[2~",   // 16 insert space
    "\013"       // 17 erase EOL
};

static int dgscmp(const char* p, const char* q, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (p[i] != q[i] || !p[i] || !q[i])
            return i;
    }
    return n;
}

static char* findcmd() {
    int i, l;
    l = cmdli;
    int rmin = 0;
    namebuf[0] = 0;
    int c = 0;
    for (i = 0; cmd_table[i].name; i++) {
        const char* cmdname = cmd_table[i].name;
        if (dgscmp(&cmdline[0], cmdname, l) == l) {
            if (c++) {
                int r = dgscmp(&cmdname[l], namebuf, DOGSIZE);
                if (r < rmin)
                    rmin = r;
            } else {
                rmin = strlen(&cmdname[l]);
            }
            strncpy(namebuf, &cmdname[l], DOGSIZE - l);
            namebuf[DOGSIZE] = 0;
        }
    }
    if (c == 1) {
        twotabs = 0;
        namebuf[rmin++] = ' ';
        namebuf[rmin] = 0;
        return namebuf;
    }
    if (c > 1 && rmin > 0) {
        twotabs = 1;
        namebuf[rmin] = 0;
        return namebuf;
    }
    putchar('\007');
    if (c == 0 || twotabs < 2) {
        twotabs = 2;
        namebuf[0] = 0;
        return namebuf;
    }
    c = 0;
    dgputs("\r\n");
    for (i = 0; cmd_table[i].name; i++) {
        const char* cmdname = cmd_table[i].name;
        if (dgscmp(&cmdline[0], cmdname, l) == l) {
            printf("%12s", cmdname);
            if (++c % 6 == 0)
                dgputs("\r\n");
        }
    }
    if (c % 6 != 0)
        dgputs("\r\n");
    if (dgprom)
        dgputs(dgprom);
    dgputs(cmdline);
    for (i = cmdli; i < cmdlb; i++) {
        putchar('\b');
    }
    namebuf[0] = 0;
    return namebuf;
}

static char* findit(int patha) {
    int i, j, l;
    char* p;
    namebuf[0] = 0;
    for (i = j = patha; i < cmdli; i++) {
        if (cmdline[i] == '/')
            j = i + 1;
    }
    l = cmdli - j;
    if (l > DOGSIZE)
        l = DOGSIZE;
    if (j > patha) {
        int k = j;
        while (k - 1 > patha && cmdline[k - 1] == '/')
            k--;
        char t = cmdline[k];
        cmdline[k] = 0;
        p = full_path(&cmdline[patha]);
        cmdline[k] = t;
    } else {
        p = full_path("");
    }
    lfs_dir_t fd;
    if (fs_dir_open(&fd, p) < LFS_ERR_OK) {
        putchar('\007');
        twotabs = 1;
        return namebuf;
    }
    int c = 0;
    struct lfs_info nbuf;
    int rmin = 0, rtyp = 0, nmax = 0;
    while (fs_dir_read(&fd, &nbuf) > 0) {
        if (dgscmp(&cmdline[j], nbuf.name, l) == l) {
            int nlen = strlen(nbuf.name);
            if (c++) {
                int r = dgscmp(&nbuf.name[l], namebuf, DOGSIZE);
                if (r < rmin)
                    rmin = r;
                rtyp = 0;
            } else {
                rmin = strlen(&nbuf.name[l]);
                rtyp = nbuf.type == LFS_TYPE_DIR ? '/' : ' ';
            }
            if (nmax < nlen)
                nmax = nlen;
            strncpy(namebuf, &nbuf.name[l], DOGSIZE - l);
            namebuf[DOGSIZE] = 0;
        }
    }
    if (c == 1) {
        fs_dir_close(&fd);
        twotabs = 0;
        namebuf[rmin++] = rtyp;
        namebuf[rmin] = 0;
        return namebuf;
    }
    if (c > 1 && rmin > 0) {
        fs_dir_close(&fd);
        twotabs = 1;
        namebuf[rmin] = 0;
        return namebuf;
    }
    putchar('\007');
    if (c == 0 || twotabs < 2) {
        fs_dir_close(&fd);
        twotabs = 2;
        namebuf[0] = 0;
        return namebuf;
    }
    c = 0;
    int nmod = 72 / (nmax + 4);
    nmax = 72 / nmod;
    fs_dir_rewind(&fd);
    dgputs("\r\n");
    while (fs_dir_read(&fd, &nbuf) > 0) {
        if (dgscmp(&cmdline[j], nbuf.name, l) == l) {
            printf("%*s", nmax, nbuf.name);
            if (++c % nmod == 0)
                dgputs("\r\n");
        }
    }
    fs_dir_close(&fd);
    if (c % nmod != 0)
        dgputs("\r\n");
    if (dgprom)
        dgputs(dgprom);
    dgputs(cmdline);
    for (i = cmdli; i < cmdlb; i++) {
        putchar('\b');
    }
    namebuf[0] = 0;
    return namebuf;
}

static char* separator(int c) { return index(" <>|", c); }

static void dotab() {
    int j, fd;
    char* p;
    if (!dgmnt)
        return;
    j = cmdli;
    while (j > 0) {
        j--;
        if (separator(cmdline[j])) {
            j++;
            break;
        }
    }
    if (j == 0)
        p = findcmd();
    else
        p = findit(j);
    int n = strlen(p);
    for (j = cmdlb - 1; j >= cmdli; j--) {
        cmdline[j + n] = cmdline[j];
    }
    while (*p)
        cookputc(cmdline[cmdli++] = (*p++));
    cmdlb += n;
    cmdline[cmdlb] = 0;
    for (j = cmdli; j < cmdlb; j++) {
        cookputc(cmdline[j]);
    }
    for (j = cmdli; j < cmdlb; j++) {
        putchar('\b');
    }
    return;
}

void savehist(){
	lfs_file_t fp;
	if(fs_file_open(&fp,"/.history",
		LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < 0) {
		return;
	}
    int j = hista;
    while (j != histb) {
        char c = history[j];
        if (c == 0) fs_file_write(&fp,"\n",1);
        else fs_file_write(&fp,&c,1);
        j = (j + 1) % HSTSIZE;
    }
	fs_file_close(&fp);
}

static void addhistw(char* s) {
    for (;;) {
        int j = (histb + 1) % HSTSIZE;
        if (j == hista) {
            while (history[hista])
                hista = (hista + 1) % HSTSIZE;
            hista = (hista + 1) % HSTSIZE;
        }
        history[histb] = *s;
        histb = j;
        if (!*s)
            break;
        s++;
    }
}

static void addhist(char* s){
	int oldhistb = histb;
	addhistw(s);
	if (histb < oldhistb) savehist();
}

static void resthist(){
	lfs_file_t fp;
	if(fs_file_open(&fp,"/.history",LFS_O_RDONLY) < 0) {
		return;
	}
	int j = 0;
	for (;;) {
		char c;
		if (fs_file_read(&fp,&c,1) <= 0) break;
		if(c == '\n') {
			cmdline[j] = 0;
			addhistw(cmdline);
			j = 0;
		} else {
			cmdline[j++] = c;
		}
	}
	if (j > 0){
		cmdline[j] = 0;
		addhist(cmdline);
	}
	fs_file_close(&fp);
}

static const char* strprefix(const char* s, char* p) {
    while (*p)
        if (*s++ != *p++)
            return 0;
    return s;
}

static char* findhist(char* s, int n) {
    int j = histb, k = 0;
    if (hista == j)
        return 0;
    j = (j - 1 + HSTSIZE) % HSTSIZE;
    for (;;) {
        int jn = (j - 1 + HSTSIZE) % HSTSIZE;
        if (!history[jn]) {
            if (strprefix(&history[j], s)) {
                if (k++ == n)
                    return &history[j];
            }
        }
        if (jn == hista) {
            if (strprefix(&history[jn], s)) {
                if (k++ == n)
                    return &history[jn];
            }
            if (k == 0)
                return 0;
            n = (n % k + k) % k;
            k = 0;
            jn = (histb - 1 + HSTSIZE) % HSTSIZE;
        }
        j = jn;
    }
}

static void flushesc() {
    int ea = 0;
    while (escbuf[ea]) {
        cookputc(escbuf[ea]);
        cmdline[cmdli++] = escbuf[ea];
        cmdlb++;
        escbuf[ea++] = 0;
    }
    return;
}

static void eschp(char* hp) {
    if (hp) {
        for (cmdlb = 0; hp[cmdlb]; cmdlb++) {
            cmdline[cmdlb] = hp[cmdlb];
        }
        cmdli = cmdlb;
    }
    flushesc();
    return;
}

static int matchwork(int c) {
    int k;
    escbuf[esceb++] = c;
    for (k = 0; k < sizeof(esckeys) / sizeof(char*); k++) {
        const char* b;
        b = strprefix(esckeys[k], escbuf);
        if (b) {
            if (*b == 0)
                return k + 1;
            return -k - 1;
        }
    }
    escbuf[--esceb] = 0;
    return 0;
}

static int matchkey(int c) {
    int r = matchwork(c);
    if (!r) {
        flushesc();
        esceb = 0;
        r = matchwork(c);
    }
    if (r > 0) {
        while (esceb > 0)
            escbuf[--esceb] = 0;
    }
    return r;
}

static void cmdinsert(int c) {
    int j;
    for (;;) {
        for (j = cmdlb; j >= cmdli; j--) {
            cmdline[j] = cmdline[j - 1];
        }
        cmdlb++;
        cmdline[cmdli++] = c;
        cookputc(c);
        if (cmdlb >= CMDFULL)
            break;
        if (!dgleft())
            break;
        c = getchar();
        if (c < '\040' || c >= '\177') {
            ungetc(c, stdin);
            break;
        }
    }
    for (j = cmdli; j < cmdlb; j++) {
        cookputc(cmdline[j]);
    }
    for (j = cmdli; j < cmdlb; j++) {
        putchar('\b');
    }
}

char* dgreadln(char* buffer, int mnt, char* prom) {
    char* hp = 0;
    int hi = 0, ky;
    cmdline = buffer;
    dgmnt = mnt;
    dgprom = prom;
    esceb = 0;
    cmdlb = 0;
    cmdli = 0;
    twotabs = 0;
	if(hista==0&&histb==0) resthist();
    for (;;) {
        int c = getchar();
        if (c == '\t') {
            eschp(hp), hp = 0, hi = 0;
            dotab();
        } else if ((twotabs = 0, ky = matchkey(c))) {
            if (ky < 0) {
                //  look for more rabbits
            } else if (ky == 4 || ky == 5) {
                //  history is a special case
                int j;
                char* hp2;
                cmdline[cmdlb] = 0;
                hp2 = findhist(cmdline, hi);
                if (hp2) {
                    if (hp) {
                        for (j = 0; hp[j]; j++) {
                            dgputs("\b \b");
                        }
                    } else {
                        for (j = cmdli; j < cmdlb; j++)
                            putchar(' ');
                        for (j = cmdli; j < cmdlb; j++)
                            putchar('\b');
                        for (j = 0; j < cmdli; j++) {
                            dgputs("\b \b");
                        }
                    }
                    hp = hp2;
                    for (j = 0; hp[j]; j++)
                        cookputc(hp[j]);
                    if (ky == 4)
                        hi++;
                    else
                        hi--;
                } else {
                    putchar('\007');
                }
            } else
                switch (eschp(hp), hp = 0, hi = 0, ky) {
                case 2: // rubout
                    if (cmdli < cmdlb) {
                        cookputc(cmdline[cmdli++]);
                    }
                case 1: // backspace
                    if (cmdli > 0) {
                        int j;
                        cmdli--;
                        cmdlb--;
                        dgputs("\b");
                        for (j = cmdli; j < cmdlb; j++) {
                            cmdline[j] = cmdline[j + 1];
                            cookputc(cmdline[j]);
                        }
                        putchar(' ');
                        for (j = cmdli; j <= cmdlb; j++) {
                            putchar('\b');
                        }
                    } else {
                        putchar('\007');
                    }
                    break;
                case 3: // erase line
                    if (cmdli > 0) {
                        int j;
                        for (j = cmdli; j > 0; j--) {
                            putchar('\b');
                        }
                        for (j = cmdli; j < cmdlb; j++) {
                            cmdline[j - cmdli] = cmdline[j];
                            cookputc(cmdline[j]);
                        }
                        for (j = 0; j < cmdli; j++) {
                            putchar(' ');
                        }
                        for (j = 0; j < cmdlb; j++) {
                            putchar('\b');
                        }
                        cmdlb -= cmdli;
                        cmdli = 0;
                    } else {
                        putchar('\007');
                    }
                    break;
                case 6: // right arrow
                    if (cmdli < cmdlb) {
                        cookputc(cmdline[cmdli++]);
                    } else {
                        putchar('\007');
                    }
                    break;
                case 7: // left arrow
                    if (cmdli > 0) {
                        dgputs("\b");
                        cmdli--;
                    } else {
                        putchar('\007');
                    }
                    break;
                case 8: // home
                case 9: // home2
                    while (cmdli > 0) {
                        putchar('\b');
                        cmdli--;
                    }
                    break;
                case 10: // end
                case 11: // end2
                    while (cmdli < cmdlb)
                        cookputc(cmdline[cmdli++]);
                    break;
                case 12: // back word
                case 13: // back word2
                    while (cmdli > 0) {
                        putchar('\b');
                        cmdli--;
                        if (separator(cmdline[cmdli]))
                            break;
                    }
                    break;
                case 14: // next word
                case 15: // next word2
                    while (cmdli < cmdlb) {
                        cookputc(cmdline[cmdli++]);
                        if (cmdli < cmdlb) {
                            if (separator(cmdline[cmdli]))
                                break;
                        }
                    }
                    break;
                case 16: // insert space
                    cmdinsert(' ');
                    break;
                case 17: // erase EOL
                    if (cmdli < cmdlb) {
                        int j;
                        for (j = cmdli; j < cmdlb; j++) {
                            putchar(' ');
                        }
                        for (j = cmdli; j < cmdlb; j++) {
                            putchar('\b');
                        }
                        cmdlb = cmdli;
                    }
                }
        } else if (eschp(hp), hp = 0, hi = 0, c == '\r' || c == '\n') {
            dgputs("\r\n");
            cmdline[cmdlb] = 0;
            if (cmdlb > 0)
                addhist(cmdline);
            cmdline[cmdlb++] = '\n';
            cmdline[cmdlb] = 0;
            return cmdline;
        } else {
            cmdinsert(c);
        }
        if (cmdlb >= CMDFULL) {
            cmdline[cmdlb++] = '\n';
            cmdline[cmdlb] = 0;
            return cmdline;
        }
    }
}
