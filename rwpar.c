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
	for (l = 0; str[l]; l++)
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
find_volume(u16 *name, i64 vol)
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
		if (a && !(a->status & 0x1)) {
			a = a->next;
			continue;
		}
		if (b && !(b->status & 0x1)) {
			b = b->next;
			continue;
		}
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
find_volumes(par_t *par, int tofind)
{
	int m;
	pfile_t *v;
	hfile_t *p;
	par_t *tmp;

	v = 0;
	m = 0;
	if (par->vol_number) {
		CNEW(v, 1);
		v->match = find_file_path(stuni(par->filename));
		v->vol_number = par->vol_number;
		m++;
	}
	free_file_list(par->volumes);
	par->volumes = v;
	if (m >= tofind) return m;
	fprintf(stderr, "\nLooking for %sPXX volumes:\n",
			m ? "additional " : "");
	for (p = hfile; p && (m < tofind); p = p->next) {
		tmp = read_par_header(stuni(p->filename), 0, 0, 1);
		if (!tmp) continue;
		if (tmp->vol_number && files_equal(tmp->files, par->files)) {
			for (v = par->volumes; v; v = v->next) {
				if (v->vol_number == tmp->vol_number)
					break;
			}
			if (!v) {
				CNEW(v, 1);
				v->match = p;
				v->vol_number = tmp->vol_number;
				v->next = par->volumes;
				par->volumes = v;
				m++;
				fprintf(stderr, "  %-40s - OK\n",
					stuni(p->filename));
			}
		}
		free_par(tmp);
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
	COPY(file->filename, pf->filename, l);
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
create_par_header(char *file, i64 vol)
{
	par_t *par;

	CNEW(par, 1);
	par->magic = PAR_MAGIC;
	par->version = 0x00000900;
	par->client = 0x02000400;
	par->vol_number = vol;
	par->filename = make_uni_str(file);
	par->comment = uni_empty;

	return par;
}

/*\
|*| Read in a PAR file, and return it into a newly allocated struct
|*| (to be freed with free_par())
\*/
par_t *
read_par_header(char *file, int create, i64 vol, int silent)
{
	par_t par, *r;
	i64 i, n;
	md5 hash;

	if (!(par.f = file_open_ascii(file, 0))) {
		if (!create) {
			if (!silent)
				perror("Error opening PAR file");
			return 0;
		}
		return create_par_header(file, vol);
	}

	/*\ Read in the first part of the struct, it fits directly on top \*/
	if (file_read(par.f, &par, PAR_FIX_HEAD_SIZE) < PAR_FIX_HEAD_SIZE) {
		if (!silent)
			perror("Error reading file");
		file_close(par.f);
		return 0;
	}
	/*\ Is it the right file type ? \*/
	if (!IS_PAR(par)) {
		if (!silent)
			fprintf(stderr, "Not a PAR file\n");
		file_close(par.f);
		return 0;
	}
	par_endian_read(&par);

	/*\ Check version number \*/
	if (par.version != 0x0900) {
		if (!silent)
			fprintf(stderr, "PAR Version mismatch! (%0x04x)\n",
					par.version);
		file_close(par.f);
		return 0;
	}

	/*\ Check md5 control hash \*/
	if (cmd.ctrl && (!file_get_md5(par.f, 0x20, hash) ||
			!CMP_MD5(par.control_hash, hash)))
	{
		if (!silent)
			fprintf(stderr, "PAR file corrupt:"
					"control hash mismatch!\n");
		file_close(par.f);
		return 0;
	}

	if (cmd.ctrl || (par.file_list != PAR_FIX_HEAD_SIZE))
		file_seek(par.f, par.file_list);

	/*\ Read in the filelist. \*/
	par.files = read_pfiles(par.f, par.file_list_size);

	if (par.data != par.file_list + par.file_list_size)
		file_seek(par.f, par.data);

	par.filename = make_uni_str(file);

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
	free(par);
}

/*\
|*| Write out a PAR file entry from a file struct
\*/
static void
write_pfile(pfile_t *file, pfile_entr_t *pf)
{
	i64 i;

	i = FILE_ENTRY_FIX_SIZE + uni_sizeof(file->filename);

	write_i64(i, &pf->size);
	write_i64(file->status, &pf->status);
	write_i64(file->file_size, &pf->file_size);
	COPY(pf->hash, file->hash, sizeof(md5));
	COPY(pf->hash_16k, file->hash_16k, sizeof(md5));
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

	tot = 0;
	for (p = files; p; p = p->next) {
		tot += FILE_ENTRY_FIX_SIZE
		    + uni_sizeof(p->filename);
	}
	if (f) {
		for (p = files; p; p = p->next) {
			write_pfile(p, &pfe);
			file_write(f, &pfe, FILE_ENTRY_FIX_SIZE);
			file_write(f, p->filename, uni_sizeof(p->filename));
		}
	}
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
	memset(par->set_hash, 0, sizeof(md5));
	par->num_files = 0;
	for (p = par->files; p; p = p->next) {
		for (i = 0; i < 16; i++)
			par->set_hash[i] ^= p->hash[i];
		par->num_files++;
	}

	if (cmd.loglevel > 1)
		dump_par(par);

	par_endian_write(par, &data);

	file_write(f, &data, PAR_FIX_HEAD_SIZE);
	write_file_entries(f, par->files);

	if (par->vol_number == 0) {
		file_write(f, par->comment, par->data_size);
		if (cmd.ctrl)
			file_add_md5(f, 0x0010, 0x0020);
		file_close(f);
	}
	return f;
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
			par_t *par;

			while (!v->match || v->crt)
				v = v->next;
			par = read_par_header(stuni(v->match->filename),
					0, 0, 0);
			if (!par) {
				fprintf(stderr, "Internal error!\n");
				in[i].f = 0;
				continue;
			}
			in[i].filenr = par->vol_number;
			in[i].files = fnrs;
			in[i].size = par->data_size;
			in[i].f = par->f;
			par->f = 0;
			free_par(par);
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
		par_t *par;
		if (!v->match || !v->crt)
			continue;

		par = read_par_header(stuni(v->filename), 1, v->vol_number, 0);
		if (!par) {
			fprintf(stderr, "  %-40s - FAILED\n",
					stuni(v->match->filename));
			continue;
		}
		/*\ Copy file list into par file \*/
		par->files = files;
		par->data_size = size;
		out[j].f = write_par_header(par);
		par->files = 0;
		if (!out[j].f) {
			fprintf(stderr, "  %-40s - FAILED\n",
					stuni(par->filename));
			fail |= 1;
			continue;
		}
		if (v->crt) {
			v->match = hfile_add(par->filename);
			fprintf(stderr, "  %-40s - OK\n",
					stuni(par->filename));
			v->filename = v->match->filename;
		}
		out[j].size = size;
		out[j].filenr = par->vol_number;
		out[j].files = fnrs;
		free_par(par);
		j++;
	}
	out[j].filenr = 0;

	if (!fail && !recreate(in, out))
		fail |= 1;

	for (i = 0; in[i].filenr; i++)
		file_close(in[i].f);
	for (i = 0; out[i].filenr; i++) {
		if (out[i].files && cmd.ctrl)
			file_add_md5(out[i].f, 0x0010, 0x0020);
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

	free_file_list(files);
	if (fail) {
		fprintf(stderr, "\nErrors occurred.\n\n");
		return -1;
	}
	return 1;
}
