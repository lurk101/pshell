/* Array test. Calculate primes using sieve. */

char s[100];

void sieve() {
    int i, j;
    for (i = 2; i < sizeof(s); i++)
        for (j = i + i; j < sizeof(s); j += i)
            s[j] = 1;
}

int main() {
    printf("Prime numbers less than %d\n\n", sizeof(s));
    int start = time_us_32();
    sieve();
    start = time_us_32() - start;
    int i;
    for (i = 2; i < sizeof(s); ++i)
        if (!s[i])
            printf("%d ", i);
    printf("\n\n");
    printf("calculation time %d microseconds\n", start);
    return 0;
}
