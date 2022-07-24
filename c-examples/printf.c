/* Simple printf and sprintf formating test */

int main() {
    char buf[100];
    sprintf(buf, "%7s %d %f %d %f\n", "sprintf", 1, 2.0, 3, 4.0);
    printf(buf);
    return printf("%7s %d %f %d %f\n", "printf", 1, 2.0, 3, 4.0);
}
