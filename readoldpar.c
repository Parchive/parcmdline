/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Read old version PAR and PXX files
\*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "par.h"
#include "util.h"
#include "fileops.h"
#include "rs.h"
#include "backend.h"

/*\ Endianless fixing code \*/

static i64
read_i64(void *data)
{
	int i;
	i64 r = 0;
	u8 *ptr = data;

	for (i = sizeof(i64); --i >= 0; ) {
		r <<= 8;
		r += (i64)ptr[i];
	}
	return r;
}

static u32
read_u32(void *data)
{
	int i;
	u32 r = 0;
	u8 *ptr = data;

	for (i = sizeof(u32); --i >= 0; ) {
		r <<= 8;
		r += (u32)ptr[i];
	}
	return r;
}

static u16
read_u16(void *data)
{
	int i;
	u16 r = 0;
	u8 *ptr = data;

	for (i = sizeof(u16); --i >= 0; ) {
		r <<= 8;
		r += (u16)ptr[i];
	}
	return r;
}

/*\ Read N bytes of little-endian u16s \*/
static void
read_u16s(u16 *str, void *data, i64 n)
{
	u8 *ptr = data;
	while (--n >= 0) {
		*str = ptr[0] + (ptr[1] << 8);
		str++;
		ptr += 2;
	}
}

/*\ Check if something is an old style PAR file \*/
int
is_old_par(void *data)
{
	if (!memcmp(data, "PAR", 4))
		return 1;
	if (!memcmp(data, "PXX", 4))
		return 2;
	return 0;
}

/*\
|*| Read in a PAR file entry to a file struct
\*/
static i64
read_old_pfile(pfile_t *file, u8 *ptr, u16 *path, i64 pl)
{
	i64 i, l;

	file->status = read_i64(ptr + 0x08);
	file->file_size = read_i64(ptr + 0x10);
	COPY(file->hash, ptr + 0x28, sizeof(md5));
	COPY(file->hash_16k, ptr + 0x18, sizeof(md5));
	for (i = 0; ptr[0x3A + i] || ptr[0x3A + i + 1]; i += 2)
		;
	l = pl + i;
	NEW(file->filename, l);
	COPY(file->filename, path, pl);
	read_u16s(file->filename + pl, ptr + 0x3A, i);

	return read_i64(ptr);
}

static pfile_t *
read_old_pfiles(file_t f, i64 size, u16 *path)
{
	pfile_t *files = 0, **fptr = &files;
	u8 *buf;
	i64 i, pl;

	for (pl = i = 0; path[i]; i++)
		if (path[i] == DIR_SEP)
			pl = i + 1;

	NEW(buf, size);
	size = file_read(f, buf, size);

	/*\ The list size is at the start of the block \*/
	i = 0;
	/*\ Loop over the entries; the size of an entry is at the start \*/
	while (i < size) {
		CNEW(*fptr, 1);
		i += read_old_pfile(*fptr, buf + i, path, pl);
		fptr = &((*fptr)->next);
	}
	free(buf);
	return files;
}

/*\ read in an old style PAR file.  File should point to beginning. \*/
par_t *
read_old_par(file_t f, u16 *file, int silent)
{
	u8 buf[8];
	int px;
	par_t par, *r;
	char *path;

	memset(&par, 0, sizeof(par));

	path = complete_path(stuni(file));

	file_read(f, buf, 4);
	px = is_old_par(buf);
	if (!px) {
		file_close(f);
		return 0;
	}
	if (px == 1) px = 0;
	par.magic = PAR_MAGIC;
	par.client = 0x02000500;
	file_read(f, buf, 2);
	par.version = read_u16(buf);
	file_read(f, par.set_hash, 16);
	par.vol_number = 0;
	if (px) {
		file_read(f, buf, 2);
		par.vol_number = read_u16(buf);
		if (par.version != 0x85)
			par.vol_number = 1;
	}
	file_read(f, buf, 8);
	par.file_list = read_i64(buf);
	file_read(f, buf, 8);
	par.data = read_i64(buf);
	if (px) {
		file_read(f, buf, 8);
		par.data_size = read_i64(buf);
	}
	file_read(f, par.control_hash, 16);

	par.control_hash_offset = px ? 0x40 : 0x36;
	par.f = f;
	par.filename = file;

	if (!silent && !par_control_check(&par)) {
		file_close(f);
		return 0;
	}

	if (file_seek(f, par.file_list) < 0) {
		if (!silent) {
			fprintf(stderr, "Unable to seek in PAR file");
			perror("");
		}
		file_close(f);
		return 0;
	}
	par.filename = make_uni_str(path);
	file_read(f, buf, 8);
	par.file_list_size = read_i64(buf) - 8;
	par.files = read_old_pfiles(f, par.file_list_size, par.filename);

	if (px) {
		if (par.data != par.file_list + par.file_list_size)
			file_seek(f, par.data);
		par.f = f;
	} else {
		par.f = 0;
		file_close(f);
	}

	par.volumes = 0;

	NEW(r, 1);
	COPY(r, &par, 1);
	return r;
}
