/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the BSD 3 clause license, which unfortunately
 * won't be written for another century.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A little flash file system for the Raspberry Pico
 *
 */

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"
#include "pico/time.h"

#include "pico_hal.h"

#define FS_SIZE (1024 * 1024)

static int pico_hal_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer,
                         lfs_size_t size);

static int pico_hal_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                         const void* buffer, lfs_size_t size);

static int pico_hal_erase(const struct lfs_config* c, lfs_block_t block);

static int pico_hal_sync(const struct lfs_config* c);

// configuration of the filesystem is provided by this struct
// for Pico: prog size = 256, block size = 4096, so cache is 8K
// minimum cache = block size, must be multiple
struct lfs_config pico_cfg = {
    // block device operations
    .read = pico_hal_read,
    .prog = pico_hal_prog,
    .erase = pico_hal_erase,
    .sync = pico_hal_sync,
    // block device configuration
    .read_size = 1,
    .prog_size = FLASH_PAGE_SIZE,
    .block_size = FLASH_SECTOR_SIZE,
    .block_count = FS_SIZE / FLASH_SECTOR_SIZE,
    .cache_size = FLASH_SECTOR_SIZE / 4,
    .lookahead_size = 32,
    .block_cycles = 256,
};

lfs_t pico_lfs;

// Pico specific hardware abstraction functions

// file system offset in flash
#define FS_BASE (PICO_FLASH_SIZE_BYTES - FS_SIZE)

static int pico_hal_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer,
                         lfs_size_t size) {
    (void)c;
    // read flash via XIP mapped space
    uint8_t* p =
        (uint8_t*)(XIP_NOCACHE_NOALLOC_BASE + FS_BASE + (block * pico_cfg.block_size) + off);
    memcpy(buffer, p, size);
    return LFS_ERR_OK;
}

static int pico_hal_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                         const void* buffer, lfs_size_t size) {
    (void)c;
    uint32_t p = (block * pico_cfg.block_size) + off;
    // program with SDK
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(FS_BASE + p, buffer, size);
    restore_interrupts(ints);
    return LFS_ERR_OK;
}

static int pico_hal_erase(const struct lfs_config* c, lfs_block_t block) {
    uint32_t off = block * pico_cfg.block_size;
    (void)c;
    // erase with SDK
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FS_BASE + off, pico_cfg.block_size);
    restore_interrupts(ints);
    return LFS_ERR_OK;
    return LFS_ERR_OK;
}

static int pico_hal_sync(const struct lfs_config* c) {
    (void)c;
    // nothing to do!
    return LFS_ERR_OK;
}

// utility functions

static uint32_t tm;

void hal_start(void) { tm = time_us_32(); }

float hal_elapsed(void) { return (time_us_32() - tm) / 1000000.0; }

// posix emulation

int pico_errno;

int pico_mount(void) { return lfs_mount(&pico_lfs, &pico_cfg); }

int pico_format(void) { return lfs_format(&pico_lfs, &pico_cfg); }

int pico_open(const char* path, int flags) {
    lfs_file_t* file = lfs_malloc(sizeof(lfs_file_t));
    if (file == NULL)
        return -1;
    int err = lfs_file_open(&pico_lfs, file, path, flags);
    if (err != LFS_ERR_OK) {
        pico_errno = err;
        return -1;
    }
    return (int)file;
}

int pico_close(int file) {
    return lfs_file_close(&pico_lfs, (lfs_file_t*)file);
    lfs_free((lfs_file_t*)file);
}

lfs_size_t pico_write(int file, const void* buffer, lfs_size_t size) {
    return lfs_file_write(&pico_lfs, (lfs_file_t*)file, buffer, size);
}

lfs_size_t pico_read(int file, void* buffer, lfs_size_t size) {
    return lfs_file_read(&pico_lfs, (lfs_file_t*)file, buffer, size);
}

int pico_rewind(int file) { return lfs_file_rewind(&pico_lfs, (lfs_file_t*)file); }

int pico_unmount(void) { return lfs_unmount(&pico_lfs); }

int pico_remove(const char* path) { return lfs_remove(&pico_lfs, path); }

int pico_rename(const char* oldpath, const char* newpath) {
    return lfs_rename(&pico_lfs, oldpath, newpath);
}

int pico_fsstat(struct pico_fsstat_t* stat) {
    stat->block_count = pico_cfg.block_count;
    stat->block_size = pico_cfg.block_size;
    stat->blocks_used = lfs_fs_size(&pico_lfs);
    return LFS_ERR_OK;
}

lfs_soff_t pico_lseek(int file, lfs_soff_t off, int whence) {
    return lfs_file_seek(&pico_lfs, (lfs_file_t*)file, off, whence);
}

int pico_truncate(int file, lfs_off_t size) {
    return lfs_file_truncate(&pico_lfs, (lfs_file_t*)file, size);
}

lfs_soff_t pico_tell(int file) { return lfs_file_tell(&pico_lfs, (lfs_file_t*)file); }

int pico_stat(const char* path, struct lfs_info* info) { return lfs_stat(&pico_lfs, path, info); }

lfs_ssize_t pico_getattr(const char* path, uint8_t type, void* buffer, lfs_size_t size) {
    return lfs_getattr(&pico_lfs, path, type, buffer, size);
}

int pico_setattr(const char* path, uint8_t type, const void* buffer, lfs_size_t size) {
    return lfs_setattr(&pico_lfs, path, type, buffer, size);
}

int pico_removeattr(const char* path, uint8_t type) {
    return lfs_removeattr(&pico_lfs, path, type);
}

int pico_opencfg(int file, const char* path, int flags, const struct lfs_file_config* config) {
    return lfs_file_opencfg(&pico_lfs, (lfs_file_t*)file, path, flags, config);
}

int pico_fflush(int file) { return lfs_file_sync(&pico_lfs, (lfs_file_t*)file); }

lfs_soff_t pico_size(int file) { return lfs_file_size(&pico_lfs, (lfs_file_t*)file); }

int pico_mkdir(const char* path) { return lfs_mkdir(&pico_lfs, path); }

int pico_dir_open(lfs_dir_t* dir, const char* path) { return lfs_dir_open(&pico_lfs, dir, path); }

int pico_dir_close(lfs_dir_t* dir) { return lfs_dir_close(&pico_lfs, (lfs_dir_t*)dir); }

int pico_dir_read(lfs_dir_t* dir, struct lfs_info* info) {
    return lfs_dir_read(&pico_lfs, dir, info);
}

int pico_dir_seek(lfs_dir_t* dir, lfs_off_t off) {
    return lfs_dir_seek(&pico_lfs, (lfs_dir_t*)dir, off);
}

lfs_soff_t pico_dir_tell(lfs_dir_t* dir) { return lfs_dir_tell(&pico_lfs, (lfs_dir_t*)dir); }

int pico_dir_rewind(lfs_dir_t* dir) { return lfs_dir_rewind(&pico_lfs, (lfs_dir_t*)dir); }

int pico_fs_traverse(int (*cb)(void*, lfs_block_t), void* data) {
    return lfs_fs_traverse(&pico_lfs, cb, data);
}

int pico_flash_base(void) { return FS_BASE; }
