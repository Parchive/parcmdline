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
#include "endian.h"
#include "rs.h"

static u16 uni_empty[] = { 0 };

static size_t
uni_sizeof(u16 *str)
{
	i64 l;
	for (l = 0; str[l++]; )
		;
	return (2 * l);
}

/*\
|*| Compare two unicode strings
|*| -1: Strings are not equal
|*|  0: Strings are equal
|*|  1: Strings only differ in upper/lowercase (only happens with cmd.nocase)
\*/
int
unicode_cmp(u16 *a, u16 *b)
{
	for (; *a == *b; a++, b++)
		if (!*a) return 0;
	if (cmd.nocase)
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
		"    16k hash: %s\n",
		file->status,
		file->file_size,
		stmd5(file->hash_16k));
	fprintf(stderr,
		"    hash: %s\n",
		stmd5(file->hash));
	fprintf(stderr,
		"    volume number: %d\n"
		"    filename: %s\n",
		file->vol_number,
		stuni(file->filename));
	fprintf(stderr,
		"    comment: %s\n\n",
		stuni(file->comment));
}

static void
dump_pxx(pxx_t *pxx)
{
	pfile_t *p;

	fprintf(stderr,  "PXX file dump:\n"
		"  version: 0x%04x\n"
		"  set hash: %s\n",
		pxx->version,
		stmd5(pxx->set_hash));
	fprintf(stderr,
		"  volume number: %d\n"
		"  file list: 0x%llx\n"
		"  parity data: 0x%llx\n"
		"  parity data size: %lld\n"
		"  control hash: %s\n",
		pxx->vol_number,
		pxx->file_list,
		pxx->parity_data,
		pxx->parity_data_size,
		stmd5(pxx->control_hash));
	fprintf(stderr,
		"  comment: %s\n\nFiles:\n\n",
		stuni(pxx->comment));
	for (p = pxx->files; p; p = p->next)
		dump_file(p);
}

