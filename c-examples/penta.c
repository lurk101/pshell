// Pentamino

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
    l0[(y + y_max / 2) * x_max + x + x_max / 2] = 1;
    putchar_xy(x + x_max / 2, y + y_max / 2, '*');
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

// pentamino
int xy[5][2] = {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2}};

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
    for (i = 0; i < 5; ++i)
        set(xy[i][0], xy[i][1]);
    while (true)
        next_gen();
    return 0;
}
