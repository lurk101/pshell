/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the BSD 3 clause license, which unfortunately
 * won't be written for another century.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A little flash file system manager for the Raspberry Pico
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "pico_hal.h"
#include "tusb.h"
#include "xreceive.h"
#include "xtransmit.h"

#define MAX_ARGS 2

#define VT_CLEAR "\033[H\033[J"
#define VT_BELL "\007"

static int file;
static char cmd_buffer[128];
static int argc;
static char* argv[MAX_ARGS];
static char curdir[128] = {0};
static bool mounted = false;
static char path[128];
static char emsg[128];

static void parse_cmd(void) {
    // read line into buffer
    char* cp = cmd_buffer;
    char* cp_end = cp + sizeof(cmd_buffer);
    char c;
    do {
        c = getchar();
        if (c == '\b') {
            if (cp != cmd_buffer) {
                cp--;
                printf(" \b");
                fflush(stdout);
            }
        } else if (cp < cp_end)
            *cp++ = c;
    } while ((c != '\r') && (c != '\n'));
    // parse buffer
    cp = cmd_buffer;
    bool not_last = true;
    for (argc = 0; not_last && (argc < MAX_ARGS); argc++) {
        while ((*cp == ' ') || (*cp == ','))
            cp++; // skip blanks
        if ((*cp == '\r') || (*cp == '\n'))
            break;
        argv[argc] = cp; // start of string
        while ((*cp != ' ') && (*cp != ',') && (*cp != '\r') && (*cp != '\n'))
            cp++; // skip non blank
        if ((*cp == '\r') || (*cp == '\n'))
            not_last = false;
        *cp++ = 0; // terminate string
    }
}

static char* full_path(char* name) {
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

static void xmodem_cb(uint8_t* buf, uint32_t len) {
    if (pico_write(file, buf, len) != len)
        printf("error writing file\n");
}

typedef void (*cmd_func_t)(void);

typedef struct {
    int match;
    const char* name;
    cmd_func_t func;
    const char* descr;
} cmd_t;

static bool check_mount(bool need) {
    if (mounted == need)
        return false;
    sprintf(emsg, "filesystem is %s mounted", (need ? "not" : "already"));
    return true;
}

static bool check_name(void) {
    if (argc > 1)
        return false;
    strcpy(emsg, "missing file or directory name");
    return true;
}

static void put_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    file = pico_open(full_path(argv[1]), LFS_O_WRONLY | LFS_O_CREAT);
    if (file < 0) {
        strcpy(emsg, "Can't create file");
        return;
    }
    stdio_set_translate_crlf(&stdio_uart, false);
    xmodemReceive(xmodem_cb);
    stdio_set_translate_crlf(&stdio_uart, true);
    int pos = pico_lseek(file, 0, LFS_SEEK_END);
    pico_close(file);
    printf("\nfile transfered, size: %d\n", pos);
}

static void get_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    file = pico_open(full_path(argv[1]), LFS_O_RDONLY);
    if (file < 0) {
        strcpy(emsg, "Can't open file");
        return;
    }
    uint32_t len = pico_lseek(file, 0, LFS_SEEK_END);
    pico_rewind(file);
    char* buf = malloc(len);
    if (buf == NULL) {
        strcpy(emsg, "not enough memory");
        goto err2;
    }
    if (pico_read(file, buf, len) != len) {
        strcpy(emsg, "error reading file");
        goto err1;
    }
    stdio_set_translate_crlf(&stdio_uart, false);
    xmodemTransmit(buf, len);
    stdio_set_translate_crlf(&stdio_uart, true);
    printf("\nfile transfered, size: %d\n", len);
err1:
    free(buf);
err2:
    pico_close(file);
}

static void md_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (pico_mkdir(full_path(argv[1])) < 0) {
        strcpy(emsg, "Can't create directory");
        return;
    }
}

static void rm_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (pico_remove(full_path(argv[1])) < 0)
        strcpy(emsg, "Can't remove file or directory");
}

static void mount_cmd(void) {
    if (check_mount(false))
        return;
    if (pico_mount() != LFS_ERR_OK) {
        strcpy(emsg, "Error mounting filesystem");
        return;
    }
    mounted = true;
}

static void unmount_cmd(void) {
    if (check_mount(true))
        return;
    if (pico_unmount() != LFS_ERR_OK) {
        strcpy(emsg, "Error unmounting filesystem");
        return;
    }
    mounted = false;
}

static void format_cmd(void) {
    if (check_mount(false))
        return;
    printf(VT_BELL "are you sure (y/N) ? ");
    fflush(stdout);
    parse_cmd();
    if ((argc == 0) || ((argv[0][0] | ' ') != 'y')) {
        strcpy(emsg, "user cancelled");
        return;
    }
    if (pico_format() != LFS_ERR_OK)
        strcpy(emsg, "Error formating filesystem");
}

