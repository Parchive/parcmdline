/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Read and write PAR files
\*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "util.h"
#include "rwpar.h"
#include "fileops.h"
#include "rs.h"
#include "readoldpar.h"
#include "md5.h"

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

static void
write_i64(i64 v, void *data)
{
	size_t i;
	u8 *ptr = data;

	for (i = 0; i < sizeof(i64); i++) {
		ptr[i] = v & 0xFF;
		v >>= 8;
	}
}

static void
write_u32(u32 v, void *data)
{
	size_t i;
	u8 *ptr = data;

	for (i = 0; i < sizeof(u32); i++) {
		ptr[i] = v & 0xFF;
		v >>= 8;
	}
}

/*\ Write N bytes of little-endian u16s \*/
static void
write_u16s(u16 *str, void *data, i64 n)
{
	u8 *ptr = data;
	while (--n >= 0) {
		ptr[0] = (*str) & 0xff;
		ptr[1] = ((*str) >> 8) & 0xff;
		str++;
		ptr += 2;
	}
}

/*\ Change endianness to host byte order
|*| NB: This is a fix in place.  Don't call this twice!
\*/
static void
par_endian_read(par_t *par)
{
	par->version = read_u32(&par->version);
	par->client = read_u32(&par->client);
	par->vol_number = read_i64(&par->vol_number);
	par->num_files = read_i64(&par->num_files);
	par->file_list = read_i64(&par->file_list);
	par->file_list_size = read_i64(&par->file_list_size);
	par->data = read_i64(&par->data);
	par->data_size = read_i64(&par->data_size);
}

static void
par_endian_write(par_t *par, void *data)
{
	par_t *p = (par_t *)data;
	memcpy(p, par, PAR_FIX_HEAD_SIZE);
	write_u32(par->version, &p->version);
	write_u32(par->client, &p->client);
	write_i64(par->vol_number, &p->vol_number);
	write_i64(par->num_files, &p->num_files);
	write_i64(par->file_list, &p->file_list);
	write_i64(par->file_list_size, &p->file_list_size);
	write_i64(par->data, &p->data);
	write_i64(par->data_size, &p->data_size);
}

static u16 uni_empty[] = { 0 };

static i64
uni_sizeof(u16 *str)
{
	i64 l;
	for (l = 0; str[l]; l++)
		;
	return (2 * l);
}

/*\
|*| Order two unicode strings (caseless)
|*| Return 1 if a > b
\*/
int
unicode_gt(u16 *a, u16 *b)
{
	for (; *a || *b; a++, b++) {
		if (tolower(*a) > tolower(*b)) return 1;
		if (tolower(*a) < tolower(*b)) return 0;
	}
	return 0;
}

/*\
|*| Compare two unicode strings
|*| -1: Strings are not equal
|*|  0: Strings are equal
|*|  1: Strings only differ in upper/lowercase (doesn't happen with cmd.usecase)
\*/
int
unicode_cmp(u16 *a, u16 *b)
{
	for (; *a == *b; a++, b++)
		if (!*a) return 0;
	if (!cmd.usecase)
		for (; tolower(*a) == tolower(*b); a++, b++)
			if (!*a) return 1;
	return -1;
}

/*\
|*| Debugging output functions
\*/
static void
dump_file(pfile_t *file)
{
	fprintf(stderr,
		"    status: 0x%llx\n"
		"    file size: %lld\n"
		"    hash: %s\n",
		file->status,
		file->file_size,
		stmd5(file->hash));
	fprintf(stderr,
		"    16k hash: %s\n",
		stmd5(file->hash_16k));
	fprintf(stderr,
		"    filename: %s\n",
		stuni(file->filename));
}

void
dump_par(par_t *par)
{
	pfile_t *p;

	fprintf(stderr,  "PAR file dump:\n"
		"  version: 0x%04x\n"
		"  client: 0x%04x\n"
		"  control hash: %s\n",
		par->version,
		par->client,
		stmd5(par->control_hash));
	fprintf(stderr,
		"  set hash: %s\n",
		stmd5(par->set_hash));
	fprintf(stderr,
		"  volume number: %lld\n"
		"  number of files: %lld\n"
		"  file list: 0x%llx\n"
		"  file list size: 0x%llx\n"
		"  data: 0x%llx\n"
		"  data size: 0x%llx\n",
		par->vol_number,
		par->num_files,
		par->file_list,
		par->file_list_size,
		par->data,
		par->data_size);
	if (!par->vol_number)
		fprintf(stderr,
			"  comment: %s\n",
			stuni(par->comment));
	fprintf(stderr, "\nFiles:\n\n");
	for (p = par->files; p; p = p->next)
		dump_file(p);
}

