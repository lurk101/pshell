
int histogram[64];

#define ITERATIONS 1000000

int main() {
    printf("Using get_rand_32()\n\n");

    printf("Upper 4 bits distribution. All buckets should be aproximately equal\n");
    int i, n;
    for (i = 0; i < 16; ++i)
        histogram[i] = 0;
    for (i = 0; i < ITERATIONS; ++i)
        ++histogram[(get_rand_32() >> 28) & 0xf];
    for (i = 0; i < 16; ++i)
        printf("%2d - %-8d expected %-.0f\n", i, histogram[i], (float)ITERATIONS / 16.0);

    printf("\nLower 4 bits distribution\n");
    for (i = 0; i < 16; ++i)
        histogram[i] = 0;
    for (i = 0; i < ITERATIONS; ++i)
        ++histogram[get_rand_32() & 0xf];
    for (i = 0; i < 16; ++i)
        printf("%2d - %-8d expected %-.0f\n", i, histogram[i], (float)ITERATIONS / 16.0);

    printf(
        "\nSingle bit distribution. There should be aproximately the same number of 0s and 1s\n");
    int ones = 0, zeros = 0, j;
    for (i = 0; i < ITERATIONS; ++i) {
        n = get_rand_32();
        for (j = 0; j < 32; ++j) {
            if (n < 0)
                ++ones;
            else
                ++zeros;
            n <<= 1;
        }
    }
    printf("zeros = %-8d  expected %.0f\n", zeros, (float)ITERATIONS * 16.0);
    printf("ones  = %-8d  expected %.0f\n", ones, (float)ITERATIONS * 16.0);

    printf("\nRun lengths distributions. The number of 0 runs and 1 runs should\n"
           "be aproximately the same, and total runs should be aproximately as\n"
           "expected\n");
    for (i = 0; i < 32; ++i)
        histogram[i] = 0;
    int last = -1, last_count;
    for (i = 0; i < ITERATIONS; ++i) {
        n = get_rand_32();
        for (j = 0; j < 32; ++j) {
            int next = n < 0 ? 1 : 0;
            n <<= 1;
            if (last < 0) {
                last = next;
                last_count = 1;
                continue;
            }
            if (next == last) {
                ++last_count;
                continue;
            }
            if (last_count > 31)
                last_count = 31;
            ++histogram[last * 32 + last_count];
            last = next;
            last_count = 1;
        }
    }
    if (last_count > 31)
        last_count = 31;
    ++histogram[last * 32 + last_count];
    float expected = (float)ITERATIONS * 8.0;
    for (i = 1; i < 32; ++i) {
        printf("%2d - zeros %-7d  ones %-7d  total %-8d  expected %.0f\n", i, histogram[i],
               histogram[i + 32], histogram[i] + histogram[i + 32], expected);
        expected /= 2.0;
    }
    return 0;
}

