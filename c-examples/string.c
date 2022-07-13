/* string function test. Compare then concatenate
   two program arguments */

int main(int ac, char* av[]) {
    if (ac < 3) {
        printf("please provide 2 strings");
        return -1;
    }
    char* a = (char*)strdup(av[1]);
    char* b = (char*)strdup(av[2]);
    int i = strcmp(a, b);
    if (i < 0)
        printf("%s is less than %s\n", a, b);
    else if (i > 0)
        printf("%s is greater than %s\n", a, b);
    else
        printf("%s is equal to %s\n", a, b);
    char* c = (char*)malloc(strlen(a) + strlen(b) + 1);
    strcpy(c, a);
    strcat(c, b);
    printf("combining strings gives %s\n", c);
    free(a);
    free(b);
    free(c);
}
