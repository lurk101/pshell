char s[100];

int sieve() {
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
    while (i < sizeof(s)) {
        if (!s[i])
            printf("%d ", i);
        i++;
    }
    printf("\n\n");
    return printf("calculation time %f milliseconds\n", (float)start / 1000.0);
}
