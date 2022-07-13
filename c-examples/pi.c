/* Test floats and recursion */

float F(int i) {
    if (i > 20)
        return 1.0;
    return 1.0 + (float)i / (2.0 * (float)i + 1.0) * F(i + 1);
}

int main() { printf("The value of Pi is %f\n", 2.0 * F(1)); }
