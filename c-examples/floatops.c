void float_tests(float x, float y) {
    printf("%f + %f = %f\n", x, y, x + y);
    printf("%f - %f = %f\n", x, y, x - y);
    printf("%f * %f = %f\n", x, y, x * y);
    printf("%f / %f = %f\n", x, y, x / y);
    printf("%f > %f = %d\n", x, y, x > y);
    printf("%f < %f = %d\n", x, y, x < y);
    printf("%f >= %f = %d\n", x, y, x >= y);
    printf("%f <= %f = %d\n", x, y, x <= y);
    printf("%f == %f = %d\n", x, y, x == y);
    printf("%f != %f = %d\n\n", x, y, x != y);
}

int main() {
    float_tests(-3.0, 4.0);
    float_tests(4.0, -3.0);
    float_tests(4.0, 4.0);
    float x;
    for (x = 0.0; x < 11.0; x = x + 1.0)
        printf("sqrtf(%f) = %f\n", x, sqrtf(x));
    return 0;
}
