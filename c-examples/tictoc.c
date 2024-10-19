/* Timer test */

int main(int ac, char* av[]) {
    int tic = 0;
    while (1) {
        printf("%s\n", (tic ^= 1) ? "tic" : "toc");
        if (getchar_timeout_us(500000) == 3)
            break;
    }
    return 0;
}
