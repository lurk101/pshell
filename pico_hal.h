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

void hal_start(void);

float hal_elapsed(void);

// posix emulation

extern int pico_errno;

struct pico_fsstat_t {
    lfs_size_t block_size;
    lfs_size_t block_count;
    lfs_size_t blocks_used;
};

// implemented
int pico_flash_base(void);
int pico_mount(void);
int pico_format(void);
int pico_unmount(void);
int pico_remove(const char* path);
int pico_open(const char* path, int flags);
int pico_close(int file);
int pico_fsstat(struct pico_fsstat_t* stat);
int pico_rewind(int file);
int pico_rename(const char* oldpath, const char* newpath);
lfs_size_t pico_read(int file, void* buffer, lfs_size_t size);
lfs_size_t pico_write(int file, const void* buffer, lfs_size_t size);
lfs_soff_t pico_lseek(int file, lfs_soff_t off, int whence);
int pico_truncate(int file, lfs_off_t size);
lfs_soff_t pico_tell(int file);

// untested
int pico_stat(const char* path, struct lfs_info* info);
lfs_ssize_t pico_getattr(const char* path, uint8_t type, void* buffer, lfs_size_t size);
int pico_setattr(const char* path, uint8_t type, const void* buffer, lfs_size_t size);
int pico_removeattr(const char* path, uint8_t type);
int pico_opencfg(int file, const char* path, int flags, const struct lfs_file_config* config);
int pico_fflush(int file);
lfs_soff_t pico_size(int file);
int pico_mkdir(const char* path);
int pico_dir_open(lfs_dir_t* dir, const char* path);
int pico_dir_close(lfs_dir_t* dir);
int pico_dir_read(lfs_dir_t* dir, struct lfs_info* info);
int pico_dir_seek(lfs_dir_t* dir, lfs_off_t off);
lfs_soff_t pico_dir_tell(lfs_dir_t* dir);
int pico_dir_rewind(lfs_dir_t* dir);
int pico_fs_traverse(int (*cb)(void*, lfs_block_t), void* data);

#ifdef __cplusplus
}
#endif

#endif // _HAL_
