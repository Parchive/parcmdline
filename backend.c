/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Backend operations
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
#include "backend.h"
#include "md5.h"

/*\
|*| Static directory list
\*/
hfile_t *hfile = 0;

char *
basename(u16 *path)
{
	u16 *ret;

	for (ret = path; *path; path++)
		if (*path == DIR_SEP)
			ret = path + 1;
	return stuni(ret);
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
|*| Rename a file, and move the directory entry with it.
\*/
static int
rename_file(u16 *src, u16 *dst)
{
	hfile_t *p;
	int n;

	fprintf(stderr, "    Rename: %s", basename(src));
	fprintf(stderr, " -> %s", basename(dst));
	n = file_rename(src, dst);
	if (n) {
		perror(" ");
		return n;
	}
	fprintf(stderr, "\n");

	/*\ Change directory entries \*/

	for (p = hfile; p; p = p->next) {
		if (!unicode_cmp(p->filename, src)) {
			free(p->filename);
			p->filename = unicode_copy(dst);
		}
	}
	return 0;
}

hfile_t *
hfile_add(u16 *filename)
{
	int i;
	hfile_t **pp;

	for (pp = &hfile; *pp; pp = &((*pp)->next))
		;
	CNEW(*pp, 1);
	(*pp)->filename = unicode_copy(filename);
	return (*pp);
}

/*\
|*| Read in a directory and add it to the static directory structure
\*/
void
hash_directory(char *dir)
{
	hfile_t *p, *q, **pp;

	/*\ only add new items \*/
	for (p = read_dir(dir); p; ) {
		for (pp = &hfile; *pp; pp = &((*pp)->next))
			if (!unicode_cmp(p->filename, (*pp)->filename))
				break;
		if (*pp) {
			q = p;
			p = p->next;
			free(q);
		} else {
			*pp = p;
			p = p->next;
			(*pp)->next = 0;
		}
	}
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
|*| Rename a file, but first move away the destination
\*/
int
rename_away(u16 *src, u16 *dst)
{
	if (move_away(dst, ".bad")) {
		fprintf(stderr, "    Rename: %s", basename(src));
		fprintf(stderr, " -> %s : ", basename(dst));
		fprintf(stderr, "File exists\n");
		return -1;
	} else if (rename_file(src, dst)) {
		return -1;
	} else {
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

	/*\ Check filename (caseless) and then check md5 hash \*/
	for (p = hfile; p; p = p->next) {
		cm = unicode_cmp(p->filename, file->filename);
		if (cm < 0) continue;
		if (!hash_file(p, HASH)) {
			if (displ) {
				fprintf(stderr, "      ERROR: %s",
						basename(p->filename));
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
					basename(p->filename));
		corr = 1;
	}
	if (file->match) {
		if (displ)
			fprintf(stderr, "  %-40s - OK\n",
				basename(file->filename));
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
				fprintf(stderr, "  %-40s - FOUND",
					basename(file->filename));
				fprintf(stderr, ": %s\n",
					basename(p->filename));
			}
			if (!displ || !cmd.dupl)
				return 1;
		}
		fprintf(stderr, "    Duplicate: %s",
			stuni(file->match->filename));
		fprintf(stderr, " == %s\n",
			basename(p->filename));
	}
	if (!file->match && displ)
		fprintf(stderr, "  %-40s - %s\n",
			basename(file->filename),
			corr ? "CORRUPT" : "NOT FOUND");
	return (file->match != 0);
}

/*\
|*| Find a file in the static directory structure
\*/
hfile_t *
find_file_name(u16 *path, int displ)
{
	hfile_t *p, *ret = 0;

	hash_directory(stuni(path));
	path = unist(complete_path(stuni(path)));

	/*\ Check filename (caseless) and then check md5 hash \*/
	for (p = hfile; p; p = p->next) {
		switch (unicode_cmp(p->filename, path)) {
		case 1:
			if (ret) break;
		case 0:
			ret = p;
		}
	}
	if (!ret && displ)
		fprintf(stderr, "  %-40s - NOT FOUND\n", basename(path));
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
	u16 *filename;
	i64 i;
	hfile_t *p, *ret = 0;
	int nd, v;

	if (vol < 1)
		return 0;
	nd = 2;
	for (v = 100; vol >= v; v *= 10)
		nd++;
	for (i = 0; name[i]; i++)
		;
	if ((name[i-1] < '0') || (name[i-1] > '9')) {
		i = i - 2;
	} else {
		while ((name[i-1] >= '0') && (name[i-1] <= '9'))
			i--;
	}
	i += nd;
	NEW(filename, i + 1);
	uni_copy(filename, name, i);
	filename[i] = 0;
	v = vol;
	while (--nd >= 0) {
		filename[--i] = '0' + v % 10;
		v /= 10;
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
	free(filename);
	return ret;
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
		/*\ Look for a match \*/
		for (j = 1, qq = list; *qq; qq = &((*qq)->next), j++) {
			if ((*files)->file_size != (*qq)->file_size) continue;
			if (!CMP_MD5((*files)->hash, (*qq)->hash)) continue;
			break;
		}
		if (USE_FILE(*files))
			fnrs[i++] = j;
		/*\ No match ? Move the file entry to the tail of the list \*/
		if (!*qq) {
			*qq = *files;
			*files = (*files)->next;
			(*qq)->next = 0;
		} else {
			files = &((*files)->next);
		}
	}
	fnrs[i] = 0;
	return fnrs;
}

int
par_control_check(par_t *par)
{
	md5 hash;

	if (!cmd.ctrl) return 1;

	/*\ Check version number \*/
	if (par->version > 0x0001ffff) {
		fprintf(stderr, "%s: PAR Version mismatch! (%x.%x)\n",
				basename(par->filename), par->version >> 16,
				(par->version & 0xffff) >> 8);
		return 0;
	}

	/*\ Check md5 control hash \*/
	if ((!file_get_md5(par->f, par->control_hash_offset, hash) ||
			!CMP_MD5(par->control_hash, hash)))
	{
		fprintf(stderr, "%s: PAR file corrupt:"
				"control hash mismatch!\n",
				basename(par->filename));
		return 0;
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
	if (par->vol_number) {
		CNEW(v, 1);
		v->match = find_file_name(par->filename, 1);
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
					basename(p->filename));
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
				basename(p->filename));
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

	hash_directory(".");

	for (p = hfile; p; p = p->next) {
		if ((p->hashed >= HASH16K) && !IS_PAR(*p)
				&& !is_old_par(&p->magic))
			continue;
		tmp = read_par_header(p->filename, 0, 0, 1);
		if (!tmp) continue;
		if (tmp->vol_number) {
			if (!par_control_check(tmp)) {
				fprintf(stderr, "  %-40s - CORRUPT\n",
					basename(p->filename));
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
					basename(p->filename));
		}
		free_par(tmp);
	}
	if (!par->volumes) {
		free_par(par);
		return 0;
	}
	return par;
}

/*\ Calc sub pattern for a sub string, Return result with rhs tacked on. \*/
static sub_t *
make_sub_r(u16 *from, int fl, u16 *to, int tl, int off, sub_t *rhs)
{
	int i, j, k, m;
	int ml, mf, mt;
	sub_t *ret;

	/*\ Cut off matching front and rear \*/
	while ((*from == *to) && (fl > 0) && (tl > 0)) {
		from++;
		to++;
		fl--;
		tl--;
		off++;
	}
	while ((from[fl-1] == to[tl-1]) && (fl > 0) && (tl > 0)) {
		fl--;
		tl--;
	}

	/*\ Complete match \*/
	if ((fl == 0) && (tl == 0))
		return rhs;

	/*\ Look for longest match \*/
	ml = 0, mf = 0, mt = 0;
	for (i = 0; (fl - i) > ml; i++) {
		for (j = 0; (tl - j) > ml; j++) {
			m = (fl - i);
			if (m > (tl - j)) m = (tl - j);
			for (k = 0; k < m; k++)
				if (from[i + k] != to[j + k])
					break;
			if (k > ml) {
				ml = k;
				mf = i;
				mt = j;
			}
		}
	}
	/*\ No match found \*/
	if (ml == 0) {
		NEW(ret, 1);
		ret->next = rhs;
		ret->off = off;
		ret->fs = from;
		ret->fl = fl;
		ret->ts = to;
		ret->tl = tl;
		return ret;
	}
	/*\ Match found: recurse with left and right parts \*/
	ret = make_sub_r(from + mf + ml, fl - (mf + ml),
			to + mt + ml, tl - (mt + ml), off + mf + ml, rhs);
	return make_sub_r(from, mf, to, mt, off, ret);
}

/*\
|*| Calculate a substitution pattern
\*/
sub_t *
make_sub(u16 *from, u16 *to)
{
	int fl, tl;

	for (fl = 0; from[fl]; fl++) ;
	for (tl = 0; to[tl]; tl++) ;
	return make_sub_r(from, fl, to, tl, 0, 0);
}

/*\
|*|  Free s sub pattern.
\*/
void
free_sub(sub_t *sub)
{
	sub_t *t;
	while (sub) {
		t = sub;
		sub = sub->next;
		free(t);
	}
}

/*\
|*| Pass the string through the substitution pattern
\*/
u16 *
do_sub(u16 *from, sub_t *sub)
{
	static u16 *ret = 0;
	static int retl = 0;
	int i, fp, rp;
	sub_t *p;

	for (i = 0; from[i]; i++) ;
	for (p = sub; p; p = p->next)
		i += (p->tl - p->fl);

	if (retl < i+1) {
		retl = i+1;
		RENEW(ret, retl);
	}

	fp = rp = 0;
	for (p = sub; p; p = p->next) {
		while (fp < p->off) {
			if (!from[fp])
				return from;
			ret[rp++] = from[fp++];
		}
		for (i = 0; i < p->fl; i++) {
			if (from[fp++] != p->fs[i])
				return from;
		}
		for (i = 0; i < p->tl; i++)
			ret[rp++] = p->ts[i];
	}
	while (from[fp])
		ret[rp++] = from[fp++];
	ret[rp] = 0;
	return ret;
}

/*\
|*| Look for the sub pattern that makes most of the filenames match.
|*|  If less than m files match any pattern, 0 is returned.
\*/
sub_t *
find_best_sub(pfile_t *files, int m)
{
	sub_t *best, *cur;
	pfile_t *p, *q;
	u16 *str;
	int i;

	best = 0;

	for (p = files; p; p = p->next) {
		if (!find_file(p, 0))
			continue;
		cur = make_sub(p->filename, p->match->filename);
		i = 0;
		for (q = files; q; q = q->next) {
			if (!find_file(q, 0))
				continue;
			if (!unicode_cmp(do_sub(q->filename, cur),
						q->match->filename))
				i++;
		}
		if (i > m) {
			free_sub(best);
			best = cur;
		} else {
			free_sub(cur);
		}
	}
	return best;
}