/*\
|*| Static directory list
\*/
static hfile_t *hfile = 0;
static int hfile_done = 0;

/*\
|*| Rename a file, and move the directory entry with it.
\*/
static int
rename_file(u16 *src, u16 *dst)
{
	hfile_t *p;
	int n;

	fprintf(stderr, "    Rename: %s", stuni(src));
	fprintf(stderr, " -> %s", stuni(dst));
	n = file_rename(src, dst);
	if (n) {
		perror(" ");
		return n;
	}
	fprintf(stderr, "\n");

	/*\ Change directory entries \*/

	for (p = hfile; p; p = p->next) {
		if (!unicode_cmp(p->filename, src)) {
			uni_copy(p->filename, dst, FILENAME_MAX);
		}
	}
	return 0;
}

static hfile_t *
hfile_add(u16 *filename)
{
	int i;
	hfile_t **p;

	for (p = &hfile; *p; p = &((*p)->next))
		;
	CNEW(*p, 1);
	for (i = 0; filename[i] && (i < FILENAME_MAX - 1); i++)
		(*p)->filename[i] = filename[i];
	(*p)->filename[i] = 0;
	return (*p);
}

/*\
|*| Read in a directory and add it to the static directory structure
\*/
static void
hash_all_files(char *dir)
{
	hfile_t **p;

	for (p = &hfile; *p; p = &((*p)->next))
		;
	*p = read_dir(dir);
}

/*\
|*| Calculate md5 sums for a file, but only once
\*/
int
hash_file(hfile_t *file, char type)
{
	i64 s;
	u8 buf[16384];

	if (type < HASH16K) return 1;
	if (file->hashed < HASH16K) {
		if (!file_md5_buffer(file->filename, file->hash_16k,
					buf, sizeof(buf)))
			return 0;
		file->hashed = HASH16K;
		COPY(&file->magic, buf, 1);
	}
	if (type < HASH) return 1;
	if (file->hashed < HASH) {
		s = file_md5(file->filename, file->hash);
		if (s >= 0) {
			file->hashed = HASH;
			if (!file->file_size)
				file->file_size = s;
		}
	}
	return 1;
}

/*\
|*| Rename a file, but first move away the destination
\*/
int
rename_away(u16 *src, u16 *dst)
{
	if (move_away(dst, ".bad")) {
		fprintf(stderr, "    Rename: %s", stuni(src));
		fprintf(stderr, " -> %s : ", stuni(dst));
		fprintf(stderr, "File exists\n");
		fprintf(stderr, "  %-40s - NOT FIXED\n", stuni(dst));
		return -1;
	} else if (rename_file(src, dst)) {
		fprintf(stderr, "  %-40s - NOT FIXED\n", stuni(dst));
		return -1;
	} else {
		fprintf(stderr, "  %-40s - FIXED\n", stuni(dst));
		return 0;
	}
}

/*\
|*| Find a file in the static directory structure that matches the md5 sums.
|*| Try matching filenames first
\*/
int
find_file(pfile_t *file, int displ)
{
	hfile_t *p;
	int cm, corr = 0;

	if (file->match) return 1;

	if (!hfile_done) {
		hash_all_files(".");
		hfile_done = 1;
	}

	/*\ Check filename (caseless) and then check md5 hash \*/
	for (p = hfile; p; p = p->next) {
		cm = unicode_cmp(p->filename, file->filename);
		if (cm < 0) continue;
		if (!hash_file(p, HASH)) {
			if (displ) {
				fprintf(stderr, "      ERROR: %s",
						stuni(p->filename));
				perror(" ");
			}
			corr = 1;
			continue;
		}
		if (CMP_MD5(p->hash, file->hash)) {
			if (!cm || !file->match)
				file->match = p;
			continue;
		}
		if (displ)
			fprintf(stderr, "      ERROR: %s: Failed md5 sum\n",
					stuni(p->filename));
		corr = 1;
	}
	if (file->match) {
		if (displ)
			fprintf(stderr, "  %-40s - OK\n",
				stuni(file->filename));
		if (!displ || !cmd.dupl)
			return 1;
	}

	/*\ Try to match md5 hash on all files \*/
	for (p = hfile; p; p = p->next) {
		if (file->match == p)
			continue;
		if (!hash_file(p, HASH16K))
			continue;
		if (!CMP_MD5(p->hash_16k, file->hash_16k))
			continue;
		if (!hash_file(p, HASH))
			continue;
		if (!CMP_MD5(p->hash, file->hash))
			continue;
		if (!file->match) {
			file->match = p;
			if (displ) {
				if (cmd.fix) {
					rename_away(p->filename,
							file->filename);
				} else {
					fprintf(stderr, "  %-40s - FOUND",
							stuni(file->filename));
					fprintf(stderr, ": %s\n",
							stuni(p->filename));
				}
			}
			if (!displ || !cmd.dupl)
				return 1;
		}
		fprintf(stderr, "    Duplicate: %s",
			stuni(file->match->filename));
		fprintf(stderr, " == %s\n",
			stuni(p->filename));
	}
	if (!file->match && displ)
		fprintf(stderr, "  %-40s - %s\n",
			stuni(file->filename),
			corr ? "CORRUPT" : "NOT FOUND");
	return (file->match != 0);
}

