/*  crc16.c--Compute cyclic redundancy check of specified files
    Written 2022 by Eric Olson

    Note: If compiling with optimization using clang or gcc please
    enable the -fwrapv option, otherwise the results are wrong. */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int crc16_file(char* s) {
    int fd = open(s, 0);
#ifdef LINUX
    unsigned
#endif
        char c;
    int crc = 0;
    while (read(fd, &c, 1)) {
        int x = (crc >> 8) ^ c;
        x ^= x >> 4;
        crc = ((crc << 8) ^ x ^ (x << 5) ^ (x << 12)) & 0177777;
    }
    close(fd);
    return crc;
}

int main(int argc, char* argv[]) {
    int i, r;
    if (argc == 1) {
        printf("crc16.c--Compute cyclic redundancy check of "
               "specified files\nWritten 2022 by Eric Olson\n\n"
               "Usage:  cc crc16.c <file1> [<file2> ...]\n");
        exit(1);
    }
    for (i = 1; i < argc; i++) {
        r = crc16_file(argv[i]);
        printf("%4x  %s\n", r, argv[i]);
    }
    return 0;
}