static void status_cmd(void) {
    if (check_mount(true))
        return;
    struct pico_fsstat_t stat;
    pico_fsstat(&stat);
    const char percent = 37;
    printf(
        "\nflash base 0x%08x, blocks %d, block size %d, used %d, total %d bytes, %1.1f%c used.\n\n",
        pico_flash_base(), (int)stat.block_count, (int)stat.block_size, (int)stat.blocks_used,
        stat.block_count * stat.block_size, stat.blocks_used * 100.0 / stat.block_count, percent);
}

static void ls_cmd(void) {
    if (check_mount(true))
        return;
    if (argc > 1)
        full_path(argv[1]);
    else
        full_path("");
    lfs_dir_t dir;
    if (pico_dir_open(&dir, path) < 0) {
        strcpy(emsg, "not a directory");
        return;
    }
    printf("\n   size name\n");
    struct lfs_info info;
    while (pico_dir_read(&dir, &info) > 0)
        if (strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (info.type == LFS_TYPE_DIR)
                printf("  %5d [%s]\n", info.size, info.name);
    pico_dir_rewind(&dir);
    while (pico_dir_read(&dir, &info) > 0)
        if (strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (info.type == LFS_TYPE_REG)
                printf("  %5d %s\n", info.size, info.name);
    pico_dir_close(&dir);
    printf("\n");
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
            strcpy(emsg, "not a directory");
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
    if (pico_dir_open(&dir, path) < 0) {
        strcpy(emsg, "not a directory");
        return;
    }
    pico_dir_close(&dir);
    strcpy(curdir, path);
}

static void quit_cmd(void) {
    // release any resources we were using
    if (mounted)
        pico_unmount();
    printf("done\n");
    exit(0);
}

static bool via_uart = true;

void stdio_init(int uart_rx_pin) {
    gpio_init(uart_rx_pin);
    gpio_set_pulls(uart_rx_pin, 1, 0);
    sleep_ms(1);
    bool v1 = gpio_get(uart_rx_pin);
    gpio_set_pulls(uart_rx_pin, 0, 1);
    sleep_ms(1);
    bool v2 = gpio_get(uart_rx_pin);
    gpio_set_pulls(uart_rx_pin, 0, 0);
    if (v1 != v2) {
        via_uart = false;
        stdio_usb_init();
        while (!tud_cdc_connected())
            sleep_ms(1000);
    } else {
        stdio_uart_init();
        getchar_timeout_us(1000);
    }
}

static cmd_t commands[] = {{1, "cd", cd_cmd, "change directory"},
                           {1, "format", format_cmd, "format and mount the filesystem"},
                           {1, "get", get_cmd, "get file"},
                           {1, "ls", ls_cmd, "list directory"},
                           {2, "mkdir", md_cmd, "create directory"},
                           {2, "mount", mount_cmd, "mount filesystem"},
                           {1, "put", put_cmd, "put file"},
                           {1, "q", quit_cmd, "quit"},
                           {1, "rm", rm_cmd, "remove file or directory"},
                           {1, "status", status_cmd, "filesystem status"},
                           {1, "unmount", unmount_cmd, "unmount filesystem"}};

// application entry point
int main(void) {

    // initialize the pico SDK
    stdio_init(PICO_DEFAULT_UART_RX_PIN);
    printf(VT_CLEAR "connected on %s\n\n"
                    "lfstool  Copyright (C) 1883 Thomas Edison\n"
                    "This program comes with ABSOLUTELY NO WARRANTY.\n"
                    "This is free software, and you are welcome to redistribute it\n"
                    "under certain conditions. See LICENSE.md for details.\n\n"
                    "enter command, hit return for help\n",
           (via_uart ? "UART" : "USB"));
    for (;;) {
        printf("%s: ", full_path(""));
        fflush(stdout);
        parse_cmd();
        bool found = false;
        int i;
        emsg[0] = 0;
        if (argc)
            for (i = 0; i < sizeof commands / sizeof commands[0]; i++) {
                int l = strlen(argv[0]);
                if ((l >= commands[i].match) &&
                    (memcmp(argv[0], commands[i].name, commands[i].match) == 0)) {
                    commands[i].func();
                    found = true;
                    break;
                }
            }
        if (!found) {
            printf(VT_BELL "\n");
            for (int i = 0; i < sizeof commands / sizeof commands[0]; i++)
                printf("%7s - %s\n", commands[i].name, commands[i].descr);
            printf("\n");
            continue;
        }
        printf("%s%s: %s\n", (emsg[0] ? VT_BELL : ""), commands[i].name, (emsg[0] ? emsg : "ok"));
    }
}
