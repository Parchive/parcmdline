/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File operations, in a separate file because these are probably
|*|   going to cause the most portability problems.
\*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include "md5.h"
#include "fileops.h"
#include "util.h"

/*\
|*| Translate an ASCII string into unicode, write it onto the buffer
\*/
void
unistr(const char *str, u16 *buf)
{
	do *buf++ = *str; while (*str++);
}

/*\
|*| Translate an ASCII string into unicode
|*| Returns an allocated string
\*/
u16 *
make_uni_str(const char *str)
{
	u16 *uni;

	NEW(uni, strlen(str) +1);
	unistr(str, uni);
	return uni;
}

/*\
|*| Translate a unicode string into ASCII
|*| Returns static string, which is overwritten at next call
\*/
char *
stuni(const u16 *str)
{
	i64 i;
	static char *buf = 0;
	static i64 bufsize = 0;

	/*\ Count the length \*/
	for (i = 0; str[i]; i++)
		;
	if ((i + 1) > bufsize) {
		bufsize = i + 1;
		RENEW(buf, bufsize);
	}
	/*\ For now, just copy the low byte \*/
	for (i = 0; str[i]; i++)
		buf[i] = str[i];
	buf[i] = 0;
	return buf;
}

/*\
|*| Translate an md5 hash into ASCII
|*| Returns static string, which is overwritten at next call
\*/
char *
stmd5(const md5 hash)
{
	int i;
	static char buf[33];

	for (i = 0; i < 16; i++) {
		buf[i * 2] = HEXDIGIT((hash[i] >> 4) & 0xF);
		buf[i * 2 + 1] = HEXDIGIT(hash[i] & 0xF);
	}
	buf[i * 2] = 0;
	return buf;
}

i64
uni_copy(u16 *dst, u16 *src, i64 n)
{
	i64 i;
	for (i = 0; src[i] && (i < (n - 1)); i++)
		dst[i] = src[i];
	dst[i] = 0;
	return i;
}

int
file_rename(u16 *src, u16 *dst)
{
	int i;
	char *s, *d;

	s = stuni(dst);
	NEW(d, strlen(s) + 1);
	strcpy(d, s);
	s = stuni(src);
	i = rename(s, d);
	free(d);
	return i;
}

file_t
file_open_ascii(const char *path, int wr)
{
	int i;
	/*\ This is so complicated to make sure we don't overwrite \*/
	i = open(path, wr ? O_RDWR|O_CREAT|O_EXCL : O_RDONLY, 0666);
	if (i < 0) return 0;
	return fdopen(i, wr ? "w+b" : "rb");
}

file_t
file_open(const u16 *path, int wr)
{
	return file_open_ascii(stuni(path), wr);
}

int
file_close(file_t f)
{
	return (f ? fclose(f) : 0);
}

int
file_exists(u16 *file)
{
	file_t f;
	f = file_open(file, 0);
	if (!f) return (errno != ENOENT);
	file_close(f);
	return 1;
}

int
file_delete(u16 *file)
{
	return remove(stuni(file));
}

int
file_seek(file_t f, i64 off)
{
	return fseek(f, off, SEEK_SET);
}

/*\ Read a directory.
|*|  Returns a linked list of file entries.
\*/
hfile_t *
read_dir(char *dir)
{
	DIR *d;
	struct dirent *de;
	hfile_t *rd = 0, **rdptr = &rd;

	d = opendir(dir);
	if (!d) return 0;

	while ((de = readdir(d))) {
		CNEW(*rdptr, 1);
		unistr(de->d_name, (*rdptr)->filename);
		rdptr = &((*rdptr)->next);
	}
	closedir(d);

	return rd;
}

i64
file_read(file_t f, void *buf, i64 n)
{
	if (!f) return 0;
	return fread(buf, 1, n, f);
}

i64
file_write(file_t f, void *buf, i64 n)
{
	if (!f) return 0;
	return fwrite(buf, 1, n, f);
}

/*\ Calculate md5 sums on a file \*/
i64
file_md5(u16 *file, md5 block)
{
	FILE *f;
	i64 i;

	f = fopen(stuni(file), "rb");
	if (!f) return 0;
	i = md5_stream(f, block);
	fclose(f);
	return i;
}

int
file_md5_16k(u16 *file, md5 block)
{
	u8 buf[16384];
	file_t f;
	i64 s;

	f = file_open(file, 0);
	if (!f) return 0;
	s = file_read(f, buf, sizeof(buf));
	file_close(f);
	if (s < 0) return 0;
	return (md5_buffer(buf, s, block) != 0);
}

/*\ Calculate the md5sum of a file from offset 'off',
|*| put it at offset 'md5off'
|*| Also, check the file size.  Return 0 on failure.
\*/
int
file_add_md5(file_t f, i64 md5off, i64 off, i64 len)
{
	md5 hash;

	if (file_seek(f, off) < 0)
		return 0;
	if (md5_stream(f, hash) < 0)
		return 0;
	/*\ Filepointer should be at EOF now \*/
	if ((i64)ftell(f) != len)
		return 0;
	if (file_seek(f, md5off) < 0)
		return 0;
	file_write(f, hash, sizeof(hash));
	return 1;
}

/*\ Get the md5sum over part of a file.  \*/
int
file_get_md5(file_t f, i64 off, md5 block)
{
	if (file_seek(f, off) < 0)
		return 0;
	return (md5_stream(f, block) != 0);
}

