#include "pico/stdio.h"

#include "stdio.h"

#include "fs.h"
#include "tar.h"

#define BLOCKSIZE 512

extern char* full_path(char* name);

struct posix_header {   /* byte offset */
    char name[100];     /*   0 */
    char mode[8];       /* 100 */
    char uid[8];        /* 108 */
    char gid[8];        /* 116 */
    char size[12];      /* 124 */
    char mtime[12];     /* 136 */
    char chksum[8];     /* 148 */
    char typeflag;      /* 156 */
    char linkname[100]; /* 157 */
    char magic[6];      /* 257 */
    char version[2];    /* 263 */
    char uname[32];     /* 265 */
    char gname[32];     /* 297 */
    char devmajor[8];   /* 329 */
    char devminor[8];   /* 337 */
    char prefix[155];   /* 345 */
    char pad[12];       /* 500 */
};

static struct posix_header* header;

static char* path;
static int root_len;
static lfs_file_t tar_f;
static char* tar_fn;

static bool tar_file(struct lfs_info* info) {
    int path_len = strlen(path);
    if (path[strlen(path) - 1] != '/')
        strcat(path, "/");
    strcat(path, info->name);
    if (strcmp(path, tar_fn) == 0) {
        printf("skipping %s\n", path);
        goto done;
    }
    lfs_file_t in_f;
    if (fs_file_open(&in_f, path, LFS_O_RDONLY) < LFS_ERR_OK) {
        printf("can't open %s\n", path);
        return false;
    }
    memset(header, 0, BLOCKSIZE);
    strcpy(header->name, path + root_len);
    sprintf(header->size, "%011o", info->size);
    memset(header->chksum, ' ', 8);
    memcpy(header->magic, "ustar ", 6);
    strcpy(header->version, " ");
    strcpy(header->mode, "0777");
    header->typeflag = '0';
    unsigned cks = 0;
    for (int i = 0; i < BLOCKSIZE; i++)
        cks += ((unsigned char*)header)[i];
    sprintf(header->chksum, "%07o", cks);
    if (fs_file_write(&tar_f, header, BLOCKSIZE) < LFS_ERR_OK) {
        printf("error writing %s\n", header->name);
        return false;
    }
    int len = info->size;
    while (len > 0) {
        if (fs_file_read(&in_f, header, BLOCKSIZE) < LFS_ERR_OK) {
            printf("can't read %s\n", full_path(header->name));
            return false;
        }
        if (fs_file_write(&tar_f, header, BLOCKSIZE) < LFS_ERR_OK) {
            printf("can't write tar file\n");
            return false;
        }
        len -= 512;
    }
    fs_file_close(&in_f);
    printf("%s archived\n", path + root_len);
done:
    path[path_len] = 0;
    return true;
}

static bool tar_dir(struct lfs_info* info) {
    int path_len = strlen(path);
    if (path[strlen(path) - 1] != '/')
        strcat(path, "/");
    if (info->name[0] != '/')
        strcat(path, info->name);
    lfs_dir_t in_d;
    if (fs_dir_open(&in_d, path) < LFS_ERR_OK) {
        printf("can't open %s\n", header->name);
        return false;
    }
    while (true) {
        struct lfs_info info;
        if (fs_dir_read(&in_d, &info) <= 0)
            break;
        int rc = true;
        if (info.type == LFS_TYPE_DIR) {
            if (strcmp(info.name, ".") && strcmp(info.name, ".."))
                rc = tar_dir(&info);
        } else
            rc = tar_file(&info);
        if (!rc)
            return false;
    }
    fs_dir_close(&in_d);
    path[path_len] = 0;
    return true;
}

static void tar_list(void) {
    printf("\n");
    while (true) {
        if (fs_file_read(&tar_f, header, BLOCKSIZE) < LFS_ERR_OK) {
            printf("error reading tar file\n");
            return;
        }
        if (header->name[0] == 0)
            break;
        printf("%s\n", header->name);
        int l = strtol(header->size, NULL, 8);
        l = (l + BLOCKSIZE - 1) & (~(BLOCKSIZE - 1));
        fs_file_seek(&tar_f, l, LFS_SEEK_CUR);
    }
}

void tar(int ac, char* av[]) {
    if (ac < 3) {
    help:
        printf("\ntar [-][t|c|x]f tarball file_or_dir [... file_or_dir]\n");
        return;
    }
    bool create;
    bool list = false;
    char* cp = av[1];
    if (*cp == '-')
        ++cp;
    if (*cp == 'c')
        create = true;
    else if (*cp == 'x')
        create = false;
    else if (*cp == 't')
        list = true;
    else
        goto help;
    ++cp;
    if (*cp != 'f')
        goto help;
    if (!list && ac < 4)
        goto help;
    header = malloc(sizeof(struct posix_header));
    if (!header) {
        printf("no memory");
        return;
    }
    path = malloc(256);
    if (!path) {
        printf("no memory");
        goto bail2;
    }
    strcpy(path, full_path(""));
    root_len = strlen(path);
    int mode = LFS_O_RDONLY;
    if (create)
        mode = LFS_O_WRONLY | LFS_O_CREAT;
    tar_fn = strdup(full_path(av[2]));
    if (!tar_fn) {
        printf("no memory");
        return;
    }
    if (fs_file_open(&tar_f, tar_fn, mode) < LFS_ERR_OK) {
        printf("Can't open %s\n", tar_fn);
        goto bail2;
    }
    if (list) {
        tar_list();
        goto bail1;
    }
    if (create) {
        for (int an = 3; an < ac; an++) {
            char* name = av[an];
            struct lfs_info info;
            if (fs_stat(full_path(name), &info) < LFS_ERR_OK) {
                printf("Can't find %s\n", full_path(name));
                goto bail1;
            }
            bool rc;
            if (info.type == LFS_TYPE_DIR)
                rc = tar_dir(&info);
            else
                rc = tar_file(&info);
            if (!rc)
                goto bail1;
        }
        memset(header, 0, BLOCKSIZE);
        fs_file_write(&tar_f, header, BLOCKSIZE);
        fs_file_write(&tar_f, header, BLOCKSIZE);
    }
bail1:
    fs_file_close(&tar_f);
bail2:
    free(header);
    if (path)
        free(path);
    if (tar_fn)
        free(tar_fn);
}
