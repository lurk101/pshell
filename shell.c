/* SPDX-License-Identifier: GPL-2.0-or-later */

/* Copyright (c) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the BSD 3 clause license, which unfortunately
 * won't be written for another century.
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "hardware/structs/scb.h"

#include "pico/bootrom.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"

#include "cc.h"
#include "io.h"
#include "readln.h"
#include "tar.h"
#include "vi.h"
#include "xmodem.h"
#include "ymodem.h"
#if !defined(NDEBUG) || defined(PSHELL_TESTS)
#include "tests.h"
#endif

//#define COPYRIGHT "\u00a9" // for UTF8
#define COPYRIGHT "(c)" // for ASCII

#define MAX_ARGS 16

#define VT_ESC "\033"
#define VT_CLEAR VT_ESC "[H" VT_ESC "[J"
#define VT_BLINK VT_ESC "[5m"
#define VT_BOLD VT_ESC "[1m"
#define VT_NORMAL VT_ESC "[m"

typedef char buf_t[128];

extern char __heap_start;
extern char __heap_end;

buf_t result;

static uint32_t screen_x = 80, screen_y = 24;
static lfs_file_t file;
static buf_t cmd_buffer, path, curdir = "/";
static int argc;
static char* argv[MAX_ARGS + 1];
static bool mounted = false, run = true;
static char console[8];

static void set_translate_crlf(bool enable) {
    stdio_driver_t* driver;
#if LIB_PICO_STDIO_UART
    driver = &stdio_uart;
#endif
#if LIB_PICO_STDIO_USB
    driver = &stdio_usb;
#endif
    stdio_set_translate_crlf(driver, enable);
}

// used by Vi
void get_screen_xy(uint32_t* x, uint32_t* y) {
    *x = screen_x;
    *y = screen_y;
}

static void echo_key(char c) {
    putchar(c);
    if (c == '\r')
        putchar('\n');
}

char* full_path(const char* name) {
    if (name == NULL)
        return NULL;
    if (name[0] == '/') {
        strcpy(path, name);
        return path;
    }
    if (strncmp(name, "./", 2) == 0)
        name += 2;
    strcpy(path, curdir);
    if (strncmp(name, "../", 3) != 0) {
        if (name[0])
            strcat(path, name);
    } else {
        name += 3; // root doen't have a parent
        char* cp = strrchr(path, '/');
        if (cp != NULL)
            *cp = 0;
        cp = strrchr(path, '/');
        if (cp != NULL)
            *(cp + 1) = 0;
        strcat(path, name);
    }
    return path;
}

static void parse_cmd(void) {
    // read line into buffer
    char* cp = cmd_buffer;
    char* cp_end = cp + sizeof(cmd_buffer);
    char prompt[128];
    snprintf(prompt, sizeof(prompt), VT_BOLD "%s: " VT_NORMAL, full_path(""));
    cp = dgreadln(cmd_buffer, mounted, prompt);
    bool not_last = true;
    for (argc = 0; not_last && (argc < MAX_ARGS); argc++) {
        while ((*cp == ' ') || (*cp == ',') || (*cp == '='))
            cp++; // skip blanks
        if ((*cp == '\r') || (*cp == '\n'))
            break;
        argv[argc] = cp; // start of string
        while ((*cp != ' ') && (*cp != ',') && (*cp != '=') && (*cp != '\r') && (*cp != '\n'))
            cp++; // skip non blank
        if ((*cp == '\r') || (*cp == '\n'))
            not_last = false;
        *cp++ = 0; // terminate string
    }
    argv[argc] = NULL;
}

static int xmodem_rx_cb(uint8_t* buf, uint32_t len) {
    if (fs_file_write(&file, buf, len) != len)
        printf("error writing file\n");
}

static int xmodem_tx_cb(uint8_t* buf, uint32_t len) { return fs_file_read(&file, buf, len); }

static bool check_mount(bool need) {
    if (mounted == need)
        return false;
    sprintf(result, "filesystem is %smounted", (need ? "not " : ""));
    return true;
}

static bool check_name(void) {
    if (argc > 1)
        return false;
    strcpy(result, "missing file or directory name");
    return true;
}

static void xput_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (fs_file_open(&file, full_path(argv[1]), LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) <
        LFS_ERR_OK) {
        strcpy(result, "Can't create file");
        return;
    }
    set_translate_crlf(false);
    xmodemReceive(xmodem_rx_cb);
    set_translate_crlf(true);
    busy_wait_ms(3000);
    sprintf(result, "\nfile transfered, size: %d", fs_file_seek(&file, 0, LFS_SEEK_END));
    fs_file_close(&file);
}

static void yput_cmd(void) {
    if (check_mount(true))
        return;
    if (argc > 1) {
        strcpy(result, "yput doesn't take a parameter");
        return;
    }
    char* tmpname = strdup(full_path("ymodem.tmp"));
    if (fs_file_open(&file, tmpname, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < LFS_ERR_OK) {
        strcpy(result, "Can't create file");
        return;
    }
    set_translate_crlf(false);
    char name[256];
    int res = Ymodem_Receive(&file, 0x7fffffff, name);
    set_translate_crlf(true);
    fs_file_close(&file);
    if (res >= 0) {
        sprintf(result, "\nfile transfered, size: %d", fs_file_seek(&file, 0, LFS_SEEK_END));
        fs_rename(tmpname, full_path(name));
    } else {
        strcpy(result, "File transfer failed");
        fs_remove(tmpname);
    }
    free(tmpname);
}

static void cmnt_cmd(void){};

int check_from_to_parms(char** from, char** to, int copy) {
    *from = NULL;
    *to = NULL;
    int rc = 1;
    do {
        if (argc < 3) {
            strcpy(result, "need two names");
            break;
        }
        bool from_is_dir = false;
        bool to_is_dir = false;
        bool to_exists = false;
        *from = strdup(full_path(argv[1]));
        if (*from == NULL) {
            strcpy(result, "no memory");
            break;
        }
        struct lfs_info info;
        if (fs_stat(*from, &info) < LFS_ERR_OK) {
            sprintf(result, "%s not found", *from);
            break;
        }
        from_is_dir = info.type == LFS_TYPE_DIR;
        *to = strdup(full_path(argv[2]));
        if (*to == NULL) {
            strcpy(result, "no memory");
            break;
        }
        if (fs_stat(*to, &info) == LFS_ERR_OK) {
            to_is_dir = info.type == LFS_TYPE_DIR;
            to_exists = 1;
        }
        if (copy && from_is_dir) {
            strcpy(result, "can't copy a directory");
            break;
        }
        if (to_exists && to_is_dir) {
            char* name = strrchr(*from, '/') + 1;
            bool append_slash = (*to)[strlen(*to) - 1] == '/' ? false : true;
            int l = strlen(*to) + strlen(name) + 1;
            if (append_slash)
                l++;
            char* to2 = malloc(l);
            if (!to2) {
                strcpy(result, "no memory");
                break;
            }
            strcpy(to2, *to);
            if (append_slash)
                strcat(to2, "/");
            strcat(to2, name);
            free(*to);
            *to = to2;
        }
        rc = 0;
    } while (0);
    if (rc) {
        if (*from)
            free(*from);
        if (*to)
            free(*to);
    }
    return rc;
}

static void mv_cmd(void) {
    char* from;
    char* to;
    if (check_from_to_parms(&from, &to, 0))
        return;
    struct lfs_info info;
    if (fs_rename(from, to) < LFS_ERR_OK)
        sprintf(result, "could not move %s to %s", from, to);
    else
        sprintf(result, "%s moved to %s", from, to);
    free(from);
    free(to);
}

static void news_cmd(void) {
    printf("\nWhat's new in version 2.1.5\n\n"
           "General\n\n"
           " - general source code, cmake, and script file cleanup\n"
           " - add support for picow and vgaboard\n"
           " - update C example source code\n"
           " - centralize reset function, no longer using watchdoc\n"
           " - remove trim command, garbage collection in littlefs is automatic\n"
           " - move all constant variables to flash, releasing RAM to better use\n"
           " - expand status command to show memory and screen use.\n"
           " - clarify and pack exe file header\n"
           " - delete resize cmd, use clear instead\n"
           " - improve release_build script (less error prone)\n\n"
           "Pico2 specific\n\n"
           " - add PWM IRQ numbers\n"
           " - all floating pointt operators as well as sqrtf function\n"
           "   calls are handled directly with inline CM33 floating point\n"
           "   intructions instead of wrapper calls\n"
           " - integer divide and modulus operators are handled using\n"
           "   inline CM33 hardware integer divide instructions instead\n"
           "   of wrapper calls\n"
           " - use expanded range of movw immediate to reduce amount of PC\n"
           "   relative addressing\n"
           " - many new peep hole optimizer opportunities\n");
}

static void cp_cmd(void) {
    char* from;
    char* to;
    char* buf = NULL;
    if (check_from_to_parms(&from, &to, 1))
        return;
    lfs_file_t in, out;
    bool in_ok = false, out_ok = false;
    do {
        buf = malloc(4096);
        if (buf == NULL) {
            strcpy(result, "no memory");
            break;
        }
        if (fs_file_open(&in, from, LFS_O_RDONLY) < LFS_ERR_OK) {
            sprintf(result, "error opening %s", from);
            break;
        }
        in_ok = true;
        if (fs_file_open(&out, to, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < LFS_ERR_OK) {
            sprintf(result, "error opening %s", from);
            break;
        }
        out_ok = true;
        int l = fs_file_read(&in, buf, 4096);
        while (l > 0) {
            if (fs_file_write(&out, buf, l) != l) {
                sprintf(result, "error writing %s", to);
                break;
            }
            l = fs_file_read(&in, buf, 4096);
        }
    } while (false);
    if (in_ok)
        fs_file_close(&in);
    if (out_ok)
        fs_file_close(&out);
    if (buf) {
        if (out_ok && fs_getattr(from, 1, buf, 4) == 4 && strcmp(buf, "exe") == 0)
            fs_setattr(to, 1, buf, 4);
        free(buf);
    }
    if (!result[0])
        sprintf(result, "file %s copied to %s", from, to);
    free(from);
    free(to);
}

static void cat_cmd(void) {
    if (check_mount(true))
        return;
    bool paginate = false;
    char* path = NULL;
    for (int arg = 1; arg < argc; arg++)
        if (strcmp(argv[arg], "-p") == 0)
            paginate = true;
        else
            path = argv[arg];
    if (path == NULL) {
        strcpy(result, "file name argument is required");
        return;
    }
    lfs_file_t file;
    char* fpath = full_path(path);
    if (fs_file_open(&file, fpath, LFS_O_RDONLY) < LFS_ERR_OK) {
        sprintf(result, "error opening file %s", fpath);
        return;
    }
    int l = fs_file_seek(&file, 0, LFS_SEEK_END);
    fs_file_seek(&file, 0, LFS_SEEK_SET);
    char* buf = malloc(l);
    if (!buf) {
        strcpy(result, "insufficient memory");
        return;
    }
    int line = 0;
    int last = 0;
    if (fs_file_read(&file, buf, l) != l) {
        sprintf(result, "error reading file %s", fpath);
        goto done;
    }
    char* cp_end = buf + l;
    for (char* cp = buf; cp < cp_end; cp++) {
        if (*cp == 0x1A)
            break;
        putchar(*cp);
        if (paginate && line && (last != line) && (line % (screen_y - 1) == 0)) {
            char cc = getchar();
            if (cc == 0x03)
                break;
        }
        last = line;
        if (*cp == 0x0A)
            line++;
    }
done:
    free(buf);
    fs_file_close(&file);
}

static void hex_cmd(void) {
    if (check_mount(true))
        return;
    bool paginate = false;
    char* path = NULL;
    for (int arg = 1; arg < argc; arg++)
        if (strcmp(argv[arg], "-p") == 0)
            paginate = true;
        else
            path = argv[arg];
    if (path == NULL) {
        strcpy(result, "file name argument is required");
        return;
    }
    lfs_file_t file;
    char* fpath = full_path(path);
    if (fs_file_open(&file, fpath, LFS_O_RDONLY) < LFS_ERR_OK) {
        sprintf(result, "error opening file %s", fpath);
        return;
    }
    int l = fs_file_seek(&file, 0, LFS_SEEK_END);
    fs_file_seek(&file, 0, LFS_SEEK_SET);
    char* buf = malloc(l);
    if (!buf) {
        strcpy(result, "insufficient memory");
        return;
    }
    if (fs_file_read(&file, buf, l) != l) {
        sprintf(result, "error reading file %s", fpath);
        goto done;
    }
    char* p = buf;
    char* pend = p + l;
    int line = 0;
    while (p < pend) {
        char* p2 = p;
        printf("%04x", (int)(p - buf));
        for (int i = 0; i < 16; i++) {
            if ((i & 3) == 0)
                printf(" ");
            if (p + i < pend)
                printf("%02x", p[i]);
            else
                printf("  ");
        }
        printf(" '");
        p = p2;
        for (int i = 0; i < 16; i++) {
            if (p + i < pend) {
                if (isprint(p[i]))
                    printf("%c", p[i]);
                else
                    printf(".");
            } else
                printf(" ");
        }
        printf("'\n");
        p += 16;
        if (paginate & (++line % (screen_y - 1) == 0)) {
            char cc = getchar();
            if (cc == 0x03)
                break;
        }
    }
done:
    free(buf);
    fs_file_close(&file);
}

static void xget_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (fs_file_open(&file, full_path(argv[1]), LFS_O_RDONLY) < LFS_ERR_OK) {
        strcpy(result, "Can't open file");
        return;
    }
    set_translate_crlf(false);
    xmodemTransmit(xmodem_tx_cb);
    set_translate_crlf(true);
    fs_file_close(&file);
}

static void yget_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (fs_file_open(&file, full_path(argv[1]), LFS_O_RDONLY) < LFS_ERR_OK) {
        strcpy(result, "Can't open file");
        return;
    }
    int siz = fs_file_seek(&file, 0, LFS_SEEK_END);
    fs_file_seek(&file, 0, LFS_SEEK_SET);
    set_translate_crlf(false);
    int res = Ymodem_Transmit(full_path(argv[1]), siz, &file);
    set_translate_crlf(true);
    fs_file_close(&file);
    if (res)
        strcpy(result, "File transfer failed");
    else
        sprintf(result, "%d bytes sent", siz);
}

static void mkdir_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (fs_mkdir(full_path(argv[1])) < LFS_ERR_OK) {
        strcpy(result, "Can't create directory");
        return;
    }
    sprintf(result, "%s created", full_path(argv[1]));
}

static char rmdir_path[256];

static bool clean_dir(char* name) {
    int path_len = strlen(rmdir_path);
    if (path_len)
        strcat(rmdir_path, "/");
    strcat(rmdir_path, name);
    lfs_dir_t dir_f;
    if (fs_dir_open(&dir_f, rmdir_path) < LFS_ERR_OK) {
        printf("can't open %s directory\n", rmdir_path);
        return false;
    }
    struct lfs_info info;
    while (fs_dir_read(&dir_f, &info) > 0)
        if (info.type == LFS_TYPE_DIR && strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (!clean_dir(info.name)) {
                fs_dir_close(&dir_f);
                return false;
            }
    fs_dir_rewind(&dir_f);
    while (fs_dir_read(&dir_f, &info) > 0) {
        if (info.type == LFS_TYPE_REG) {
            int plen = strlen(rmdir_path);
            strcat(rmdir_path, "/");
            strcat(rmdir_path, info.name);
            if (fs_remove(rmdir_path) < LFS_ERR_OK) {
                printf("can't remove %s", rmdir_path);
                fs_dir_close(&dir_f);
                return false;
            }
            printf("%s removed\n", rmdir_path);
            rmdir_path[plen] = 0;
        }
    }
    fs_dir_close(&dir_f);
    if (fs_remove(rmdir_path) < LFS_ERR_OK) {
        sprintf(result, "can't remove %s", rmdir_path);
        return false;
    }
    printf("%s removed\n", rmdir_path);
    rmdir_path[path_len] = 0;
    return true;
}

static void rm_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    bool recursive = false;
    if (strcmp(argv[1], "-r") == 0) {
        if (argc < 3) {
            strcpy(result, "specify a file or directory name");
            return;
        }
        recursive = true;
        argv[1] = argv[2];
    }
    // lfs won't remove a non empty directory but returns without error!
    struct lfs_info info;
    char* fp = full_path(argv[1]);
    if (fs_stat(fp, &info) < LFS_ERR_OK) {
        sprintf(result, "%s not found", full_path(argv[1]));
        return;
    }
    int isdir = 0;
    if (info.type == LFS_TYPE_DIR) {
        isdir = 1;
        lfs_dir_t dir;
        fs_dir_open(&dir, fp);
        int n = 0;
        while (fs_dir_read(&dir, &info))
            if ((strcmp(info.name, ".") != 0) && (strcmp(info.name, "..") != 0))
                n++;
        fs_dir_close(&dir);
        if (n) {
            if (recursive) {
                rmdir_path[0] = 0;
                clean_dir(fp);
                return;
            } else
                sprintf(result, "directory %s not empty", fp);
            return;
        }
    }
    if (fs_remove(fp) < LFS_ERR_OK)
        strcpy(result, "Can't remove file or directory");
    sprintf(result, "%s %s removed", isdir ? "directory" : "file", fp);
}

static void mount_cmd(void) {
    if (check_mount(false))
        return;
    if (fs_mount() != LFS_ERR_OK) {
        strcpy(result, "Error mounting filesystem");
        return;
    }
    mounted = true;
    strcpy(result, "mounted");
}

static void unmount_cmd(void) {
    if (check_mount(true))
        return;
    if (fs_unmount() != LFS_ERR_OK) {
        strcpy(result, "Error unmounting filesystem");
        return;
    }
    mounted = false;
    strcpy(result, "unmounted");
}

#if !defined(NDEBUG) || defined(PSHELL_TESTS)
static void trim_cmd(void) {
    if (check_mount(true))
        return;
    if (fs_gc() != LFS_ERR_OK)
        strcpy(result, "Error trimming filesystem");
    else
        strcpy(result, "Ok");
}
#endif

static void format_cmd(void) {
    if (check_mount(false))
        return;
    printf("are you sure (y/N) ? ");
    fflush(stdout);
    parse_cmd();
    if ((argc == 0) || ((argv[0][0] | ' ') != 'y')) {
        strcpy(result, "user cancelled");
        return;
    }
    if (fs_format() != LFS_ERR_OK)
        strcpy(result, "Error formating filesystem");
    strcpy(result, "formatted");
}

static void disk_space(uint64_t n, char* buf) {
    double d = n;
    static const char* suffix[] = {"B", "KB", "MB", "GB", "TB"};
    char** sfx = (char**)suffix;
    while (d >= 1000.0) {
        d /= 1000.0;
        sfx++;
    }
    sprintf(buf, "%.1f%s", d, *sfx);
}

static void status_cmd(void) {
    struct fs_fsstat_t stat;
    if (mounted) {
        fs_fsstat(&stat);
        const char percent = 37;
        char total_size[32], used_size[32];
        disk_space((int64_t)stat.block_count * stat.block_size, total_size);
        disk_space((int64_t)stat.blocks_used * stat.block_size, used_size);
#ifndef NDEBUG
        printf("\ntext size 0x%x (%d), bss size 0x%x (%d)", stat.text_size, stat.text_size,
               stat.bss_size, stat.bss_size);
#endif
        sprintf(result, "Storage - blocks: total %d, used %d, size %d (%s of %s, %1.1f%c used)\n",
                (int)stat.block_count, (int)stat.blocks_used, (int)stat.block_size, used_size,
                total_size, stat.blocks_used * 100.0 / stat.block_count, percent);
    } else
        sprintf(result, "Storage - not mounted\n");
    sprintf(result + strlen(result),
            "Memory  - heap: %.1fK, program code space: %dK, global data space: %dK\n"
            "Console - %s, width %d, height %d",
            (&__heap_end - &__heap_start) / 1024.0, prog_space / 1024, data_space / 1024, console,
            screen_x, screen_y);
}

static void ls_cmd(void) {
    if (check_mount(true))
        return;
    int show_all = 0;
    char** av = argv;
    if ((argc > 1) && (strcmp(av[1], "-a") == 0)) {
        argc--;
        av++;
        show_all = 1;
    }
    if (argc > 1)
        full_path(av[1]);
    else
        full_path("");
    lfs_dir_t dir;
    if (fs_dir_open(&dir, path) < LFS_ERR_OK) {
        strcpy(result, "not a directory");
        return;
    }
    printf("\n");
    struct lfs_info info;
    while (fs_dir_read(&dir, &info) > 0)
        if (strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (info.type == LFS_TYPE_DIR)
                if ((info.name[0] != '.') || show_all)
                    printf(" %7d [%s]\n", info.size, info.name);
    fs_dir_rewind(&dir);
    while (fs_dir_read(&dir, &info) > 0)
        if (strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (info.type == LFS_TYPE_REG)
                if ((info.name[0] != '.') || show_all)
                    printf(" %7d %s\n", info.size, info.name);
    fs_dir_close(&dir);
}

static void cd_cmd(void) {
    if (check_mount(true))
        return;
    if (argc < 2) {
        strcpy(path, "/");
        goto cd_done;
    }
    if (strcmp(argv[1], ".") == 0)
        goto cd_done;
    if (strcmp(argv[1], "..") == 0) {
        if (strcmp(curdir, "/") == 0) {
            strcpy(result, "not a directory");
            return;
        }
        strcpy(path, curdir);
        char* cp = strrchr(path, '/');
        if (cp == NULL)
            cp = curdir;
        *cp = 0;
        cp = strrchr(path, '/');
        if (cp != NULL)
            *(cp + 1) = 0;
        goto cd_done;
    }
    full_path(argv[1]);
    lfs_dir_t dir;
    if (fs_dir_open(&dir, path) < LFS_ERR_OK) {
        strcpy(result, "not a directory");
        return;
    }
    fs_dir_close(&dir);
cd_done:
    strcpy(curdir, path);
    if (curdir[strlen(curdir) - 1] != '/')
        strcat(curdir, "/");
    sprintf(result, "changed to %s", curdir);
}

static void cc_cmd(void) {
    if (check_mount(true))
        return;
    cc(0, argc, argv);
}

static void tar_cmd(void) {
    if (check_mount(true))
        return;
    tar(argc, argv);
}

#if !defined(NDEBUG) || defined(PSHELL_TESTS)
static void tests_cmd(void) {
    if (check_mount(true))
        return;
    run_tests(argc, argv);
}
#endif

static void vi_cmd(void) {
    if (check_mount(true))
        return;
    vi(argc - 1, argv + 1);
}

#define NORETURN __attribute__((noreturn))

NORETURN static void reset_after(int ms) {
    sleep_ms(ms);
    scb_hw->aircr = 0x5FA0004;
    for (;;)
        ;
}

NORETURN static void reboot_cmd(void) {
    // release any resources we were using
    if (mounted) {
        savehist();
        fs_unmount();
    }
    reset_after(500);
}

#if LIB_PICO_STDIO_USB
static void usbboot_cmd(void) {
    // release any resources we were using
    if (mounted) {
        savehist();
        fs_unmount();
    }
    reset_usb_boot(0, 0);
}
#endif

static void quit_cmd(void) {
    printf("\nare you sure (Y/n) ? ");
    fflush(stdout);
    char c = getchar();
    putchar(c);
    putchar('\n');
    if (c != 'y' && c != 'Y' && c != '\r')
        return;
    // release any resources we were using
    if (mounted) {
        savehist();
        fs_unmount();
    }
    printf("\nbye!\n");
    exit(0);
}

static void version_cmd(void) {
    printf("\nPico Shell " PSHELL_GIT_TAG ", LittleFS v%d.%d, Vi " VI_VER ", SDK v%d.%d.%d\n",
           LFS_VERSION >> 16, LFS_VERSION & 0xffff, PICO_SDK_VERSION_MAJOR, PICO_SDK_VERSION_MINOR,
           PICO_SDK_VERSION_REVISION);
#if !defined(NDEBUG)
    printf("gcc %s\n", __VERSION__);
#endif
}

static bool cursor_pos(uint32_t* x, uint32_t* y) {
    int rc = false;
    *x = 80;
    *y = 24;
    do {
        printf(VT_ESC "[6n");
        fflush(stdout);
        int k = getchar_timeout_us(100000);
        if (k == PICO_ERROR_TIMEOUT)
            break;
        char* cp = cmd_buffer;
        while (cp < cmd_buffer + sizeof cmd_buffer) {
            k = getchar_timeout_us(100000);
            if (k == PICO_ERROR_TIMEOUT)
                break;
            *cp++ = k;
        }
        if (cp == cmd_buffer)
            break;
        if (cmd_buffer[0] != '[')
            break;
        *cp = 0;
        if (cp - cmd_buffer < 5)
            break;
        char* end;
        uint32_t row, col;
        if (!isdigit(cmd_buffer[1]))
            break;
        errno = 0;
        row = strtoul(cmd_buffer + 1, &end, 10);
        if (errno)
            break;
        if (*end != ';' || !isdigit(end[1]))
            break;
        col = strtoul(end + 1, &end, 10);
        if (errno)
            break;
        if (*end != 'R')
            break;
        if (row < 1 || col < 1 || (row | col) > 0x7fff)
            break;
        *x = col;
        *y = row;
        rc = true;
    } while (false);
    return rc;
}

static bool screen_size(void) {
    int rc = false;
    screen_x = 80;
    screen_y = 24;
    uint32_t cur_x, cur_y;
    do {
        set_translate_crlf(false);
        if (!cursor_pos(&cur_x, &cur_y))
            break;
        printf(VT_ESC "[999;999H");
        if (!cursor_pos(&screen_x, &screen_y))
            break;
        if (cur_x > screen_x)
            cur_x = screen_x;
        if (cur_y > screen_y)
            cur_y = screen_y;
        printf("\033[%d;%dH", cur_y, cur_x);
        fflush(stdout);
        rc = true;
    } while (false);
    set_translate_crlf(true);
    return rc;
}

static void clear_cmd(void) {
    strcpy(result, VT_CLEAR "\n");
    screen_size();
}

// clang-format off
const cmd_t cmd_table[] = {
    {"cat",     cat_cmd,        "display a text file, use -p to paginate"},
    {"cc",      cc_cmd,         "compile & run C source file. cc -h for help"},
    {"cd",      cd_cmd,         "change directory"},
    {"clear",   clear_cmd,      "clear the screen"},
    {"cp",      cp_cmd,         "copy a file"},
    {"format",  format_cmd,     "format the filesystem"},
    {"hex",     hex_cmd,        "simple hexdump, use -p to paginate"},
    {"ls",      ls_cmd,         "list a directory, -a to show hidden files"},
    {"mkdir",   mkdir_cmd,      "create a directory"},
    {"mount",   mount_cmd,      "mount the filesystem"},
    {"mv",      mv_cmd,         "rename a file or directory"},
    {"news",    news_cmd,       "what's new in this release"},
    {"quit",    quit_cmd,       "shutdown the system"},
    {"reboot",  reboot_cmd,     "restart the system"},
    {"rm",      rm_cmd,         "remove a file or directory. -r for recursive"},
    {"status",  status_cmd,     "display the filesystem status"},
    {"tar",     tar_cmd,        "manage tar archives"},
#if !defined(NDEBUG) || defined(PSHELL_TESTS)
    {"tests",   tests_cmd,      "run all tests"},
    {"trim",    trim_cmd,       "filesystem garbage collection"},
#endif
    {"umount",  unmount_cmd,    "unmount the filesystem"},
    {"version", version_cmd,    "display pico shell's version"},
    {"vi",      vi_cmd,         "edit file(s) with vi"},
    {"xget",    xget_cmd,       "xmodem get a file (pico->host)"},
    {"xput",    xput_cmd,       "xmodem put a file (host->pico)"},
    {"yget",    yget_cmd,       "ymodem get a file (pico->host)"},
    {"yput",    yput_cmd,       "ymodem put a file (host->pico)"},
    {"#",       cmnt_cmd,       "comment line"},
	{0}
};
// clang-format on

static void help(void) {
    printf("\n");
    for (int i = 0; cmd_table[i].name; i++)
        printf("%7s - %s\n", cmd_table[i].name, cmd_table[i].descr);
}

static const char* search_cmds(int len) {
    if (len == 0)
        return NULL;
    int i, last_i, count = 0;
    for (i = 0; cmd_table[i].name; i++)
        if (strncmp(cmd_buffer, cmd_table[i].name, len) == 0) {
            last_i = i;
            count++;
        }
    if (count != 1)
        return NULL;
    return cmd_table[last_i].name + len;
}

NORETURN static void Fault_Handler(void) {
    static const char* clear = "\n\n" VT_BOLD "*** " VT_BLINK "CRASH" VT_NORMAL VT_BOLD
                               " - Rebooting in 3 seconds ***" VT_NORMAL "\r\n\n";
    for (const char* cp = clear; *cp; cp++)
        putchar(*cp);
#ifndef NDEBUG
    for (;;)
        ;
#endif
    reset_after(3000);
}

static bool run_as_cmd(const char* dir) {
    char* tfn;
    if (strlen(dir) == 0)
        tfn = full_path(argv[0]);
    else {
        if (argv[0][0] == '/')
            return false;
        tfn = argv[0];
    }
    char* fn = malloc(strlen(tfn) + 6);
    strcpy(fn, dir);
    strcat(fn, tfn);
    char buf[4];
    if (fs_getattr(fn, 1, buf, sizeof(buf)) != sizeof(buf)) {
        free(fn);
        return false;
    }
    if (strcmp(buf, "exe")) {
        free(fn);
        return false;
    }
    argv[0] = fn;
    cc(1, argc, argv);
    free(fn);
    return true;
}

// application entry point
int main(void) {
    // initialize the pico SDK
    stdio_init_all();
    bool uart = true;
#if LIB_PICO_STDIO_USB
    while (!stdio_usb_connected())
        sleep_ms(1000);
    uart = false;
#endif
    ((int*)scb_hw->vtor)[3] = (int)Fault_Handler; // Hard fault
#if PICO_RP2350
    ((int*)scb_hw->vtor)[4] = (int)Fault_Handler; // Memory manager
    ((int*)scb_hw->vtor)[5] = (int)Fault_Handler; // Bus fault
    ((int*)scb_hw->vtor)[6] = (int)Fault_Handler; // Usage fault
#endif
    getchar_timeout_us(1000);
    bool detected = screen_size();
    printf(VT_CLEAR);
    fflush(stdout);
    printf("\n" VT_BOLD "Pico Shell" VT_NORMAL " - Copyright " COPYRIGHT " 1883 Thomas Edison\n"
           "This program comes with ABSOLUTELY NO WARRANTY.\n"
           "This is free software, and you are welcome to redistribute it\n"
           "under certain conditions. See LICENSE file for details.\n");
    char buf[16];
#if defined(VGABOARD_SD_CLK_PIN)
    strcpy(buf, "sd card");
#else
    strcpy(buf, "flash");
#endif
    if (uart) {
#if defined(PICO_DEFAULT_UART)
        sprintf(console, "UART%d", PICO_DEFAULT_UART);
#else
        strcpy(console, "UART");
#endif
    } else
        strcpy(console, "USB");
    printf("\nplatform: " PICO_BOARD ", console: %s [%u X %u], filesystem: %s\n\n", console,
           screen_x, screen_y, buf);
    if (!detected) {
        printf("\nYour terminal does not respond to standard VT100 escape sequences"
               "\nsequences. The editor will likely not work at all!");
        fflush(stdout);
    }

    if (fs_load() != LFS_ERR_OK) {
        printf("Can't access filesystem device! Aborting.\n");
        exit(-1);
    }
    if (fs_mount() != LFS_ERR_OK) {
        printf("The flash file system appears corrupt or unformatted!\n"
               " would you like to format it (Y/n) ? ");
        fflush(stdout);
        char c = getchar();
        while (c != 'y' && c != 'Y' && c != 'N' && c != 'n' && c != '\r')
            c = getchar();
        putchar(c);
        if (c != '\r')
            echo_key('\r');
        putchar('\n');
        if (c == 'y' || c == 'y')
            if (fs_format() != LFS_ERR_OK)
                printf("Error formating file system!\n\n");
            else {
                if (fs_mount() != LFS_ERR_OK)
                    printf("Error formating file system!\n\n");
                else {
                    printf("file system formatted and mounted\n\n");
                    mounted = true;
                }
            }
    } else {
        printf("file system automatically mounted\n\n");
        mounted = true;
    }
    printf("enter a command or hit ENTER for command list\n");
    while (run) {
        printf("\n" VT_BOLD "%s: " VT_NORMAL, full_path(""));
        fflush(stdout);
        parse_cmd();
        result[0] = 0;
        bool found = false;
        if (argc) {
            if (!strcmp(argv[0], "q")) {
                quit_cmd();
                continue;
            }
            for (int i = 0; cmd_table[i].name; i++)
                if (strcmp(argv[0], cmd_table[i].name) == 0) {
                    cmd_table[i].func();
                    if (result[0])
                        printf("\n%s\n", result);
                    found = true;
                    break;
                }
            if (!found) {
                if (!run_as_cmd("") && !run_as_cmd("/bin/"))
                    printf("\nunknown command '%s'. hit ENTER for help\n", argv[0]);
            }
        } else
            help();
    }
    fs_unload();

    printf("\ndone\n");
    sleep_ms(1000);
}
