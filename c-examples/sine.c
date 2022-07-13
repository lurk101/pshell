/* Math function test. Display a sine wave */

int main() {
    int angle, incr;
    for (angle = incr, incr = 16; angle <= 360; angle += incr) {
        float rad = (float)angle * 0.01745329252;
        int pos = 30 + (int)(sinf(rad) * 25.0);
        while (pos > 0) {
            printf(" ");
            pos--;
        }
        printf("*\n");
    }
}
