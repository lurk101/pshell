char s[100];

int sieve() {
    int i = 2;
    while (i < sizeof(s)) {
        int j = i + i;
        while (j < sizeof(s)) {
            s[j] = 1;
            j += i;
        }
        i++;
    }
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
