int main() {
    char buf[100];
    sprintf(buf, "%d %f %d %f\n", 1, 2.0, 3, 4.0);
    printf(buf);
    printf("%d %f %d %f\n", 1, 2.0, 3, 4.0);
}
