/* LittleFs file I/O test. Create, write, close, open, read, seek, close. */

int main(int ac, char* av[]) {

    int rc = -1;

    int fin = 0, fout = open("test.txt", O_WRONLY + O_CREAT);
    if (fout < 0) {
        printf("error opening test.txt\n");
        goto exit;
    }
    printf("file created\n");

    char* s = "part 1 part 2";
    if (write(fout, s, strlen(s)) != strlen(s)) {
        printf("error writing test.txt\n");
        goto exit_close;
    }
    printf("file written\n");

    close(fout);
    fout = 0;
    printf("file closed\n");

    rename("test.txt", "test2.txt");
    printf("file renamed\n");

    fin = open("test2.txt", O_RDONLY);
    if (fin < 0) {
        printf("error opening test.txt\n");
        goto exit;
    }
    printf("file opened\n");

    lseek(fin, 7, 0);
    printf("file seeked\n");

    char buf[32];
    if (read(fin, buf, sizeof(buf)) < 0) {
        printf("error reading test.txt\n");
        goto exit_close;
    }
    printf("read: %s\n", buf);
    if (strcmp(buf, "part 2")) {
        printf("expected part2!");
        goto exit_close;
    }

    close(fin);
    fin = 0;
    printf("file closed\n");

    remove("test2.txt");

    rc = 0;

exit_close:
    if (fout)
        close(fout);
    if (fin)
        close(fin);
exit:;
    return rc;
}
