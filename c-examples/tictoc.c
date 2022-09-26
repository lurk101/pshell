/* Timer test */

int main(int ac, char* av[]) {
    int intvl = 2;
    if (ac > 1)
        intvl = atoi(av[1]);
    int tic = 0;
    while (1) {
        printf("%s\n", (tic ^= 1) ? "tic" : "toc");
        if (getchar_timeout_us(intvl * 1000000) == 3)
            break;
    }
    return intvl;
}
