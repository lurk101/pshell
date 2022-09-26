#define A 1 << 0
#define B 1 << 1
#define C 1 << 2
#define D 1 << 3
#define E 1 << 4
#define F 1 << 5
#define G 1 << 6

#define CF C | F
#define NCF ~CF
#define ACF A | C | F
#define NACF ~ACF
#define BCDF B | C | D | F
#define NBCDF ~BCDF
#define BCEF B | C | E | F
#define NBCEF ~BCEF
#define CDE C | D | E
#define NCDE ~CDE

char *line, *lp, N[10], n_numbers, n_patterns, numbers[16], patterns[16], S[256];
int size, result[7];

void parse() {
    int bar = 0;
    n_numbers = 0;
    n_patterns = 0;
    while (*lp != '\n') {
        int d = 0;
        while ((*lp >= 'a') && (*lp <= 'g'))
            d |= 1 << (*lp++ - 'a');
        if (bar)
            numbers[n_numbers++] = d;
        else
            patterns[n_patterns++] = d;
        while (*lp != '\n' && (*lp == ' ' || *lp == '|')) {
            if (*lp == '|')
                bar = 1;
            ++lp;
        }
    }
    ++lp;
}

int is_lit(int disp, int segment) { return disp & (1 << segment); }

void solve() {
    int r;
    for (r = 0; r < 7; ++r)
        result[r] = N[8];
    int pp;
    for (pp = 0; pp < n_patterns; pp++) {
        int pattern = patterns[pp];
        char wire_in;
        switch (popcount(pattern)) {
        case 2:
            for (wire_in = 0; wire_in < 7; ++wire_in)
                result[wire_in] &= is_lit(pattern, wire_in) ? CF : NCF;
            break;
        case 3:
            for (wire_in = 0; wire_in < 7; ++wire_in)
                result[wire_in] &= is_lit(pattern, wire_in) ? ACF : NACF;
            break;
        case 4:
            for (wire_in = 0; wire_in < 7; ++wire_in)
                result[wire_in] &= is_lit(pattern, wire_in) ? BCDF : NBCDF;
            break;
        case 5:
            for (wire_in = 0; wire_in < 7; ++wire_in)
                if (!is_lit(pattern, wire_in))
                    result[wire_in] &= BCEF;
            break;
        case 6:
            for (wire_in = 0; wire_in < 7; ++wire_in)
                if (!is_lit(pattern, wire_in))
                    result[wire_in] &= CDE;
            break;
        }
    }
    int w;
    for (w = 0; w < 7; ++w) {
        if (popcount(result[w]) != 1)
            continue;
        int wu;
        for (wu = 0; wu < 7; ++wu) {
            if (popcount(result[wu]) == 1)
                continue;
            result[wu] &= ~result[w];
        }
    }
}

int decode() {
    int rslt = 0;
    int np;
    for (np = 0; np < n_numbers; ++np) {
        char digit = numbers[np];
        int display = 0;
        int b;
        for (b = 0; b < 7; ++b) {
            if (is_lit(digit, b))
                display |= result[b];
        }
        rslt = rslt * 10 + S[display];
    }
    return rslt;
}

int main() {
    printf("--- Day 8: Seven Segment Search ---\n");
    int start = time_us_32();
    int fd;
    if ((fd = open("day8.txt", O_RDONLY)) == 0) {
        printf("can't find day8.txt\n");
        exit(-1);
    }
    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    line = (char*)malloc(size);
    read(fd, line, size);
    close(fd);
    N[0] = A | B | C | E | F | G;
    N[1] = C | F;
    N[2] = A | C | D | E | G;
    N[3] = A | C | D | F | G;
    N[4] = B | C | D | F;
    N[5] = A | B | D | F | G;
    N[6] = A | B | D | E | F | G;
    N[7] = A | C | F;
    N[8] = A | B | C | D | E | F | G;
    N[9] = A | B | C | D | F | G;
    memset(S, 0, sizeof(S));
    int n;
    for (n = 0; n < 10; ++n)
        S[N[n]] = n;
    lp = line;
    int part1 = 0, part2 = 0;
    while (lp < line + size) {
        parse();
        int nn;
        for (nn = 0; nn < n_numbers; ++nn) {
            char num = numbers[nn];
            char cnt = popcount(num);
            if (cnt == 2 || cnt == 3 || cnt == 4 || cnt == 7)
                ++part1;
        };
        solve();
        part2 += decode();
    }

    printf("Part 1  - %d\n", part1);
    printf("Part 2  - %d\n", part2);
    printf("Elapsed - %f ms.\n", (float)(time_us_32() - start) / 1000.0);
    return 0;
}
