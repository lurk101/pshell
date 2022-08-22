// Pentamino

char esc[16];
int x_max, y_max;
char *l0, *l1, *count;
int scr_size;

void putchar_xy(int x, int y, char c) {
    sprintf(esc + 1, "[%d;%dH", y + 1, x + 1);
    printf("%s%c", esc, c);
}

void clear(int on) {
    strcpy(esc + 1, "[H");
    printf(esc);
    esc[2] = 'J';
    printf(esc);
	if (on)
    	strcpy(esc + 2, "?25h");
	else
    	strcpy(esc + 2, "?25l");
    printf(esc);
}

void set(int x, int y) {
    l0[(y + y_max / 2) * x_max + x + x_max / 2] = 1;
    putchar_xy(x, y, '*');
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
int xy[10] = { 1, 0, 2, 0,  0, 1, 1, 1,  1, 2};

int main() {
    esc[0] = 27;
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
