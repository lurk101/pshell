int main() {
    float x = 3, y = 4;
    printf("%f + %f = %f\n", x, y, x + y);
    printf("%f - %f = %f\n", x, y, x - y);
    printf("%f * %f = %f\n", x, y, x * y);
    printf("%f / %f = %f\n", x, y, x / y);
    printf("%f > %f = %d\n", x, y, x > y);
    printf("%f < %f = %d\n", x, y, x < y);
    printf("%f >= %f = %d\n", x, y, x >= y);
    printf("%f <= %f = %d\n\n", x, y, x <= y);
    x = 4, y = 3;
    printf("%f + %f = %f\n", x, y, x + y);
    printf("%f - %f = %f\n", x, y, x - y);
    printf("%f * %f = %f\n", x, y, x * y);
    printf("%f / %f = %f\n", x, y, x / y);
    printf("%f > %f = %d\n", x, y, x > y);
    printf("%f < %f = %d\n", x, y, x < y);
    printf("%f >= %f = %d\n", x, y, x >= y);
    printf("%f <= %f = %d\n\n", x, y, x <= y);
    x = 4, y = 4;
    printf("%f + %f = %f\n", x, y, x + y);
    printf("%f - %f = %f\n", x, y, x - y);
    printf("%f * %f = %f\n", x, y, x * y);
    printf("%f / %f = %f\n", x, y, x / y);
    printf("%f > %f = %d\n", x, y, x > y);
    printf("%f < %f = %d\n", x, y, x < y);
    printf("%f >= %f = %d\n", x, y, x >= y);
    printf("%f <= %f = %d\n\n", x, y, x <= y);
    int i;
    for (i = 0; i < 11; i++) {
        float a = sqrtf((float)i);
        printf("sqrtf(%d) = %f\n", i, a);
    }

    return 0;
}
