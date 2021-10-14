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

#ifndef _HAL_
#define _HAL_

#include "lfs.h"

#ifdef __cplusplus
extern "C" {
#endif

// utility functions

struct fs_fsstat_t {
    lfs_size_t block_size;
    lfs_size_t block_count;
    lfs_size_t blocks_used;
};

// implemented
int fs_flash_base(void);

extern lfs_t fs_lfs;
extern struct lfs_config fs_cfg;

static inline int fs_format(void) { return lfs_format(&fs_lfs, &fs_cfg); }

static inline int fs_mount(void) { return lfs_mount(&fs_lfs, &fs_cfg); }

static inline int fs_unmount(void) { return lfs_unmount(&fs_lfs); }

static inline int fs_remove(const char* path) { return lfs_remove(&fs_lfs, path); }

static inline int fs_rename(const char* oldpath, const char* newpath) {
    return lfs_rename(&fs_lfs, oldpath, newpath);
}

static inline int fs_stat(const char* path, struct lfs_info* info) {
    return lfs_stat(&fs_lfs, path, info);
}

static inline lfs_ssize_t fs_getattr(const char* path, uint8_t type, void* buffer,
                                     lfs_size_t size) {
    return lfs_getattr(&fs_lfs, path, type, buffer, size);
}

static inline int fs_setattr(const char* path, uint8_t type, const void* buffer, lfs_size_t size) {
    return lfs_setattr(&fs_lfs, path, type, buffer, size);
}

static inline int fs_removeattr(const char* path, uint8_t type) {
    return lfs_removeattr(&fs_lfs, path, type);
}

static inline int fs_file_open(lfs_file_t* file, const char* path, int flags) {
    return lfs_file_open(&fs_lfs, file, path, flags);
}

static inline int fs_file_opencfg(lfs_file_t* file, const char* path, int flags,
                                  const struct lfs_file_config* config) {
    return lfs_file_opencfg(&fs_lfs, file, path, flags, config);
}

static inline int fs_file_close(lfs_file_t* file) { return lfs_file_close(&fs_lfs, file); }

static inline int fs_file_sync(lfs_file_t* file) { return lfs_file_close(&fs_lfs, file); }

static inline lfs_ssize_t fs_file_read(lfs_file_t* file, void* buffer, lfs_size_t size) {
    return lfs_file_read(&fs_lfs, file, buffer, size);
}

static inline lfs_ssize_t fs_file_write(lfs_file_t* file, const void* buffer, lfs_size_t size) {
    return lfs_file_write(&fs_lfs, file, buffer, size);
}

static inline lfs_soff_t fs_file_seek(lfs_file_t* file, lfs_soff_t off, int whence) {
    return lfs_file_seek(&fs_lfs, file, off, whence);
}

static inline int fs_file_truncate(lfs_file_t* file, lfs_off_t size) {
    return lfs_file_truncate(&fs_lfs, (lfs_file_t*)file, size);
}

static inline lfs_soff_t fs_file_tell(lfs_file_t* file) { return lfs_file_tell(&fs_lfs, file); }

static inline int fs_file_rewind(lfs_file_t* file) { return lfs_file_rewind(&fs_lfs, file); }

static inline lfs_soff_t fs_file_size(lfs_file_t* file) { return lfs_file_size(&fs_lfs, file); }

static inline int fs_mkdir(const char* path) { return lfs_mkdir(&fs_lfs, path); }

static inline int fs_dir_open(lfs_dir_t* dir, const char* path) {
    return lfs_dir_open(&fs_lfs, dir, path);
}

static inline int fs_dir_close(lfs_dir_t* dir) { return lfs_dir_close(&fs_lfs, dir); }

static inline int fs_dir_read(lfs_dir_t* dir, struct lfs_info* info) {
    return lfs_dir_read(&fs_lfs, dir, info);
}

static inline int fs_dir_seek(lfs_dir_t* dir, lfs_off_t off) {
    return lfs_dir_seek(&fs_lfs, dir, off);
}

static inline lfs_soff_t fs_dir_tell(lfs_dir_t* dir) { return lfs_dir_tell(&fs_lfs, dir); }

static inline int fs_dir_rewind(lfs_dir_t* dir) { return lfs_dir_rewind(&fs_lfs, dir); }

static inline lfs_ssize_t fs_fs_size(void) { return lfs_fs_size(&fs_lfs); }

int fs_fsstat(struct fs_fsstat_t* stat);

#ifdef __cplusplus
}
#endif

#endif // _HAL_
