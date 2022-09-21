int array[30];

// Swap integer values by array indexes
void swap(int a, int b) {
    int tmp = array[a];
    array[a] = array[b];
    array[b] = tmp;
}

// Partition the array into two halves and return the
// index about which the array is partitioned
int partition(int left, int right) {
    int pivotIndex = left;
    int pivotValue = array[pivotIndex];
    int index = left;
    int i;
    swap(pivotIndex, right);
    for (i = left; i < right; i++) {
        if (array[i] < pivotValue) {
            swap(i, index);
            index += 1;
        }
    }
    swap(right, index);
    return index;
}

// Quicksort the array
void quicksort(int left, int right) {
    if (left >= right)
        return;
    int index = partition(left, right);
    quicksort(left, index - 1);
    quicksort(index + 1, right);
}

int main() {
    int i, sz;
    printf("Hit RETURN ");
    getchar();
    printf("\n");
    sz = sizeof(array) / sizeof(int);
    srand(time_us_32());
    for (i = 0; i < sz; ++i)
        array[i] = rand() % sz;
    printf("random order\n");
    for (i = 0; i < sz; i++)
        printf("%d ", array[i]);
    printf("\n");
    quicksort(0, sz - 1);
    printf("sorted\n");
    for (i = 0; i < sz; i++)
        printf("%d ", array[i]);
    printf("\n");
    return 0;
}