/*\
|*| Find a file in the static directory structure
\*/
hfile_t *
find_file_path(char *path, int displ)
{
	u16 filename[FILENAME_MAX];
	char *file;
	hfile_t *p, *ret = 0;

	file = strrchr(path, '/');
	if (!file) file = path;
	if (strlen(file) >= FILENAME_MAX)
		return 0;
	unistr(file, filename);

	if (!hfile_done) {
		hash_all_files(".");
		hfile_done = 1;
	}

	/*\ Check filename (caseless) and then check md5 hash \*/
	for (p = hfile; p; p = p->next) {
		switch (unicode_cmp(p->filename, filename)) {
		case 1:
			if (ret) break;
		case 0:
			ret = p;
		}
	}
	if (!ret && displ)
		fprintf(stderr, "  %-40s - NOT FOUND\n", stuni(filename));
	return ret;
}

hfile_t *
find_file_name(u16 *path, int displ)
{
	u16 *file;
	hfile_t *p, *ret = 0;

	for (file = path; *file; file++)
		;
	while (*--file != '/')
		if (file <= path)
			break;

	if (!hfile_done) {
		hash_all_files(".");
		hfile_done = 1;
	}

	/*\ Check filename (caseless) and then check md5 hash \*/
	for (p = hfile; p; p = p->next) {
		switch (unicode_cmp(p->filename, file)) {
		case 1:
			if (ret) break;
		case 0:
			ret = p;
		}
	}
	if (!ret && displ)
		fprintf(stderr, "  %-40s - NOT FOUND\n", stuni(file));
	return ret;
}

/*\
|*| Find a volume in the static directory structure.
|*|  Base the name on the given filename and volume number.
|*|  Create it if it's not found.
\*/
hfile_t *
find_volume(u16 *name, i64 vol)
{
	u16 filename[FILENAME_MAX];
	i64 i;
	hfile_t *p, *ret = 0;
	int nd, v;

	if (vol < 1)
		return 0;
	nd = 2;
	for (v = 100; vol >= v; v *= 10)
		nd++;
	i = uni_copy(filename, name, FILENAME_MAX);
	if ((filename[i-1] < '0') || (filename[i-1] > '9')) {
		i = i - 2;
	} else {
		while ((filename[i-1] >= '0') && (filename[i-1] <= '9'))
			i--;
	}
	if ((i + nd) >= FILENAME_MAX)
		return 0;
	i += nd;
	filename[i] = 0;
	v = vol;
	while (--nd >= 0) {
		filename[--i] = '0' + v % 10;
		v /= 10;
	}

	if (!hfile_done) {
		hash_all_files(".");
		hfile_done = 1;
	}

	for (p = hfile; p; p = p->next) {
		switch (unicode_cmp(p->filename, filename)) {
		case 1:
			if (ret) break;
		case 0:
			ret = p;
		}
	}
	if (!ret)
		ret = hfile_add(filename);
	return ret;
}

/*\
|*| Rename a file so it's not in the way
|*|  Append '.bad' because I assume a file that's in the way
|*|  is a corrupt version.
\*/
int
move_away(u16 *file, const u8 *ext)
{
	u16 *fn;
	int l, i;

	if (!file_exists(file))
		return 0;
	if (!cmd.move)
		return -1;
	for (i = 0; file[i]; i++)
		;
	l = i + strlen(ext);
	NEW(fn, l + 1);
	COPY(fn, file, i);
	for (; i <= l; i++)
		fn[i] = *ext++;

	i = 0;
	while (file_exists(fn)) {
		if (i > 99) return -1;
		fn[l-2] = '0' + (i / 10);
		fn[l-1] = '0' + (i % 10);
		i++;
	}
	i = rename_file(file, fn);
	free(fn);
	return i;
}

