#if !defined(NDEBUG) || defined(PSHELL_TESTS)

#include "stdio.h"

#include "pico/stdlib.h"

#include "cc.h"
#include "fs.h"
#include "tests.h"

extern char* full_path(char* name);

void run_tests(int ac, char* av[]) {
    lfs_dir_t in_d;
    if (fs_dir_open(&in_d, full_path("")) < LFS_ERR_OK) {
        printf("can't open directory\n");
        return;
    }
    for (;;) {
        struct lfs_info info;
        if (fs_dir_read(&in_d, &info) <= 0)
            break;
        if (info.type == LFS_TYPE_REG) {
            int is_c = 0;
            char* cp = strrchr(info.name, '.');
            if (cp)
                is_c = strcmp(cp, ".c") == 0;
            if (is_c) {
                char* t_av[2] = {"cc"};
                t_av[1] = strdup(full_path(info.name));
                printf("cc %s\n", t_av[1]);
                if (cc(0, 2, t_av) != 0) {
                    free(t_av[1]);
                    break;
                }
                free(t_av[1]);
            }
        }
    }
    fs_dir_close(&in_d);
}

#endif
