char b[1760], z[1760];

void goto_top_left() { printf("%s", "\033[H"); }

void clear(int on) {
    printf("\033[H");
    printf("\033[J");
    if (on)
        printf("\033[?25h");
    else
        printf("\033[?25l");
}

void main() {
    clear(0);
    int sA = 1024, cA = 0, sB = 1024, cB = 0, _;
    for (;;) {
        goto_top_left();
        int sj = 0, cj = 1024, j, i, k;
        memset(b, 32, 1760);
        memset(z, 127, 1760);
        for (j = 0; j < 90; j++) {
            int si = 0, ci = 1024;
            for (i = 0; i < 324; i++) {
                int R1 = 1, R2 = 2048, K2 = 5120 * 1024;

                int x0 = R1 * cj + R2, x1 = ci * x0 >> 10, x2 = cA * sj >> 10, x3 = si * x0 >> 10,
                    x4 = R1 * x2 - (sA * x3 >> 10), x5 = sA * sj >> 10,
                    x6 = K2 + R1 * 1024 * x5 + cA * x3, x7 = cj * si >> 10,
                    x = 40 + 30 * (cB * x1 - sB * x4) / x6, y = 12 + 15 * (cB * x4 + sB * x1) / x6,
                    N = (-cA * x7 - cB * ((-sA * x7 >> 10) + x2) - ci * (cj * sB >> 10) >> 10) -
                            x5 >>
                        7;

                int o = x + 80 * y;
                char zz = (x6 - K2) >> 15;
                if (22 > y && y > 0 && x > 0 && 80 > x && zz < z[o]) {
                    z[o] = zz;
                    b[o] = ".,-~:;=!*#$@"[N > 0 ? N : 0];
                }
                _ = ci;
                ci -= 5 * si >> 8;
                si += 5 * _ >> 8;
                _ = 3145728 - ci * ci - si * si >> 11;
                ci = ci * _ >> 10;
                si = si * _ >> 10;
            }
            _ = cj;
            cj -= 9 * sj >> 7;
            sj += 9 * _ >> 7;
            _ = 3145728 - cj * cj - sj * sj >> 11;
            cj = cj * _ >> 10;
            sj = sj * _ >> 10;
        }
        for (k = 0; 1761 > k; k++)
            putchar(k % 80 ? b[k] : (char)10);
        _ = cA;
        cA -= 5 * sA >> 7;
        sA += 5 * _ >> 7;
        _ = 3145728 - cA * cA - sA * sA >> 11;
        cA = cA * _ >> 10;
        sA = sA * _ >> 10;
        _ = cB;
        cB -= 5 * sB >> 8;
        sB += 5 * _ >> 8;
        _ = 3145728 - cB * cB - sB * sB >> 11;
        cB = cB * _ >> 10;
        sB = sB * _ >> 10;

        if (getchar_timeout_us(0) == 3)
            break;
    }
    clear(1);
}
