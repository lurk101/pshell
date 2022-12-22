/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

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

#include "fs.h"

// file system offset in flash
#define FS_BASE (256 * 1024)

static int fs_hal_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer,
                       lfs_size_t size);

static int fs_hal_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                       const void* buffer, lfs_size_t size);

static int fs_hal_erase(const struct lfs_config* c, lfs_block_t block);

static int fs_hal_sync(const struct lfs_config* c);

#define FS_SIZE (PICO_FLASH_SIZE_BYTES - FS_BASE)

// configuration of the filesystem is provided by this struct
// for Pico: prog size = 256, block size = 4096, so cache is 8K
// minimum cache = block size, must be multiple
struct lfs_config fs_cfg = {
    // block device operations
    .read = fs_hal_read,
    .prog = fs_hal_prog,
    .erase = fs_hal_erase,
    .sync = fs_hal_sync,
    // block device configuration
    .read_size = 1,
    .prog_size = FLASH_PAGE_SIZE,
    .block_size = FLASH_SECTOR_SIZE,
    .block_count = FS_SIZE / FLASH_SECTOR_SIZE,
    .cache_size = FLASH_SECTOR_SIZE / 4,
    .lookahead_size = 32,
    .block_cycles = 256,
};

lfs_t fs_lfs;

// Pico specific hardware abstraction functions

static int fs_hal_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer,
                       lfs_size_t size) {
    (void)c;
    // read flash via XIP mapped space
    uint8_t* p = (uint8_t*)(XIP_NOCACHE_NOALLOC_BASE + FS_BASE + (block * fs_cfg.block_size) + off);
    memcpy(buffer, p, size);
    return LFS_ERR_OK;
}

static int fs_hal_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                       const void* buffer, lfs_size_t size) {
    (void)c;
    uint32_t p = (block * fs_cfg.block_size) + off;
    // program with SDK
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(FS_BASE + p, buffer, size);
    restore_interrupts(ints);
    return LFS_ERR_OK;
}

static int fs_hal_erase(const struct lfs_config* c, lfs_block_t block) {
    uint32_t off = block * fs_cfg.block_size;
    (void)c;
    // erase with SDK
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FS_BASE + off, fs_cfg.block_size);
    restore_interrupts(ints);
    return LFS_ERR_OK;
}

static int fs_hal_sync(const struct lfs_config* c) {
    (void)c;
    // nothing to do!
    return LFS_ERR_OK;
}

#ifndef NDEBUG
extern char __HeapLimit;
extern char __flash_binary_end;
#endif

int fs_fsstat(struct fs_fsstat_t* stat) {
    stat->block_count = fs_cfg.block_count;
    stat->block_size = fs_cfg.block_size;
    stat->blocks_used = lfs_fs_size(&fs_lfs);
#ifndef NDEBUG
    stat->text_size = (lfs_size_t)&__flash_binary_end - 0x10000000;
    stat->bss_size = (lfs_size_t)&__HeapLimit - 0x20000000;
#endif
    return LFS_ERR_OK;
}

int fs_flash_base(void) { return FS_BASE; }