/*\
|*| Read in a PAR file entry to a file struct
\*/
static i64
read_pfile(pfile_t *file, u8 *ptr)
{
	i64 i, l;
	pfile_entr_t *pf;

	pf = ((pfile_entr_t *)ptr);

	i = read_i64(&pf->size);
	file->status = read_i64(&pf->status);
	file->file_size = read_i64(&pf->file_size);
	COPY(file->hash, pf->hash, sizeof(md5));
	COPY(file->hash_16k, pf->hash_16k, sizeof(md5));
	l = (i - FILE_ENTRY_FIX_SIZE) / 2;
	NEW(file->filename, l + 1);
	read_u16s(file->filename, &pf->filename, l);
	file->filename[l] = 0;

	return i;
}

/*\
|*| Make a list of pointers into a list of file entries
\*/
static pfile_t *
read_pfiles(file_t f, i64 size)
{
	pfile_t *files = 0, **fptr = &files;
	u8 *buf;
	i64 i;

	NEW(buf, size);
	size = file_read(f, buf, size);

	/*\ The list size is at the start of the block \*/
	i = 0;
	/*\ Loop over the entries; the size of an entry is at the start \*/
	while (i < size) {
		CNEW(*fptr, 1);
		i += read_pfile(*fptr, buf + i);
		fptr = &((*fptr)->next);
	}
	free(buf);
	return files;
}

/*\
|*| Create a new PAR file struct
\*/
par_t *
create_par_header(u16 *file, i64 vol)
{
	par_t *par;

	CNEW(par, 1);
	par->magic = PAR_MAGIC;
	par->version = 0x00010000;
	par->client = 0x02000900;
	par->vol_number = vol;
	par->filename = unicode_copy(file);
	par->comment = uni_empty;
	par->control_hash_offset = 0x20;

	return par;
}

int
par_control_check(par_t *par)
{
	md5 hash;

	if (!cmd.ctrl) return 1;

	/*\ Check version number \*/
	if (par->version > 0x0001ffff) {
		fprintf(stderr, "%s: PAR Version mismatch! (%x.%x)\n",
				stuni(par->filename), par->version >> 16,
				(par->version & 0xffff) >> 8);
		return 0;
	}

	/*\ Check md5 control hash \*/
	if ((!file_get_md5(par->f, par->control_hash_offset, hash) ||
			!CMP_MD5(par->control_hash, hash)))
	{
		fprintf(stderr, "%s: PAR file corrupt:"
				"control hash mismatch!\n",
				stuni(par->filename));
		return 0;
	}
	return 1;
}

/*\
|*| Read in a PAR file, and return it into a newly allocated struct
|*| (to be freed with free_par())
\*/
par_t *
read_par_header(u16 *file, int create, i64 vol, int silent)
{
	par_t par, *r;

	par.f = file_open(file, 0);
	/*\ Read in the first part of the struct, it fits directly on top \*/
	if (file_read(par.f, &par, PAR_FIX_HEAD_SIZE) < PAR_FIX_HEAD_SIZE) {
		if (!create || (errno != ENOENT)) {
			if (!silent)
				perror("Error reading PAR file");
			file_close(par.f);
			return 0;
		}
		if (!vol) {
			/*\ Guess volume number from last digits \*/
			u16 *p;
			for (p = file; *p; p++)
				;
			while ((--p >= file) && (*p >= '0') && (*p <= '9'))
				;
			while (*++p)
				vol = vol * 10 + (*p - '0');
		}
		return create_par_header(file, vol);
	}
	/*\ Is it the right file type ? \*/
	if (!IS_PAR(par)) {
		if (is_old_par(&par))
			if (file_seek(par.f, 0) >= 0)
				return read_old_par(par.f, file, silent);
		if (!silent)
			fprintf(stderr, "%s: Not a PAR file\n", stuni(file));
		file_close(par.f);
		return 0;
	}
	par_endian_read(&par);

	par.control_hash_offset = 0x20;
	par.filename = file;

	if (!silent && !par_control_check(&par)) {
		file_close(par.f);
		return 0;
	}

	file_seek(par.f, par.file_list);

	/*\ Read in the filelist. \*/
	par.files = read_pfiles(par.f, par.file_list_size);

	file_seek(par.f, par.data);

	par.filename = unicode_copy(file);

	if (par.vol_number == 0) {
		CNEW(par.comment, (par.data_size / 2) + 1);
		file_read(par.f, par.comment, par.data_size);
		file_close(par.f);
		par.f = 0;
	}

	par.volumes = 0;

	NEW(r, 1);
	COPY(r, &par, 1);
	if (cmd.loglevel > 1)
		dump_par(r);
	return r;
}

