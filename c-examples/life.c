// Gosper glider gun

char esc[16];
int x_max, y_max;
char *l0, *l1, *count;
int scr_size;

void putchar_xy(int x, int y, char c) {
    sprintf(esc + 1, "[%d;%dH", y + 1, x + 1);
    printf("%s%c", esc, c);
}

void clear() {
    strcpy(esc + 1, "[H");
    printf(esc);
    esc[2] = 'J';
    printf(esc);
    strcpy(esc + 1, "[?25l");
    printf(esc);
}

void set(int x, int y) {
    l0[(y + 5) * x_max + x + 5] = 1;
    putchar_xy(x + 5, y + 5, '*');
}

void next_gen() {
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

int main() {
    esc[0] = 27;
    clear();
    x_max = screen_width();
    y_max = screen_height();
    scr_size = x_max * y_max;
    l0 = (char*)malloc(scr_size);
    memset((int)l0, 0, scr_size);
    l1 = (char*)malloc(scr_size);
    count = (char*)malloc(scr_size);
    set(0, 4);
    set(0, 5);
    set(1, 4);
    set(1, 5);
    set(10, 4);
    set(10, 5);
    set(10, 6);
    set(11, 3);
    set(11, 7);
    set(12, 2);
    set(12, 8);
    set(13, 2);
    set(13, 8);
    set(14, 5);
    set(15, 3);
    set(15, 7);
    set(16, 4);
    set(16, 5);
    set(16, 6);
    set(17, 5);
    set(20, 2);
    set(20, 3);
    set(20, 4);
    set(21, 2);
    set(21, 3);
    set(21, 4);
    set(22, 1);
    set(22, 5);
    set(24, 0);
    set(24, 1);
    set(24, 5);
    set(24, 6);
    set(34, 2);
    set(34, 3);
    set(35, 2);
    set(35, 3);
    while (true)
        next_gen();
}
