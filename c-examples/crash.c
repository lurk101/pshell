/* CRASH recovery test. Intentional hard fault */

int main() {
    *((int*)3) = 0; // unaligneid store
}