void
free_file_list(pfile_t *list)
{
	pfile_t *next;

	while (list) {
		if (list->f) file_close(list->f);
		if (list->fnrs) free(list->fnrs);
		next = list->next;
		free(list);
		list = next;
	}
}

void
free_par(par_t *par)
{
	free_file_list(par->files);
	free_file_list(par->volumes);
	free(par->filename);
	if (par->f) file_close(par->f);
	free(par);
}

/*\
|*| Write out a PAR file entry from a file struct
\*/
static i64
write_pfile(pfile_t *file, pfile_entr_t *pf)
{
	i64 i;

	i = uni_sizeof(file->filename);

	write_i64(FILE_ENTRY_FIX_SIZE + i, &pf->size);
	write_i64(file->status, &pf->status);
	write_i64(file->file_size, &pf->file_size);
	COPY(pf->hash, file->hash, sizeof(md5));
	COPY(pf->hash_16k, file->hash_16k, sizeof(md5));
	write_u16s(file->filename, &pf->filename, i / 2);
	return FILE_ENTRY_FIX_SIZE + i;
}

/*\
|*| Write a list of file entries to a file
\*/
static i64
write_file_entries(file_t f, pfile_t *files)
{
	i64 tot, t, m;
	pfile_t *p;
	pfile_entr_t *pfe;

	tot = m = 0;
	for (p = files; p; p = p->next) {
		t = FILE_ENTRY_FIX_SIZE + uni_sizeof(p->filename);
		tot += t;
		if (m < t) m = t;
	}
	pfe = (pfile_entr_t *)malloc(m);
	if (f) {
		for (p = files; p; p = p->next) {
			t = write_pfile(p, pfe);
			file_write(f, pfe, t);
		}
	}
	free(pfe);
	return tot;
}

/*\
|*| Write out a PAR volume header
\*/
file_t
write_par_header(par_t *par)
{
	file_t f;
	par_t data;
	pfile_t *p;
	int i;
	md5 *hashes;

	/*\ Open output file, but check so we don't overwrite anything \*/
	if (move_away(par->filename, ".old")) {
		fprintf(stderr, "      WRITE ERROR: %s: ",
				stuni(par->filename));
		fprintf(stderr, "File exists\n");
		return 0;
	}
	f = file_open(par->filename, 1);
	if (!f) {
		fprintf(stderr, "      WRITE ERROR: %s: ",
				stuni(par->filename));
		perror("");
		return 0;
	}
	par->file_list = PAR_FIX_HEAD_SIZE;
	par->file_list_size = write_file_entries(0, par->files);
	par->data = par->file_list + par->file_list_size;

	if (par->vol_number == 0) {
		par->data_size = uni_sizeof(par->comment);
	} else {
		for (i = 0, p = par->files; p; p = p->next, i++) {
			if (par->data_size < p->file_size)
				par->data_size = p->file_size;
		}
	}
	/*\ Calculate set hash \*/
	par->num_files = 0;
	for (i = 0, p = par->files; p; p = p->next) {
		par->num_files++;
		if (USE_FILE(p))
			i++;
	}
	NEW(hashes, i);
	for (i = 0, p = par->files; p; p = p->next) {
		if (!USE_FILE(p))
			continue;
		COPY(hashes[i], p->hash, sizeof(md5));
		i++;
	}
	md5_buffer((char *)hashes, i * sizeof(md5), par->set_hash);
	free(hashes);

	if (cmd.loglevel > 1)
		dump_par(par);

	par_endian_write(par, &data);

	file_write(f, &data, PAR_FIX_HEAD_SIZE);
	write_file_entries(f, par->files);

	if (par->vol_number == 0) {
		file_write(f, par->comment, par->data_size);
		if (cmd.ctrl) {
			if (!file_add_md5(f, 0x0010, 0x0020,
					par->data + par->data_size))
			{
				fprintf(stderr, "      ERROR: %s:",
						stuni(par->filename));
				perror("");
				fprintf(stderr, "  %-40s - FAILED\n",
						stuni(par->filename));
				file_close(f);
				f = 0;
				if (!cmd.keep) file_delete(par->filename);
			}
		}
		if (f) file_close(f);
	}
	return f;
}

