/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File operations, in a separate file because these are probably
|*|   going to cause the most portability problems.
\*/

#ifndef FILEOPS_H
#define FILEOPS_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "types.h"

struct hfile_s {
	hfile_t *next;
	md5 hash_16k;
	md5 hash;
	i64 file_size;
	u16 filename[FILENAME_MAX];
	u8 hashed;
};

#define HASH16K 1
#define HASH 2

void unistr(const char *str, u16 *buf);
u16 *make_uni_str(const char *str);
char *stuni(const u16 *str);
char *stmd5(const md5 hash);
size_t uni_copy(u16 *dst, u16 *src, size_t n);
int file_rename(u16 *src, u16 *dst);
file_t file_open(const u16 *path, int wr);
file_t file_open_ascii(const char *path, int wr);
int file_close(file_t f);
int file_exists(u16 *file);
int file_delete(u16 *file);
int file_seek(file_t f, i64 off);
ssize_t file_md5(u16 *file, md5 block);
int file_md5_16k(u16 *file, md5 block);
int file_add_md5(file_t f, i64 md5off, i64 off, i64 len);
int file_get_md5(file_t f, i64 off, md5 block);
hfile_t *read_dir(char *dir);
i64 file_read(file_t f, void *buf, i64 n);
i64 file_write(file_t f, void *buf, i64 n);

#endif
