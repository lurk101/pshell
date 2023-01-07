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

#include "io.h"
#include "sd_spi.h"

// file system offset in flash

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
    .read_size = 512,
    .prog_size = 512,
    .block_size = 512,
    //  .block_count = ?,
    .cache_size = 512,
    .lookahead_size = 32,
    .block_cycles = 256,
};

lfs_t fs_lfs;

// Pico specific hardware abstraction functions

int fs_load(void) {
    if (!sd_spi_init())
        return LFS_ERR_IO;
    int sec = sd_spi_sectors();
    if (sec == 0)
        return LFS_ERR_IO;
    fs_cfg.block_count = sec / (fs_cfg.block_size / fs_cfg.prog_size);
    return LFS_ERR_OK;
}

int fs_unload(void) { return LFS_ERR_OK; }

static int fs_hal_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer,
                       lfs_size_t size) {
    if ((size % c->block_size) != 0)
        return LFS_ERR_IO;
    int lba = block;
    char* buf = (char*)buffer;
    while (size) {
        if (!sd_spi_read(block++, buffer))
            return LFS_ERR_IO;
        buf += c->block_size;
        size -= c->block_size;
    }
    return LFS_ERR_OK;
}

static int fs_hal_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                       const void* buffer, lfs_size_t size) {
    if ((size % c->block_size) != 0)
        return LFS_ERR_IO;
    int lba = block;
    char* buf = (char*)buffer;
    while (size) {
        if (!sd_spi_write(block++, buffer))
            return LFS_ERR_IO;
        buf += c->block_size;
        size -= c->block_size;
    }
    return LFS_ERR_OK;
}

static int fs_hal_erase(const struct lfs_config* c, lfs_block_t block) {
    (void)c;
    (void)block;
    return LFS_ERR_OK;
}

static int fs_hal_sync(const struct lfs_config* c) {
    (void)c;
    // nothing to do!
    return LFS_ERR_OK;
}

int fs_fsstat(struct fs_fsstat_t* stat) {
    stat->block_count = fs_cfg.block_count;
    stat->block_size = fs_cfg.block_size;
    stat->blocks_used = lfs_fs_size(&fs_lfs);
#ifndef NDEBUG
    stat->text_size = 0;
    stat->bss_size = 0;
#endif
    return LFS_ERR_OK;
}
