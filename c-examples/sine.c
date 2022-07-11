int main() {
    int angle;
    for (angle = 0; angle <= 360; angle += 16) {
        float rad = (float)angle * 0.01745329252;
        int pos = 30 + (int)(sinf(rad) * 25.0);
        while (pos > 0) {
            printf(" ");
            pos--;
        }
        printf("*\n");
    }
}