void
dump_par(par_t *par)
{
	pfile_t *p;

	if (IS_PXX(*par)) {
		dump_pxx((pxx_t *)par);
		return;
	}

	fprintf(stderr,  "PAR file dump:\n"
		"  version: 0x%04x\n"
		"  set hash: %s\n",
		par->version,
		stmd5(par->set_hash));
	fprintf(stderr,
		"  file list: 0x%llx\n"
		"  volume list: 0x%llx\n"
		"  control hash: %s\n",
		par->file_list,
		par->volume_list,
		stmd5(par->control_hash));
	fprintf(stderr,
		"  comment: %s\n\n",
		stuni(par->comment));
	fprintf(stderr, "Files:\n\n");
	for (p = par->files; p; p = p->next)
		dump_file(p);
	fprintf(stderr, "Volumes:\n\n");
	for (p = par->volumes; p; p = p->next)
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

	if (type < HASH16K) return 1;
	if (file->hashed < HASH16K) {
		if (!file_md5_16k(file->filename, file->hash_16k))
			return 0;
		file->hashed = HASH16K;
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
void
rename_away(u16 *src, u16 *dst)
{
	if (cmd.fix) {
		if (move_away(dst, ".bad")) {
			fprintf(stderr, "    Rename: %s", stuni(src));
			fprintf(stderr, " -> %s : ", stuni(dst));
			fprintf(stderr, "File exists\n");
			fprintf(stderr, "  %-40s - NOT FIXED\n", stuni(dst));
		} else if (rename_file(src, dst)) {
			fprintf(stderr, "  %-40s - NOT FIXED\n", stuni(dst));
		} else {
			fprintf(stderr, "  %-40s - FIXED\n", stuni(dst));
		}
	} else {
		fprintf(stderr, "  %-40s - FOUND", stuni(dst));
		fprintf(stderr, ": %s\n", stuni(src));
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
			if (displ)
				rename_away(p->filename, file->filename);
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
find_file_path(char *path)
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
	if (!ret)
		fprintf(stderr, "  %-40s - NOT FOUND\n", stuni(filename));
	return ret;
}

/*\
|*| Find a volume in the static directory structure.
|*|  Base the name on the given filename and volume number.
|*|  Create it if it's not found.
\*/
hfile_t *
find_volume(u16 *name, u16 vol)
{
	u16 filename[FILENAME_MAX];
	size_t i;
	hfile_t *p, *ret = 0;

	if ((vol < 1) || (vol > 99))
		return 0;
	i = uni_copy(filename, name, FILENAME_MAX);
	filename[i - 2] = '0' + (vol / 10);
	filename[i - 1] = '0' + (vol % 10);
	filename[i] = 0;

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
|*| Check if the two file lists a and b are equal
\*/
static int
files_equal(pfile_t *a, pfile_t *b)
{
	while (a || b) {
		if (!a || !b) return 0;
		if (a->file_size != b->file_size) return 0;
		if (!CMP_MD5(a->hash, b->hash)) return 0;
		if (!CMP_MD5(a->hash_16k, b->hash_16k)) return 0;
		a = a->next; b = b->next;
	}
	return 1;
}

/*\
|*| Find all volumes that have the same set of files,
|*| but different volume numbers
\*/
int
find_volumes(pxx_t *pxx, int tofind)
{
	int m;
	pfile_t *v;
	hfile_t *p;
	pxx_t *tmp;

	CNEW(v, 1);
	v->match = find_file_path(stuni(pxx->filename));
	v->vol_number = pxx->vol_number;
	free_file_list(pxx->volumes);
	pxx->volumes = v;
	m = 1;
	if (pxx->version < 0x80)
		return 1;
	fprintf(stderr, "\nLooking for PXX volumes:\n");
	for (p = hfile; p && (m < tofind); p = p->next) {
		tmp = read_pxx_header(stuni(p->filename), 0, 0, 1);
		if (!tmp) continue;
		if (files_equal(tmp->files, pxx->files)) {
			for (v = pxx->volumes; v; v = v->next) {
				if (v->vol_number == tmp->vol_number)
					break;
			}
			if (!v) {
				CNEW(v, 1);
				v->match = p;
				v->vol_number = tmp->vol_number;
				v->next = pxx->volumes;
				pxx->volumes = v;
				m++;
				fprintf(stderr, "  %-40s - OK\n",
					stuni(p->filename));
			}
		}
		free_pxx(tmp);
	}
	return m;
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
	i64 i;
	pfile_entr_t *pf;
	u16 *p;

	pf = ((pfile_entr_t *)ptr);

	i = read_i64(&pf->size);
	file->status = read_i64(&pf->status);
	file->file_size = read_i64(&pf->file_size);
	COPY(file->hash_16k, pf->hash_16k, sizeof(md5));
	COPY(file->hash, pf->hash, sizeof(md5));
	file->vol_number = read_u16(&pf->vol_number);
	p = pf->filename;
	file->filename = p;
	while (*p)
		p++;
	p++;
	file->comment = p;

	return i;
}

/*\
|*| Make a list of pointers into a list of file entries
\*/
static pfile_t *
read_pfiles(u8 *data)
{
	pfile_t *files = 0, **fptr = &files;
	i64 size, i;

	/*\ The list size is at the start of the block \*/
	size = read_i64(data);
	i = 8;
	/*\ Count number of entries; the size of an entry is at the start \*/
	while (i < size) {
		CNEW(*fptr, 1);
		i += read_pfile(*fptr, data + i);
		fptr = &((*fptr)->next);
	}
	return files;
}

/*\
|*| Create a new PXX file struct
\*/
pxx_t *
create_pxx_header(char *file, u16 vol)
{
	pxx_t *pxx;

	CNEW(pxx, 1);
	pxx->magic = PXX_MAGIC;
	pxx->version = 0x85;
	pxx->vol_number = vol;
	pxx->filename = make_uni_str(file);
	pxx->comment = uni_empty;

	return pxx;
}

/*\
|*| Create a new PAR file struct
\*/
par_t *
create_par_header(char *file)
{
	par_t *par;

	CNEW(par, 1);
	par->magic = PAR_MAGIC;
	par->version = 0x85;
	par->filename = make_uni_str(file);
	par->comment = uni_empty;

	return par;
}

/*\
|*| Read in a PXX file, and return it into a newly allocated struct
|*| (A PAR header was already read in, but turned out to be a PXX header,
|*|  so copy that onto the start and continue reading)
|*| (to be freed with free_pxx())
\*/
pxx_t *
read_pxx_header(char *file, par_t *par, u16 vol, int silent)
{
	pxx_t pxx, *r;
	i64 i;
	md5 hash;

	if (par) {
		/*\ Copy stuff from the supplied PAR header \*/
		memcpy(&pxx, par, PAR_FIX_HEAD_SIZE);
		pxx.f = par->f;
		/*\ Read in the rest of the struct, directly on top \*/
		if (file_read(pxx.f, ((void *)&pxx) + PAR_FIX_HEAD_SIZE,
				PXX_FIX_HEAD_SIZE - PAR_FIX_HEAD_SIZE) <
		(PXX_FIX_HEAD_SIZE - PAR_FIX_HEAD_SIZE)) {
			perror("Error reading file");
			file_close(pxx.f);
			return 0;
		}
	} else {
		if (!(pxx.f = file_open_ascii(file, 0))) {
			if (!vol) {
				if (!silent)
					perror("Error opening file");
				return 0;
			}
			return create_pxx_header(file, vol);
		}
		/*\ Read in the first part of the struct,
		|*| it fits directly on top \*/
		if (file_read(pxx.f, &pxx, PXX_FIX_HEAD_SIZE)
				< PXX_FIX_HEAD_SIZE) {
			if (!silent)
				perror("Error reading file");
			file_close(pxx.f);
			return 0;
		}
		if (!IS_PXX(pxx)) {
			if (!silent)
				fprintf(stderr, "Not a PXX file\n");
			file_close(pxx.f);
			return 0;
		}
	}

	pxx_endian_read(&pxx);

	/*\ Check version number \*/
	if (pxx.version >= 0x100) {
		if (!silent)
			fprintf(stderr, "PXX Version mismatch!\n");
		file_close(pxx.f);
		return 0;
	}

	/*\ Check md5 control hash \*/
	if (cmd.ctrl && (!file_get_md5(pxx.f, hash) ||
			!CMP_MD5(pxx.control_hash, hash)))
	{
		fprintf(stderr, "PXX file corrupt: control hash mismatch!\n");
		file_close(pxx.f);
		return 0;
	}

	/*\ Check if we have the right volume \*/
	if (vol && (vol != pxx.vol_number)) {
		if (!silent)
			fprintf(stderr, "PXX Volume mismatch!\n");
		file_close(pxx.f);
		return 0;
	}

	/*\ Read everything up to the parity data \*/
	i = pxx.parity_data - PXX_FIX_HEAD_SIZE;
	NEW(pxx.data, pxx.parity_data);
	memcpy(pxx.data, &pxx, PXX_FIX_HEAD_SIZE);
	if (file_read(pxx.f, pxx.data + PXX_FIX_HEAD_SIZE, i) < i) {
		if (!silent)
			perror("Error reading file");
		file_close(pxx.f);
		return 0;
	}
	/*\ Point some pointers into the data block \*/
	pxx.comment = (u16 *)(pxx.data + PXX_FIX_HEAD_SIZE);
	pxx.files = read_pfiles(pxx.data + pxx.file_list);
	pxx.volumes = 0;

	pxx.filename = make_uni_str(file);

	NEW(r, 1);
	COPY(r, &pxx, 1);
	if (cmd.loglevel > 1)
		dump_pxx(r);
	return r;
}

/*\
|*| Read in a PAR file, and return it into a newly allocated struct
|*| (to be freed with free_par())
\*/
par_t *
read_par_header(char *file, int create)
{
	par_t par, *r;
	i64 i, n;
	md5 hash;

	if (!(par.f = file_open_ascii(file, 0))) {
		if (!create) {
			perror("Error opening file");
			return 0;
		}
		return create_par_header(file);
	}

	/*\ Read in the first part of the struct, it fits directly on top \*/
	if (file_read(par.f, &par, PAR_FIX_HEAD_SIZE) < PAR_FIX_HEAD_SIZE) {
		perror("Error reading file");
		file_close(par.f);
		return 0;
	}
	if (IS_PXX(par))
		return (par_t *)read_pxx_header(file, &par, 0, 0);
	/*\ Is it the right file type ? \*/
	if (!IS_PAR(par)) {
		fprintf(stderr, "Not a PAR file\n");
		file_close(par.f);
		return 0;
	}
	par_endian_read(&par);

	/*\ Check version number \*/
	if (par.version >= 0x100) {
		fprintf(stderr, "PAR Version mismatch!\n");
		file_close(par.f);
		return 0;
	}

	/*\ Check md5 control hash \*/
	if (cmd.ctrl && (!file_get_md5(par.f, hash) ||
			!CMP_MD5(par.control_hash, hash)))
	{
		fprintf(stderr, "PAR file corrupt: control hash mismatch!\n");
		file_close(par.f);
		return 0;
	}

	/*\ Find the start of the last block we want \*/
	n = par.file_list > par.volume_list ? par.file_list : par.volume_list;
	n += 8;
	NEW(par.data, n);
	memcpy(par.data, &par, PAR_FIX_HEAD_SIZE);
	i = n - PAR_FIX_HEAD_SIZE;
	/*\ Read it in, including the first 8 bytes containing the size \*/
	if (file_read(par.f, par.data + PAR_FIX_HEAD_SIZE, i) < i) {
		perror("Error reading file");
		file_close(par.f);
		return 0;
	}
	i = *(i64 *)(par.data + (n - 8)) - 8;
	RENEW(par.data, n + i);
	/*\ Now we have the size, read in the rest of it \*/
	if (file_read(par.f, par.data + n, i) < i) {
		perror("Error reading file");
		file_close(par.f);
		return 0;
	}
	file_close(par.f);

	/*\ Point some pointers into the data block \*/
	par.comment = (u16 *)(par.data + PAR_FIX_HEAD_SIZE);
	par.files = read_pfiles(par.data + par.file_list);
	par.volumes = read_pfiles(par.data + par.volume_list);

	par.filename = make_uni_str(file);

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
		if (list->new) {
			free(list->filename);
			free(list->comment);
		}
		next = list->next;
		free(list);
		list = next;
	}
}

void
free_pxx(pxx_t *pxx)
{
	file_close(pxx->f);
	free(pxx->data);
	free_file_list(pxx->files);
	free_file_list(pxx->volumes);
	free(pxx->filename);
	free(pxx);
}

void
free_par(par_t *par)
{
	if (IS_PXX(*par)) {
		free_pxx((pxx_t *)par);
		return;
	}
	free_file_list(par->files);
	free_file_list(par->volumes);
	free(par->data);
	free(par->filename);
	free(par);
}

/*\
|*| Write out a PAR file entry from a file struct
\*/
static void
write_pfile(pfile_t *file, pfile_entr_t *pf)
{
	i64 i;

	i = FILE_ENTRY_FIX_SIZE + uni_sizeof(file->filename) +
		uni_sizeof(file->comment);

	write_i64(i, &pf->size);
	write_i64(file->status, &pf->status);
	write_i64(file->file_size, &pf->file_size);
	COPY(pf->hash_16k, file->hash_16k, sizeof(md5));
	COPY(pf->hash, file->hash, sizeof(md5));
	write_u16(file->vol_number, &pf->vol_number);
}

/*\
|*| Write a list of file entries to a file
\*/
static i64
write_file_entries(file_t f, pfile_t *files)
{
	i64 tot, t;
	pfile_t *p;
	pfile_entr_t pfe;
	tot = 8;

	for (p = files; p; p = p->next) {
		tot += FILE_ENTRY_FIX_SIZE
		    + uni_sizeof(p->filename)
		    + uni_sizeof(p->comment);
	}
	if (!f) return tot;
	write_i64(tot, &t);
	file_write(f, &t, sizeof(t));
	for (p = files; p; p = p->next) {
		write_pfile(p, &pfe);
		file_write(f, &pfe, FILE_ENTRY_FIX_SIZE);
		file_write(f, p->filename, uni_sizeof(p->filename));
		file_write(f, p->comment, uni_sizeof(p->comment));
	}
	return tot;
}

/*\
|*| Write out a PXX volume header
\*/
static file_t
write_pxx_header(pxx_t *pxx)
{
	file_t f;
	pxx_t data;
	pfile_t *p;
	int i;

	if (!IS_PXX(*pxx))
		return 0;
	/*\ Open output file, but check so we don't overwrite anything \*/
	if (move_away(pxx->filename, ".old")) {
		fprintf(stderr, "      WRITE ERROR: %s: ",
				stuni(pxx->filename));
		fprintf(stderr, "File exists\n");
		return 0;
	}
	f = file_open(pxx->filename, 1);
	if (!f) {
		fprintf(stderr, "      WRITE ERROR: %s: ",
				stuni(pxx->filename));
		perror("");
		return 0;
	}
	pxx->file_list = PXX_FIX_HEAD_SIZE + uni_sizeof(pxx->comment);
	pxx->parity_data = pxx->file_list + write_file_entries(0, pxx->files);

	for (i = 0, p = pxx->files; p; p = p->next, i++) {
		if (pxx->parity_data_size < p->file_size)
			pxx->parity_data_size = p->file_size;
	}
	pxx_endian_write(pxx, &data);

	file_write(f, &data, PXX_FIX_HEAD_SIZE);
	file_write(f, pxx->comment, uni_sizeof(pxx->comment));
	write_file_entries(f, pxx->files);
	return f;
}

/*\
|*| Write out a PAR file
\*/
hfile_t *
write_par_header(par_t *par)
{
	file_t f;
	par_t data;

	if (!IS_PAR(*par)) {
		fprintf(stderr, "      WRITE ERROR: %s: ",
				stuni(par->filename));
		fprintf(stderr, "Not a PAR file\n");
		return 0;
	}

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

	par->file_list = PAR_FIX_HEAD_SIZE + uni_sizeof(par->comment);
	par->volume_list = par->file_list + write_file_entries(0, par->files);

	par_endian_write(par, &data);

	file_write(f, &data, PAR_FIX_HEAD_SIZE);
	file_write(f, par->comment, uni_sizeof(par->comment));
	write_file_entries(f, par->files);
	write_file_entries(f, par->volumes);

	if (cmd.ctrl)
		file_add_md5(f, 0x0026, 0x0036);

	if (file_close(f) < 0) {
		fprintf(stderr, "      WRITE ERROR: %s: ",
				stuni(par->filename));
		perror("");
		return 0;
	}
	return hfile_add(par->filename);
}

/*\
|*| Restore missing files with recovery volumes
\*/
int
restore_files(pfile_t *files, pfile_t *volumes)
{
	int N, M, i, j;
	xfile_t *in, *out;
	pfile_t *p, *v, **pp;
	int fail = 0;
	size_t size;
	u16 *fnrs;

	/*\ Copy files that have their status bit set \*/
	p = files;
	pp = &files;
	*pp = 0;
	for (; p; p = p->next) {
		if (!(p->status & 0x1))
			continue;
		CNEW(*pp, 1);
		COPY(*pp, p, 1);
		pp = &((*pp)->next);
		*pp = 0;
	}

	size = 0;
	/*\ Count number of files \*/
	for (M = N = 0, p = files; p; p = p->next) {
		N++;
		if (p->file_size > size)
			size = p->file_size;
		if (!find_file(p, 0))
			M++;
	}

	/*\ Count missing volumes \*/
	for (v = volumes; v; v = v->next)
		if (!find_file(v, 0) || v->crt)
			M++;

	NEW(in, N + 1);
	NEW(out, M + 1);
	NEW(fnrs, N + 1);

	for (i = 0; i < N; i++)
		fnrs[i] = i + 1;
	fnrs[N] = 0;

	/*\ Fill in input files \*/
	for (i = 0, p = files, v = volumes; i < N; i++, p = p->next) {
		if (p->match) {
			in[i].filenr = i + 1;
			in[i].files = 0;
			in[i].size = p->file_size;
			in[i].f = file_open(p->match->filename, 0);
		} else {
			pxx_t *pxx;

			while (!v->match || v->crt)
				v = v->next;
			pxx = read_pxx_header(stuni(v->match->filename),
					0, 0, 0);
			if (!pxx) {
				fprintf(stderr, "Internal error!\n");
				in[i].f = 0;
				continue;
			}
			in[i].filenr = pxx->vol_number;
			in[i].files = fnrs;
			if (pxx->version < 0x80) in[i].filenr = 1;
			in[i].size = pxx->parity_data_size;
			in[i].f = pxx->f;
			pxx->f = 0;
			free_pxx(pxx);
			v = v->next;
		}
	}
	in[i].filenr = 0;

	/*\ Fill in output files \*/
	for (i = j = 0, p = files; p; p = p->next, i++) {
		if (p->match) continue;
		out[j].size = p->file_size;
		out[j].filenr = i + 1;
		out[j].files = 0;
		out[j].f = 0;
		/*\ Open output file, but check we don't overwrite anything \*/
		if (move_away(p->filename, ".bad")) {
			fprintf(stderr, "      ERROR: %s: ",
				stuni(p->filename));
			fprintf(stderr, "File exists\n");
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
				stuni(p->filename));
			continue;
		}
		out[j].f = file_open(p->filename, 1);
		if (!out[j].f) {
			fprintf(stderr, "      ERROR: %s: ",
				stuni(p->filename));
			perror("");
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
				stuni(p->filename));
			continue;
		}
		j++;
	}

	/*\ Fill in output volumes \*/
	for (v = volumes; v; v = v->next) {
		pxx_t *pxx;
		if (v->match) {
			if (!v->crt) continue;
		} else {
			if (!cmd.rvol) continue;
		}

		pxx = read_pxx_header(stuni(v->filename), 0, v->vol_number, 0);
		if (!pxx) {
			fprintf(stderr, "  %-40s - FAILED\n",
					stuni(v->match->filename));
			continue;
		}
		/*\ Copy file list into pxx file \*/
		pxx->files = files;
		pxx->parity_data_size = size;
		out[j].f = write_pxx_header(pxx);
		pxx->files = 0;
		if (!out[j].f) {
			fprintf(stderr, "  %-40s - FAILED\n",
					stuni(pxx->filename));
			fail |= 1;
			continue;
		}
		if (v->crt) {
			v->match = hfile_add(pxx->filename);
			fprintf(stderr, "  %-40s - OK\n",
					stuni(pxx->filename));
			v->filename = v->match->filename;
		}
		out[j].size = size;
		out[j].filenr = pxx->vol_number;
		out[j].files = fnrs;
		free_pxx(pxx);
		j++;
	}
	out[j].filenr = 0;

	if (!fail && !recreate(in, out))
		fail |= 1;

	for (i = 0; in[i].filenr; i++)
		file_close(in[i].f);
	for (i = 0; out[i].filenr; i++) {
		if (out[i].files && cmd.ctrl)
			file_add_md5(out[i].f, 0x0030, 0x0040);
		file_close(out[i].f);
	}

	free(in);
	free(out);

	/*\ Check resulting data files \*/
	for (p = files; p; p = p->next) {
		md5 hash;
		if (p->match) continue;
		/*\ Check the md5 sum of the resulting file \*/
		if (!file_md5(p->filename, hash)) {
			fprintf(stderr, "      ERROR: %s:", stuni(p->filename));
			perror("");
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
					stuni(p->filename));
			fail |= 1;
			continue;
		}
		if (!CMP_MD5(hash, p->hash)) {
			fprintf(stderr, "      ERROR: %s: Failed md5 check\n",
					stuni(p->filename));
			fprintf(stderr, "  %-40s - NOT RESTORED\n",
					stuni(p->filename));
			fail |= 1;
			continue;
		}
		fprintf(stderr, "  %-40s - RECOVERED\n",
				stuni(p->filename));
	}

	if (cmd.rvol) {
		/*\ Check resulting recovery volumes \*/
		for (v = volumes; v; v = v->next) {
			md5 hash;
			if (v->match) continue;
			/*\ Check the md5 sum of the resulting file \*/
			if (!file_md5(v->filename, hash)) {
				fprintf(stderr, "      ERROR: %s:",
						stuni(v->filename));
				perror("");
				fprintf(stderr, "  %-40s - NOT RESTORED\n",
						stuni(v->filename));
				fail |= 1;
				continue;
			}
			if (!CMP_MD5(hash, v->hash)) {
				fprintf(stderr, "      ERROR: %s: "
						"Failed md5 check\n",
						stuni(v->filename));
				fprintf(stderr, "  %-40s - NOT RESTORED\n",
						stuni(v->filename));
				fail |= 1;
				continue;
			}
			fprintf(stderr, "  %-40s - RECOVERED\n",
					stuni(v->filename));
		}
	}
	free_file_list(files);
	if (fail) {
		fprintf(stderr, "\nErrors occurred.\n\n");
		return -1;
	}
	return 1;
}
