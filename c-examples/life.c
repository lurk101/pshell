// Gosper glider gun

int x_max, y_max;
char *l0, *l1, *count;
int scr_size;

void putchar_xy(int x, int y, char c) { printf("\033[%d;%dH%c", y + 1, x + 1, c); }

void clear(int on) {
    printf("\033[H\033[J");
    if (on)
        printf("\033[?25h");
    else
        printf("\033[?25l");
}

void set(int x, int y) {
    l0[(y + 5) * x_max + x + 5] = 1;
    putchar_xy(x + 5, y + 5, '*');
}

void next_gen() {
    int ch = getchar_timeout_us(0);
    if (ch == 3) {
        clear(1);
        exit(0);
    }
    int x, y, x2, y2;
    memset((int)count, 0, scr_size);
    // count neighbors
    for (y = x_max; y < scr_size - x_max; y += x_max)
        for (x = 1; x < x_max - 1; x++)
            if (l0[y + x])
                for (y2 = y - x_max; y2 <= y + x_max; y2 += x_max)
                    for (x2 = x - 1; x2 <= x + 1; x2++)
                        if ((x2 != x) || (y2 != y))
                            count[y2 + x2]++;
    // update
    memcpy((int)l1, (int)l0, scr_size);
    for (y = x_max; y < scr_size - x_max; y += x_max)
        for (x = 1; x < x_max - 1; x++) {
            int xy = x + y;
            int n = count[xy];
            if (l0[xy]) {
                if ((n != 2) && (n != 3)) {
                    l1[xy] = 0;
                    putchar_xy(x, y / x_max, ' ');
                }
            } else if (n == 3) {
                l1[xy] = 1;
                putchar_xy(x, y / x_max, '*');
            }
        }
    // recycle
    memcpy((int)l0, (int)l1, scr_size);
}

// gosper glider gun
int xy[72] = {0,  4, 0,  5, 1,  4, 1,  5, 10, 4, 10, 5, 10, 6, 11, 3, 11, 7, 12, 2, 12, 8, 13, 2,
              13, 8, 14, 5, 15, 3, 15, 7, 16, 4, 16, 5, 16, 6, 17, 5, 20, 2, 20, 3, 20, 4, 21, 2,
              21, 3, 21, 4, 22, 1, 22, 5, 24, 0, 24, 1, 24, 5, 24, 6, 34, 2, 34, 3, 35, 2, 35, 3};

int main() {
    clear(0);
    x_max = screen_width();
    y_max = screen_height();
    scr_size = x_max * y_max;
    l0 = (char*)malloc(scr_size);
    memset((int)l0, 0, scr_size);
    l1 = (char*)malloc(scr_size);
    count = (char*)malloc(scr_size);
    int i;
    for (i = 0; i < sizeof(xy) / sizeof(int); i += 2)
        set(xy[i], xy[i + 1]);
    while (true)
        next_gen();
    return 0;
}
