#include "pico/stdio.h"

#include "stdio.h"

#include "fs.h"
#include "tar.h"

extern char* full_path(char* name);

struct posix_hdr {      /* byte offset */
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
};                      /* 512 */

#define BLK_SZ sizeof(struct posix_hdr)

static struct posix_hdr* hdr;

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
    memset(hdr, 0, BLK_SZ);
    strcpy(hdr->name, path + root_len);
    sprintf(hdr->size, "%011o", info->size);
    memset(hdr->chksum, ' ', 8);
    memcpy(hdr->magic, "ustar ", 6);
    strcpy(hdr->version, " ");
    strcpy(hdr->mode, "0777");
    hdr->typeflag = '0';
    unsigned cks = 0;
    for (int i = 0; i < BLK_SZ; i++)
        cks += ((unsigned char*)hdr)[i];
    sprintf(hdr->chksum, "%07o", cks);
    if (fs_file_write(&tar_f, hdr, BLK_SZ) < LFS_ERR_OK) {
        printf("error writing %s\n", hdr->name);
        return false;
    }
    int len = info->size;
    while (len > 0) {
        if (fs_file_read(&in_f, hdr, BLK_SZ) < LFS_ERR_OK) {
            printf("can't read %s\n", full_path(hdr->name));
            return false;
        }
        if (fs_file_write(&tar_f, hdr, BLK_SZ) < LFS_ERR_OK) {
            printf("can't write tar file\n");
            return false;
        }
        len -= BLK_SZ;
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
        printf("can't open %s\n", hdr->name);
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

static bool create_directories(char* fn) {
    char* cp = strchr(fn, '/');
    while (cp) {
        *cp = 0;
        struct lfs_info info;
        char* fp = full_path(fn);
        if (fs_stat(fp, &info) < LFS_ERR_OK) {
            if (fs_mkdir(fp) < LFS_ERR_OK) {
                printf("unable to create %s directory\n", fn);
                return false;
            }
        } else if (info.type != LFS_TYPE_DIR) {
            printf("can't replace file %s with directory\n", fn);
            return false;
        }
        *cp = '/';
        cp = strchr(cp + 1, '/');
    }
    return true;
}

void tar(int ac, char* av[]) {

    enum { NO_OP = 0, CREATE_OP, LIST_OP, EXTRACT_OP } op = NO_OP;

    if (ac < 3) {
    help:
        printf("\ntar [-][t|c|x] tarball_file [file_or_dir [... file_or_dir]]\n\n"
               "-t   show tar file contents\n"
               "-c   create tar file from files\n"
               "-x   extract tar file contents\n");
        return;
    }
    char* cp = av[1];
    if (*cp == '-')
        ++cp;
    switch (*cp) {
    case 'c':
        op = CREATE_OP;
        break;
    case 't':
        op = LIST_OP;
        break;
    case 'x':
        op = EXTRACT_OP;
        break;
    default:
        goto help;
    }
    path = tar_fn = NULL;
    hdr = malloc(sizeof(struct posix_hdr));
    if (!hdr) {
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
    if (op == CREATE_OP)
        mode = LFS_O_WRONLY | LFS_O_CREAT;
    tar_fn = strdup(full_path(av[2]));
    if (!tar_fn) {
        printf("no memory");
        goto bail2;
    }
    if (fs_file_open(&tar_f, tar_fn, mode) < LFS_ERR_OK) {
        printf("Can't open %s\n", tar_fn);
        goto bail2;
    }
    switch (op) {
    case LIST_OP:
    case EXTRACT_OP:
        printf("\n");
        while (true) {
            if (fs_file_read(&tar_f, hdr, BLK_SZ) < LFS_ERR_OK) {
                printf("error reading tar file\n");
                return;
            }
            if (hdr->name[0] == 0)
                break;
            int l = strtol(hdr->size, NULL, 8);
            if (op == LIST_OP) {
                printf("%s\n", hdr->name);
                l = (l + BLK_SZ - 1) & (~(BLK_SZ - 1));
                fs_file_seek(&tar_f, l, LFS_SEEK_CUR);
            } else {
                printf("extracting %s\n", hdr->name);
                if (!create_directories(hdr->name))
                    goto bail1;
                lfs_file_t out_f;
                if (fs_file_open(&out_f, full_path(hdr->name), LFS_O_WRONLY | LFS_O_CREAT) <
                    LFS_ERR_OK) {
                    printf("could not create file %s\n", hdr->name);
                    goto bail1;
                }
                while (l > 0) {
                    if (fs_file_read(&tar_f, hdr, BLK_SZ) < LFS_ERR_OK) {
                        printf("error reading tar file\n");
                        fs_file_close(&out_f);
                        goto bail1;
                    }
                    if (fs_file_write(&out_f, hdr, l >= BLK_SZ ? BLK_SZ : l) < LFS_ERR_OK) {
                        printf("error writing file\n");
                        fs_file_close(&out_f);
                        goto bail1;
                    }
                    l -= BLK_SZ;
                }
                fs_file_close(&out_f);
            }
        }
        break;
    case CREATE_OP:
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
        memset(hdr, 0, BLK_SZ);
        fs_file_write(&tar_f, hdr, BLK_SZ);
        fs_file_write(&tar_f, hdr, BLK_SZ);
        break;
    }

bail1:
    fs_file_close(&tar_f);
bail2:
    free(hdr);
    if (path)
        free(path);
    if (tar_fn)
        free(tar_fn);
}