/*\
|*| Check if the two file lists a and b match.
|*|  File lists match if they are equal,
|*|   only counting files which are stored in the parity volume.
\*/
static int
files_match(pfile_t *a, pfile_t *b, int part)
{
	if (part) {
		/*\ Check if the lists overlap \*/
		pfile_t *p;

		for (; a; a = a->next) {
			if (!USE_FILE(a)) continue;
			for (p = b; p; p = p->next) {
				if (!USE_FILE(p)) continue;
				if (a->file_size != b->file_size) continue;
				if (!CMP_MD5(a->hash, b->hash)) continue;
				return 1;
			}
		}
		return 0;
	}
	/*\ Check if the lists match \*/
	while (a || b) {
		if (a && !USE_FILE(a)) {
			a = a->next;
			continue;
		}
		if (b && !USE_FILE(b)) {
			b = b->next;
			continue;
		}
		if (!a || !b) return 0;
		if (a->file_size != b->file_size) return 0;
		if (!CMP_MD5(a->hash, b->hash)) return 0;
		a = a->next; b = b->next;
	}
	return 1;
}

u16 *
file_numbers(pfile_t **list, pfile_t **files)
{
	int i, j;
	pfile_t *p, **qq;
	u16 *fnrs;

	for (i = 0, p = *files; p; p = p->next)
		if (USE_FILE(p))
			i++;
	NEW(fnrs, i + 1);
	for (i = 0; *files; ) {
		if (!USE_FILE(*files)) {
			files = &((*files)->next);
			continue;
		}
		/*\ Look for a match \*/
		for (j = 1, qq = list; *qq; qq = &((*qq)->next), j++) {
			if (!USE_FILE(*qq)) continue;
			if ((*files)->file_size != (*qq)->file_size) continue;
			if (!CMP_MD5((*files)->hash, (*qq)->hash)) continue;
			break;
		}
		/*\ No match ? Move the file entry to the tail of the list \*/
		if (!*qq) {
			*qq = *files;
			*files = (*files)->next;
			(*qq)->next = 0;
		} else {
			files = &((*files)->next);
		}
		fnrs[i++] = j;
	}
	fnrs[i] = 0;
	return fnrs;
}

/*\
|*| Find all volumes that have the same set of files,
|*| but different volume numbers
\*/
int
find_volumes(par_t *par, int tofind)
{
	int m;
	pfile_t *v;
	hfile_t *p;
	par_t *tmp;

	v = 0;
	if (par->vol_number) {
		CNEW(v, 1);
		v->match = find_file_path(stuni(par->filename), 1);
		v->vol_number = par->vol_number;
		v->file_size = par->data_size;
		v->fnrs = file_numbers(&par->files, &par->files);
		v->f = par->f;
		v->next = par->volumes;
		par->volumes = v;
	} else {
		file_close(par->f);
	}
	par->f = 0;
	for (m = 0, v = par->volumes; v; v = v->next)
		m++;
	if (m >= tofind)
		return m;
	fprintf(stderr, "\nLooking for %sPXX volumes:\n",
		m ? "additional " : "");

	if (!hfile_done) {
		hash_all_files(".");
		hfile_done = 1;
	}

	for (p = hfile; p; p = p->next) {
		if ((p->hashed >= HASH16K) && !IS_PAR(*p)
				&& !is_old_par(&p->magic))
			continue;
		tmp = read_par_header(p->filename, 0, 0, 1);
		if (!tmp) continue;
		if (tmp->vol_number && files_match(par->files, tmp->files, 0)) {
			for (v = par->volumes; v; v = v->next)
				if (v->vol_number == tmp->vol_number)
					break;
			if (v) continue;
			if (!par_control_check(tmp)) {
				fprintf(stderr, "  %-40s - CORRUPT\n",
					stuni(p->filename));
				free_par(tmp);
				continue;
			}
			CNEW(v, 1);
			v->match = p;
			v->vol_number = tmp->vol_number;
			v->file_size = tmp->data_size;
			v->fnrs = file_numbers(&par->files, &tmp->files);
			v->f = tmp->f;
			tmp->f = 0;
			v->next = par->volumes;
			par->volumes = v;
			m++;
			fprintf(stderr, "  %-40s - OK\n",
				stuni(p->filename));
		}
		free_par(tmp);
		if (m >= tofind)
			break;
	}
	return m;
}

