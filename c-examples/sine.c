int main() {
    int angle = 0;
    while (angle <= 360) {
        float rad = (float)angle * 0.01745329252;
        int pos = 30 + (int)(sin(rad) * 25.0);
        while (pos > 0) {
            printf(" ");
            pos--;
        }
        printf("*\n");
        angle += 15;
    }
}
