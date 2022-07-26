/*  lorenz.c--Lorenz 96 dynamical system animation
    Version 3 Written 2022 by Eric Olson */

#include <stdio.h>
#define N 72

char top[4], bot[4], csi[3];
int rlen, rlen2, clen;
float xmin, xmax;

int tstart;
void tic() { tstart = time_us_32(); }
float toc() { return (float)(time_us_32() - tstart) / 1000000.0; }

int hextoi(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c + 10 - 'A';
    if (c >= 'a' && c <= 'f')
        return c + 10 - 'a';
    return 0;
}

#define Lf 6
#define Lh 3

void out72(int* a) {
    int i;
    for (i = Lf - 1; i >= 0; i--) {
        printf("%03x", a[i]);
    }
    printf("\n");
}

void strto72(int* a, char* s) {
    char* p;
    int i, j;
    for (i = 0; i < Lf; i++)
        a[i] = 0;
    for (p = s; *p; p++)
        ;
    i = 0;
    j = 0;
    for (p--; p >= s; p--) {
        a[i] |= hextoi(*p) << 4 * j;
        if (j < 2)
            j++;
        else {
            j = 0;
            i++;
            if (i >= Lf)
                break;
        }
    }
}

void add72(int* a, int* b) {
    int i, r, s;
    s = 0;
    for (i = 0; i < Lf; i++) {
        r = a[i] + b[i] + s;
        a[i] = r & 4095;
        s = r >> 12;
    }
}

void mul72(int* a, int* b) {
    int c[Lf];
    int i, j, k, r, s;
    for (i = 0; i < Lf; i++) {
        c[i] = a[i];
        a[i] = 0;
    }
    if (b == a)
        b = c;
    for (i = 0; i < Lf; i++) {
        for (j = 0; j < Lf - i; j++) {
            s = b[i] * c[j];
            for (k = i + j; k < Lf && s != 0; k++) {
                r = a[k] + s;
                a[k] = r & 4095;
                s = r >> 12;
            }
        }
    }
}

void mtswap(int* a) {
    int i, r;
    for (i = 0; i < Lh; i++) {
        r = a[i];
        a[i] = a[i + Lh];
        a[i + Lh] = r;
    }
}

struct {
    int x[Lf], w[Lf], s[Lf];
} rstate;

int rint24() {
    mul72(rstate.x, rstate.x);
    add72(rstate.w, rstate.s);
    add72(rstate.x, rstate.w);
    mtswap(rstate.x);
    return rstate.x[1] << 12 | rstate.x[0];
}

int rdice(int n) {
    int d;
    d = 16777216 / n;
    while (1) {
        int r;
        r = rint24() / d;
        if (r < n)
            return r;
    }
}

float rfloat() { return (float)rint24() / 16777216.0; }

void rseed(char* s) {
    strto72(rstate.x, s);
    strto72(rstate.w, "0");
    strto72(rstate.s, "D9B5AD4ECEDA1CE2A9");
}

void pstate() {
    printf("x:");
    out72(rstate.x);
    printf("w:");
    out72(rstate.w);
    printf("s:");
    out72(rstate.s);
}

void rowcol(int x, int y) { printf("%s%d;%dH", csi, x, y); }

void clear() { printf("%sH%sJ", csi, csi); }

/* dX/dt + B(X,X) + X = F */
void lorenz(float* y, float* x) {
    int i;
    y[0] = 20.0 - x[0] - x[clen - 1] * (x[clen - 2] - x[1]);
    y[1] = 20.0 - x[1] - x[0] * (x[clen - 1] - x[2]);
    for (i = 2; i < clen - 1; i++) {
        y[i] = 20.0 - x[i] - x[i - 1] * (x[i - 2] - x[i + 1]);
    }
    y[clen - 1] = 20.0 - x[clen - 1] - x[clen - 2] * (x[clen - 3] - x[0]);
}

float k1[N];
void euler(float* x, float h) {
    int i;
    lorenz(k1, x);
    for (i = 0; i < clen; i++) {
        x[i] = x[i] + h * k1[i];
    }
}

int rp[N];
void ascplot(float* x) {
    int j, row;
    float alpha;
    alpha = (float)(rlen - 1) / (xmax - xmin);
    for (j = 0; j < clen; j++) {
        row = (int)(alpha * (x[j] - xmin)) + 1;
        if (row != rp[j]) {
            int jp1;
            jp1 = j + 1;
            rowcol(row, jp1);
            printf("*");
            if (rp[j] > 0) {
                rowcol(rp[j], jp1);
                printf(" ");
            }
            rp[j] = row;
        }
    }
}

void uniplot(float* x) {
    int j, row;
    float alpha;
    alpha = (float)(rlen2 - 1) / (xmax - xmin);
    for (j = 0; j < clen; j++) {
        row = (int)(alpha * (x[j] - xmin)) + 2;
        if (row != rp[j]) {
            int rowd2, rpjd2, jp1;
            rpjd2 = rp[j] / 2;
            rowd2 = row / 2;
            jp1 = j + 1;
            rowcol(rowd2, jp1);
            if (row % 2 == 0)
                printf(top);
            else
                printf(bot);
            if (rpjd2 > 0 && rpjd2 != rowd2) {
                rowcol(rpjd2, jp1);
                printf(" ");
            }
            rp[j] = row;
        }
    }
}

void doinit() {
    rseed("1234");
    csi[0] = 27;
    csi[1] = '[';
    csi[2] = 0;
    top[0] = 226;
    top[1] = 150;
    top[2] = 128;
    top[3] = 0;
    bot[0] = 226;
    bot[1] = 150;
    bot[2] = 132;
    bot[3] = 0;
    xmin = 0;
    xmax = 0;
    rlen = screen_height();
    clen = screen_width() - 1;
    if (clen > N)
        clen = N;
    rlen2 = 2 * rlen;
}

float X[N];
int main() {
    float t;
    int j, n, nmax;
    doinit();
    clear();
    printf("lorenz.c--Lorenz 96 dynamical system animation\n"
           "Version 3 Written 2022 by Eric Olson\n\n");
    for (j = 0; j < clen; j++) {
        X[j] = rfloat();
    }
    nmax = 100000;
    tic();
    for (n = 0; n < nmax; n++) {
        euler(X, 0.001953125);
        for (j = 0; j < clen; j++) {
            if (X[j] < xmin)
                xmin = X[j];
            if (X[j] > xmax)
                xmax = X[j];
        }
        //      ascplot(X);
        uniplot(X);
    }
    rowcol(rlen - 5, 1);
    t = toc();
    printf("Iteration rate is %g per second.\n", (float)nmax / t);
    return 0;
}