/*\
|*| Find all volumes that contain (some of) the given files.
\*/
void
find_par_files(pfile_t **volumes, pfile_t **files, int part)
{
	pfile_t *v, **vv;
	hfile_t *p;
	par_t *tmp;

	if (!hfile_done) {
		hash_all_files(".");
		hfile_done = 1;
	}

	for (p = hfile; p; p = p->next) {
		if ((p->hashed >= HASH16K) && !IS_PAR(*p)
				&& !is_old_par(&p->magic))
			continue;
		for (v = *volumes; v; v = v->next)
			if (!unicode_cmp(v->filename, p->filename))
				break;
		if (v) continue;
		tmp = read_par_header(p->filename, 0, 0, 1);
		if (!tmp) continue;
		if (tmp->vol_number && files_match(*files, tmp->files, part))
		{
			if (!par_control_check(tmp)) {
				free_par(tmp);
				continue;
			}
			CNEW(v, 1);
			v->match = p;
			v->vol_number = tmp->vol_number;
			v->file_size = tmp->data_size;
			v->fnrs = file_numbers(files, &tmp->files);
			v->f = tmp->f;
			v->filename = unicode_copy(tmp->filename);
			tmp->f = 0;

			/*\ Insert in alphabetically correct place \*/
			for (vv = volumes; *vv; vv = &((*vv)->next))
				if (unicode_gt((*vv)->filename, v->filename))
						break;
			v->next = *vv;
			*vv = v;
		}
		free_par(tmp);
	}
}

/*\
|*| Find all volumes in the current directory.
\*/
par_t *
find_all_par_files(void)
{
	par_t *par, *tmp;
	pfile_t *v;
	hfile_t *p;

	par = create_par_header(0, 0);

	fprintf(stderr, "Looking for PAR archives:\n");

	if (!hfile_done) {
		hash_all_files(".");
		hfile_done = 1;
	}

	for (p = hfile; p; p = p->next) {
		if ((p->hashed >= HASH16K) && !IS_PAR(*p)
				&& !is_old_par(&p->magic))
			continue;
		tmp = read_par_header(p->filename, 0, 0, 1);
		if (!tmp) continue;
		if (tmp->vol_number) {
			if (!par_control_check(tmp)) {
				fprintf(stderr, "  %-40s - CORRUPT\n",
					stuni(p->filename));
				free_par(tmp);
				continue;
			}
			CNEW(v, 1);
			v->match = p;
			v->vol_number = tmp->vol_number;
			v->file_size = tmp->data_size;
			v->fnrs = file_numbers(&par->files, &tmp->files);
			v->f = tmp->f;
			tmp->f = 0;
			v->next = par->volumes;
			par->volumes = v;
			fprintf(stderr, "  %-40s - FOUND\n",
					stuni(p->filename));
		}
		free_par(tmp);
	}
	if (!par->volumes) {
		free_par(par);
		return 0;
	}
	return par;
}

