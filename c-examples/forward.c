int fwd(int a, float b);

int test() { return fwd(1, 2.0); }

int fwd(int a, float b) { return printf("%d, %f\n", a, b); }

int main() {
    fwd(2, 3.0);
    return test();
}
