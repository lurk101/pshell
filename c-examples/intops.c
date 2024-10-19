void int_tests(int x, int y) {
    printf("%d + %d = %d\n", x, y, x + y);
    printf("%d - %d = %d\n", x, y, x - y);
    printf("%d * %d = %d\n", x, y, x * y);
    printf("%d / %d = %d\n", x, y, x / y);
    printf("%d %% %d = %d\n", x, y, x % y);
    printf("%d > %d = %d\n", x, y, x > y);
    printf("%d < %d = %d\n", x, y, x < y);
    printf("%d >= %d = %d\n", x, y, x >= y);
    printf("%d <= %d = %d\n", x, y, x <= y);
    printf("%d == %d = %d\n", x, y, x == y);
    printf("%d != %d = %d\n\n", x, y, x != y);
}

int main() {
    int_tests(3, 4);
    int_tests(4, 3);
    int_tests(4, 4);
    return 0;
}