/*\
|*| Restore missing files with recovery volumes
\*/
int
restore_files(pfile_t *files, pfile_t *volumes)
{
	int N, M, i;
	xfile_t *in, *out;
	pfile_t *p, *v, **pp, **qq;
	int fail = 0;
	i64 size;
	pfile_t *mis_f, *mis_v;

	/*\ Separate out missing files \*/
	p = files;
	size = 0;
	pp = &files;
	qq = &mis_f;
	*pp = *qq = 0;
	for (i = 1; p; p = p->next, i++) {
		p->vol_number = i;
		if (p->file_size > size)
			size = p->file_size;
		if (!find_file(p, 0) && USE_FILE(p)) {
			NEW(*qq, 1);
			COPY(*qq, p, 1);
			qq = &((*qq)->next);
			*qq = 0;
		} else {
			NEW(*pp, 1);
			COPY(*pp, p, 1);
			(*pp)->next = 0;
			pp = &((*pp)->next);
			*pp = 0;
		}
	}

	/*\ Separate out missing volumes \*/
	p = volumes;
	pp = &volumes;
	qq = &mis_v;
	*pp = *qq = 0;
	for (; p; p = p->next) {
		if (p->vol_number && !(p->f)) {
			NEW(*qq, 1);
			COPY(*qq, p, 1);
			qq = &((*qq)->next);
			*qq = 0;
		} else {
			NEW(*pp, 1);
			COPY(*pp, p, 1);
			pp = &((*pp)->next);
			*pp = 0;
		}
	}

	/*\ Count existing files and volumes \*/
	for (N = 0, p = files; p; p = p->next)
		N++;
	for (v = volumes; v; v = v->next, N++)
		N++;

	/*\ Count missing files and volumes \*/
	for (M = 0, p = mis_f; p; p = p->next)
		M++;
	for (v = mis_v; v; v = v->next, N++)
		M++;

	NEW(in, N + 1);
	NEW(out, M + 1);

	/*\ Fill in input files \*/
	for (i = 0, p = files; p; p = p->next) {
		p->f = file_open(p->match->filename, 0);
		if (!p->f) {
			fprintf(stderr, "      ERROR: %s:",
					stuni(p->match->filename));
			perror("");
			continue;
		}
		in[i].filenr = p->vol_number;
		in[i].files = 0;
		in[i].size = p->file_size;
		in[i].f = p->f;
		i++;
	}
	/*\ Fill in input volumes \*/
	for (v = volumes; v; v = v->next) {
		in[i].filenr = v->vol_number;
		in[i].files = v->fnrs;
		in[i].size = v->file_size;
		in[i].f = v->f;
		i++;
	}
	in[i].filenr = 0;

	/*\ Fill in output files \*/
	for (i = 0, p = mis_f; p; p = p->next) {
		/*\ Open output file, but check we don't overwrite anything \*/
		if (move_away(p->filename, ".bad")) {
			fprintf(stderr, "      ERROR: %s: ",
				stuni(p->filename));
			fprintf(stderr, "File exists\n");
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
				stuni(p->filename));
			continue;
		}
		p->f = file_open(p->filename, 1);
		if (!p->f) {
			fprintf(stderr, "      ERROR: %s: ",
				stuni(p->filename));
			perror("");
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
				stuni(p->filename));
			continue;
		}
		out[i].size = p->file_size;
		out[i].filenr = p->vol_number;
		out[i].files = 0;
		out[i].f = p->f;
		i++;
	}

	/*\ Fill in output volumes \*/
	for (v = mis_v; v; v = v->next) {
		par_t *par;

		par = create_par_header(v->filename, v->vol_number);
		if (!par) {
			fprintf(stderr, "  %-40s - FAILED\n",
					stuni(v->match->filename));
			continue;
		}
		/*\ Copy file list into par file \*/
		par->files = files;
		par->data_size = size;
		v->f = write_par_header(par);
		par->files = 0;
		if (!v->f) {
			fprintf(stderr, "  %-40s - FAILED\n",
					stuni(par->filename));
			fail |= 1;
			free_par(par);
			continue;
		}
		v->match = hfile_add(par->filename);
		v->filename = v->match->filename;
		v->file_size = par->data + par->data_size;
		out[i].size = par->data_size;
		out[i].filenr = v->vol_number;
		out[i].files = v->fnrs;
		out[i].f = v->f;
		free_par(par);
		i++;
	}
	out[i].filenr = 0;

	if (!recreate(in, out))
		fail |= 1;

	free(in);
	free(out);

	/*\ Check resulting data files \*/
	for (p = mis_f; p; p = p->next) {
		if (!p->f) continue;
		file_close(p->f);
		p->f = 0;
		p->match = hfile_add(p->filename);
		if (!hash_file(p->match, HASH)) {
			fprintf(stderr, "      ERROR: %s:", stuni(p->filename));
			perror("");
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
					stuni(p->filename));
			fail |= 1;
			if (!cmd.keep) file_delete(p->filename);
			continue;
		}
		if ((p->match->file_size == 0) && (p->file_size != 0)) {
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
					stuni(p->filename));
			fail |= 1;
			if (!cmd.keep) file_delete(p->filename);
			continue;
		}
		if (!CMP_MD5(p->match->hash, p->hash)) {
			fprintf(stderr, "      ERROR: %s: Failed md5 check\n",
					stuni(p->filename));
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
					stuni(p->filename));
			fail |= 1;
			if (!cmd.keep) file_delete(p->filename);
			continue;
		}
		fprintf(stderr, "  %-40s - RECOVERED\n",
				stuni(p->filename));
	}

	/*\ Check resulting volumes \*/
	for (v = mis_v; v; v = v->next) {
		if (!v->f) continue;
		if (!file_add_md5(v->f, 0x0010, 0x0020, v->file_size)) {
			fprintf(stderr, "  %-40s - FAILED\n",
					stuni(v->filename));
			fail |= 1;
			file_close(v->f);
			v->f = 0;
			if (!cmd.keep) file_delete(v->filename);
			continue;
		}
		fprintf(stderr, "  %-40s - OK\n", stuni(v->filename));
	}

	while ((p = files)) {
		files = p->next;
		free(p);
	}
	while ((p = volumes)) {
		volumes = p->next;
		free(p);
	}
	while ((p = mis_f)) {
		mis_f = p->next;
		free(p);
	}
	while ((p = mis_v)) {
		mis_v = p->next;
		free(p);
	}

	if (fail) {
		fprintf(stderr, "\nErrors occurred.\n\n");
		return -1;
	}
	return 1;
}
