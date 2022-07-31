/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Copyright (c) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the BSD 3 clause license, which unfortunately
 * won't be written for another century.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A little flash file system manager for the Raspberry Pico
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "hardware/structs/scb.h"
#include "hardware/watchdog.h"

#include "pico/bootrom.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"

#include "cc.h"
#include "dgreadln.h"
#include "fs.h"
#include "tar.h"
#include "version.h"
#include "vi.h"
#include "xmodem.h"

#if PICO_SDK_VERSION_MAJOR > 1 || (PICO_SDK_VERSION_MAJOR == 1 && PICO_SDK_VERSION_MINOR >= 4)
#define SDK14 1
#else
#define SDK14 0
#endif

#if LIB_PICO_STDIO_USB
#if SDK14
#define CONS_CONNECTED stdio_usb_connected()
#else
#include "tusb.h"
#define CONS_CONNECTED tud_cdc_connected()
#endif
#endif

#define COPYRIGHT "\u00a9" // for UTF8
//#define COPYRIGHT "(c)" // for ASCII

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

#define MAX_ARGS 16

#define VT_ESC "\033"
#define VT_CLEAR VT_ESC "[H" VT_ESC "[J"
#define VT_BLINK VT_ESC "[5m"
#define VT_BOLD VT_ESC "[1m"
#define VT_NORMAL VT_ESC "[m"

typedef char buf_t[128];

static uint32_t screen_x = 80, screen_y = 24;
static lfs_file_t file;
static buf_t cmd_buffer, curdir, path, result;
static int argc;
static char* argv[MAX_ARGS + 1];
static bool mounted = false, run = true;

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
    } else if (curdir[0] == 0) {
        strcpy(path, "/");
        strcat(path, name);
    } else {
        strcpy(path, curdir);
        if (name[0]) {
            strcat(path, "/");
            strcat(path, name);
        }
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

static void xmodem_cb(uint8_t* buf, uint32_t len) {
    if (fs_file_write(&file, buf, len) != len)
        printf("error writing file\n");
}

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

static void put_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (fs_file_open(&file, full_path(argv[1]), LFS_O_WRONLY | LFS_O_CREAT) < LFS_ERR_OK) {
        strcpy(result, "Can't create file");
        return;
    }
    set_translate_crlf(false);
    xmodemReceive(xmodem_cb);
    set_translate_crlf(true);
    int pos = fs_file_seek(&file, 0, LFS_SEEK_END);
    fs_file_close(&file);
    sprintf(result, "\nfile transfered, size: %d\n", pos);
}

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

