// exit test

void suba() { exit(0); }

void subb() {
    suba();
    exit(1);
}

int main() {
    printf("Should exit with return code 0\n");
    subb();
    return 2;
}
