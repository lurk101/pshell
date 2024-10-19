/* Array test. Calculate primes using sieve. */

#pragma uchar

int bits;
char s[8];

void set(int i) { s[i / 8] |= (1 << (i % 8)); }

int get(int i) { return (s[i / 8] & (1 << (i % 8))) != 0; }

void sieve() {
    int i, j;
    for (i = 2; i < bits; i++)
        for (j = i + i; j < bits; j += i)
            set(j);
}

int main() {
    printf("Prime numbers less than %d\n\n", sizeof(s) * 8);
    int start = time_us_32();
    bits = sizeof(s) * 8;
    sieve();
    start = time_us_32() - start;
    int i;
    for (i = 2; i < bits; ++i)
        if (!get(i))
            printf("%d ", i);
    printf("\n");
    printf("\n\n");
    printf("calculation time %d microseconds\n", start);
    return 0;
}