static void cp_cmd(void) {
    char* from;
    char* to;
    char* buf = NULL;
    if (check_from_to_parms(&from, &to, 1))
        return;
    result[0] = 0;
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
        if (fs_file_open(&out, to, LFS_O_WRONLY | LFS_O_CREAT) < LFS_ERR_OK) {
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
        if (out_ok && fs_getattr(from, 1, buf, 3) == 3 && memcmp(buf, "exe", 3) == 0)
            fs_setattr(to, 1, buf, 3);
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
    if (check_name())
        return;
    lfs_file_t file;
    if (fs_file_open(&file, full_path(argv[1]), LFS_O_RDONLY) < LFS_ERR_OK) {
        strcpy(result, "error opening file");
        return;
    }
    int l = fs_file_seek(&file, 0, LFS_SEEK_END);
    fs_file_seek(&file, 0, LFS_SEEK_SET);
    char buf[256];
    while (l) {
        int l2 = l;
        if (l2 > sizeof(buf))
            l2 = sizeof(buf);
        if (fs_file_read(&file, buf, l2) != l2) {
            sprintf(result, "error reading file");
            break;
        }
        fwrite(buf, l2, 1, stdout);
        l -= l2;
    }
    fs_file_close(&file);
}

static void get_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (fs_file_open(&file, full_path(argv[1]), LFS_O_RDONLY) < LFS_ERR_OK) {
        strcpy(result, "Can't open file");
        return;
    }
    uint32_t len = fs_file_seek(&file, 0, LFS_SEEK_END);
    fs_file_rewind(&file);
    char* buf = malloc(len);
    if (buf == NULL) {
        strcpy(result, "not enough memory");
        goto err2;
    }
    if (fs_file_read(&file, buf, len) != len) {
        strcpy(result, "error reading file");
        goto err1;
    }
    set_translate_crlf(false);
    xmodemTransmit(buf, len);
    set_translate_crlf(true);
    printf("\nfile transfered, size: %d\n", len);
err1:
    free(buf);
err2:
    fs_file_close(&file);
    strcpy(result, "transfered");
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

static void status_cmd(void) {
    if (check_mount(true))
        return;
    struct fs_fsstat_t stat;
    fs_fsstat(&stat);
    const char percent = 37;
    int total_size = stat.block_count * stat.block_size;
#ifndef NDEBUG
    printf("\ntext size 0x%x, bss size 0x%x (%d)", stat.text_size, stat.bss_size, stat.bss_size);
#endif
    sprintf(result,
            "\nflash base 0x%x, blocks %d, block size %d, used %d, total %u bytes (%dK), %1.1f%c "
            "used.\n",
            fs_flash_base(), (int)stat.block_count, (int)stat.block_size, (int)stat.blocks_used,
            total_size, total_size / 1024, stat.blocks_used * 100.0 / stat.block_count, percent);
}

static void ls_cmd(void) {
    if (check_mount(true))
        return;
    if (argc > 1)
        full_path(argv[1]);
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
                printf(" %7d [%s]\n", info.size, info.name);
    fs_dir_rewind(&dir);
    while (fs_dir_read(&dir, &info) > 0)
        if (strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (info.type == LFS_TYPE_REG)
                printf(" %7d %s\n", info.size, info.name);
    fs_dir_close(&dir);
    result[0] = 0;
}

static void cd_cmd(void) {
    if (check_mount(true))
        return;
    if (argc < 2) {
        curdir[0] = 0;
        return;
    }
    if (strcmp(argv[1], "..") == 0) {
        if (curdir[0] == 0) {
            strcpy(result, "not a directory");
            return;
        }
        int i;
        for (i = strlen(curdir) - 1; i >= 0; i--)
            if (curdir[i] == '/')
                break;
        if (i < 0)
            i = 0;
        curdir[i] = 0;
        return;
    }
    full_path(argv[1]);
    lfs_dir_t dir;
    if (fs_dir_open(&dir, path) < LFS_ERR_OK) {
        strcpy(result, "not a directory");
        return;
    }
    fs_dir_close(&dir);
    strcpy(curdir, path);
    sprintf(result, "changed to %s", curdir);
}

static void cc_cmd(void) {
    if (check_mount(true))
        return;
    cc(argc, argv);
    result[0] = 0;
}

static void tar_cmd(void) {
    if (check_mount(true))
        return;
    tar(argc, argv);
    result[0] = 0;
}

static void vi_cmd(void) {
    if (check_mount(true))
        return;
    vi(argc - 1, argv + 1);
    result[0] = 0;
}

static void clear_cmd(void) {
    printf(VT_CLEAR);
    fflush(stdout);
    strcpy(result, VT_CLEAR "\n");
}

static void reboot_cmd(void) {
    // release any resources we were using
    if (mounted)
        fs_unmount();
    watchdog_reboot(0, 0, 1);
}

#if LIB_PICO_STDIO_USB
static void usbboot_cmd(void) {
    // release any resources we were using
    if (mounted)
        fs_unmount();
    reset_usb_boot(0, 0);
}
#endif

static void quit_cmd(void) {
    // release any resources we were using
    if (mounted)
        fs_unmount();
    printf("\nbye!\n");
    sleep_ms(1000);
    exit(0);
}

static void version_cmd(void) {
    const char* git_branch = STRINGIZE_VALUE_OF(GIT_BRANCH);
    const char* git_hash = STRINGIZE_VALUE_OF(GIT_COMMIT_HASH);
    printf("\nPico Shell v" PS_VERSION " [%s %s], LittleFS v%d.%d, Vi " VI_VER ", SDK %d.%d.%d\n",
           git_branch, git_hash, LFS_VERSION >> 16, LFS_VERSION & 0xffff, PICO_SDK_VERSION_MAJOR,
           PICO_SDK_VERSION_MINOR, PICO_SDK_VERSION_REVISION);
    result[0] = 0;
}

// clang-format off
cmd_t cmd_table[] = {
    {"cat",     cat_cmd,        "display text file"},
    {"cc",      cc_cmd,         "run C source file. cc -h for compiler help"},
    {"cd",      cd_cmd,         "change directory"},
    {"clear",   clear_cmd,      "clear the screen"},
    {"cp",      cp_cmd,         "copy a file"},
    {"format",  format_cmd,     "format the filesystem"},
    {"ls",      ls_cmd,         "list directory"},
    {"mkdir",   mkdir_cmd,      "create directory"},
    {"mount",   mount_cmd,      "mount filesystem"},
    {"mv",      mv_cmd,         "rename file or directory"},
    {"quit",    quit_cmd,       "shutdown system"},
    {"reboot",  reboot_cmd,     "Restart system"},
    {"rm",      rm_cmd,         "remove file or directory. -r for recursive"},
    {"status",  status_cmd,     "filesystem status"},
    {"tar",     tar_cmd,        "tar archiver"},
    {"unmount", unmount_cmd,    "unmount filesystem"},
	{"version", version_cmd,    "display pshel version"},
    {"vi",      vi_cmd,         "editor"},
    {"xget",    get_cmd,        "get file (xmodem)"},
    {"xput",    put_cmd,        "put file (xmodem)"},
	{0}
};
// clang-format on

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

static bool screen_size(void) {
    int rc = false;
    screen_x = 80;
    screen_y = 24;
    do {
        set_translate_crlf(false);
        printf(VT_ESC "[999;999H" VT_ESC "[6n");
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
        set_translate_crlf(true);
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
        screen_x = col;
        screen_y = row;
        rc = true;
    } while (false);
    return rc;
}

static void help(void) {
    printf("\n");
    for (int i = 0; cmd_table[i].name; i++)
        printf("%7s - %s\n", cmd_table[i].name, cmd_table[i].descr);
}

static void HardFault_Handler(void) {
    static const char* clear = "\n\n" VT_BOLD "*** " VT_BLINK "CRASH" VT_NORMAL VT_BOLD
                               " - Rebooting in 5 seconds ***" VT_NORMAL "\r\n\n";
    for (const char* cp = clear; *cp; cp++)
        putchar(*cp);
#ifndef NDEBUG
    for (;;)
        ;
#endif
    watchdog_reboot(0, 0, 5000);
    for (;;)
        __wfi();
}

// application entry point
int main(void) {
    // initialize the pico SDK
    stdio_init_all();
    bool uart = true;
#if LIB_PICO_STDIO_USB
    while (!CONS_CONNECTED)
        sleep_ms(1000);
    uart = false;
#endif
    ((int*)scb_hw->vtor)[3] = (int)HardFault_Handler;
    getchar_timeout_us(1000);
    bool detected = screen_size();
    printf(VT_CLEAR "\n" VT_BOLD "Pico Shell" VT_NORMAL " - Copyright " COPYRIGHT
                    " 1883 Thomas Edison\n"
                    "This program comes with ABSOLUTELY NO WARRANTY.\n"
                    "This is free software, and you are welcome to redistribute it\n"
                    "under certain conditions. See LICENSE file for details.\n");
    version_cmd();
    printf("\nconsole on %s [%u X %u]\n\n"
           "enter command or hit ENTER for help\n\n",
           uart ? "UART" : "USB", screen_x, screen_y);
    if (!detected) {
        printf("\nYour terminal does not respond to standard VT100 escape sequences"
               "\nsequences. The editor will likely not work at all!");
        fflush(stdout);
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
                printf("Error formating file system!\n");
            else {
                if (fs_mount() != LFS_ERR_OK)
                    printf("Error formating file system!\n");
                else {
                    printf("file system formatted and mounted\n");
                    mounted = true;
                }
            }
    } else {
        printf("file system automatically mounted\n");
        mounted = true;
    }
    while (run) {
        printf("\n" VT_BOLD "%s: " VT_NORMAL, full_path(""));
        fflush(stdout);
        parse_cmd();
        int i;
        result[0] = 0;
        bool found = false;
        if (argc) {
            if (!strcmp(argv[0], "q"))
                quit_cmd();
            for (i = 0; cmd_table[i].name; i++)
                if (strcmp(argv[0], cmd_table[i].name) == 0) {
                    cmd_table[i].func();
                    if (result[0])
                        printf("\n%s\n", result);
                    found = true;
                    break;
                }
            if (!found)
                printf("\nunknown command '%s'. hit ENTER for help\n", argv[0]);
        } else
            help();
    }
    printf("\ndone\n");
    sleep_ms(1000);
}
