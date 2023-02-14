char array[30];

// Swap integer values by array indexes
void swap(int a, int b) {
    char tmp = array[a];
    array[a] = array[b];
    array[b] = tmp;
}

// Partition the array into two halves and return the
// index about which the array is partitioned
int partition(int left, int right) {
    int pivotIndex = left;
    int pivotValue = array[pivotIndex];
    int i, index = left;
    swap(pivotIndex, right);
    for (i = left; i < right; i++)
        if (array[i] < pivotValue)
            swap(i, index++);
    swap(right, index);
    return index;
}

// Quicksort the array
void quicksort(int left, int right) {
    if (left < right) {
        int index = partition(left, right);
        quicksort(left, index - 1);
        quicksort(index + 1, right);
    }
}

int main() {
    int i, sz;
    printf("Hit RETURN ");
    getchar();
    printf("\n");
    sz = sizeof(array) / sizeof(char);
    srand(time_us_32());
    for (i = 0; i < sz; ++i)
        array[i] = rand() % 100;
    printf("random order\n");
    for (i = 0; i < sz; i++)
        printf("%d ", array[i]);
    printf("\n");
    int t = time_us_32();
    quicksort(0, sz - 1);
    t = time_us_32() - t;
    printf("sorted\n");
    for (i = 0; i < sz; i++)
        printf("%d ", array[i]);
    printf("\nrun time %f ms.\n", (float)t / 1000.0);
    return 0;
}
