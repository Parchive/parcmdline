/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  This file holds an interface abstraction,
|*|   to be called by different user interfaces.
\*/

#include "interface.h"
#include "par.h"
#include "rwpar.h"
#include "util.h"
#include "fileops.h"
#include <stdlib.h>
#include <string.h>

static pfile_t *volumes = 0, *files = 0;

/*\ Add a PARfile to the current parlist.
|*|   filename: Name of file to load
\*/
int
par_load(u16 *filename)
{
	pfile_t *p, **pp;
	par_t *par;

	if (!filename) return PAR_ERR_INVALID;

	par = read_par_header(filename, 1, 0, 0);
	if (!par) return PAR_ERR_ERRNO;
	for (p = volumes; p; p = p->next) {
		if (!unicode_cmp(p->filename, par->filename)) {
			free_par(par);
			return PAR_ERR_ALREADY_LOADED;
		}
	}
	CNEW(p, 1);
	p->match = find_file_name(par->filename, 0);
	p->vol_number = par->vol_number;
	p->file_size = par->data_size;
	if (par->files)
		p->fnrs = file_numbers(&files, &par->files);
	p->f = par->f;
	p->filename = unicode_copy(par->filename);
	par->f = 0;
	free_par(par);

	/*\ Insert in alphabetically correct place \*/
	for (pp = &volumes; *pp; pp = &((*pp)->next))
		if (unicode_gt((*pp)->filename, filename))
			break;
	p->next = *pp;
	*pp = p;
	return PAR_OK;
}

/*\ Search for additional PARfiles matching the current ones
|*|   partial: Load files that only match partially.
\*/
int
par_search(int partial)
{
	find_par_files(&volumes, &files, partial);
	return PAR_OK;
}

/*\ Remove a PARfile from the current parlist.
|*|   entry: Name of file to remove
\*/
int
par_unload(u16 *entry)
{
	pfile_t *p, **pp;

	for (pp = &volumes; *pp; pp = &((*pp)->next)) {
		if ((*pp)->filename == entry) {
			p = *pp;
			*pp = p->next;
			if (p->f) file_close(p->f);
			if (p->fnrs) free(p->fnrs);
			free(p->filename);
			free(p);
			return PAR_OK;
		}
	}
	return PAR_ERR_EXIST;
}

/*\ List the current PARfiles
|*|  Returns: Array of filenames, NULL-terminated.
|*|   Notes: Caller should free() returned array.
\*/
u16 **
par_parlist(void)
{
	u16 **ret;
	pfile_t *p;
	int n;

	for (n = 0, p = volumes; p; p = p->next)
		n++;
	CNEW(ret, n + 1);
	for (n = 0, p = volumes; p; p = p->next, n++)
		ret[n] = p->filename;
	ret[n] = 0;
	return ret;
}

/*\ List the current filelist
|*|  Returns: Array of filenames, NULL-terminated.
|*|   Notes: Caller should free() returned array.
\*/
u16 **
par_filelist(void)
{
	u16 **ret;
	pfile_t *p;
	int n;

	for (n = 0, p = files; p; p = p->next)
		n++;
	CNEW(ret, n + 1);
	for (n = 0, p = files; p; p = p->next, n++)
		ret[n] = p->filename;
	ret[n] = 0;
	return ret;
}

/*\ Check the MD5 sum of a file
|*|   entry: file to check
\*/
int
par_check(u16 *entry)
{
	pfile_t *p;

	for (p = files; p; p = p->next) {
		if (p->filename == entry) {
			if (find_file(p, 0))
				return PAR_OK;
			return PAR_ERR_CORRUPT;
		}
	}
	return PAR_ERR_EXIST;
}

/*\ Find a file by its MD5 sum
|*|   entry: the file to find
|*|  Returns: matching filename
|*|   Notes: Returned filename should NOT be free()d.
\*/
u16 *
par_find(u16 *entry)
{
	pfile_t *p;

	for (p = files; p; p = p->next) {
		if (p->filename == entry) {
			if (!find_file(p, 0))
				return 0;
			return p->match->filename;
		}
	}
	return 0;
}

/*\ Fix incorrect filenames
|*|   entry(optional): only fix this entry
\*/
int
par_fixname(u16 *entry)
{
	int err = PAR_OK;
	pfile_t *p;

	for (p = files; p; p = p->next) {
		if (entry && (p->filename != entry))
			continue;
		if (!find_file(p, 0)) {
			err = PAR_ERR_NOT_FOUND;
			continue;
		}
		if (!unicode_cmp(p->filename, p->match->filename))
			continue;
		if (rename_away(p->match->filename, p->filename))
			err = PAR_ERR_ERRNO;
	}
	return err;
}

/*\ Get status word
|*|   entry: file to get status of
|*|  Returns: status word for entry.
\*/
i64
par_getstatus(u16 *entry)
{
	pfile_t *p;

	for (p = files; p; p = p->next)
		if (p->filename == entry)
			return p->status;
	return 0;
}

/*\ Set status word
|*|   entry: file to set status for
\*/
int
par_setstatus(u16 *entry, i64 status)
{
	pfile_t *p;

	for (p = files; p; p = p->next) {
		if (p->filename == entry) {
			p->status = status;
			return PAR_OK;
		}
	}
	return PAR_ERR_EXIST;
}

/*\ Recover missing files
|*|   entry(optional): only recover this entry
\*/
int
par_recover(u16 *entry)
{
	if (entry)
		return PAR_ERR_IMPL;
	if (restore_files(files, volumes) < 0)
		return PAR_ERR_FAILED;
	return PAR_OK;
}

/*\ Add a file to the current filelist
|*|   filename: file to add
\*/
int
par_addfile(u16 *filename)
{
	pfile_t *p, **pp;
	hfile_t *file;

	if (!filename) return PAR_ERR_INVALID;

	file = find_file_name(filename, 0);
	if (!file)
		return PAR_ERR_NOT_FOUND;
	if (!hash_file(file, HASH))
		return PAR_ERR_ERRNO;

	for (p = files; p; p = p->next) {
		if (!unicode_cmp(p->filename, filename)) {
			if (CMP_MD5(p->hash, file->hash))
				return PAR_ERR_ALREADY_LOADED;
			else
				return PAR_ERR_CLASH;
		}
	}
	/*\ Create new entry \*/
	CNEW(p, 1);
	p->filename = file->filename;
	p->match = file;
	p->file_size = file->file_size;
	COPY(p->hash, file->hash, sizeof(md5));
	COPY(p->hash_16k, file->hash_16k, sizeof(md5));
	p->status |= 0x01;

	/*\ Insert in alphabetically correct place \*/
	for (pp = &files; *pp; pp = &((*pp)->next))
		if (unicode_gt((*pp)->filename, p->filename))
			break;
	p->next = *pp;
	*pp = p;

	return PAR_OK;
}

/*\ Remove a file from the current filelist
|*|   entry: file to remove
\*/
int
par_removefile(u16 *entry)
{
	return PAR_ERR_IMPL;
}

/*\ Add new PARfiles to the current parlist
|*|   number: the highest volume number to create
\*/
int
par_addpars(u16 *entry, int number)
{
	int i, err = PAR_OK;
	pfile_t *p, **pp;
	hfile_t *h;

	if (number < 1) return PAR_ERR_INVALID;
	for (p = volumes; p; p = p->next)
		if (p->filename == entry)
			break;
	if (!p) return PAR_ERR_INVALID;
	for (i = 1; i <= number; i++) {
		h = find_volume(entry, i);
		if (!h) {
			err = PAR_ERR_ERRNO;
			continue;
		}
		for (p = volumes; p; p = p->next)
			if (!unicode_cmp(p->filename, h->filename))
				break;
		if (p) continue;
		CNEW(p, 1);
		p->match = h;
		p->vol_number = i;
		p->filename = unicode_copy(h->filename);

		/*\ Insert in alphabetically correct place \*/
		for (pp = &volumes; *pp; pp = &((*pp)->next))
			if (unicode_gt((*pp)->filename, p->filename))
				break;
		p->next = *pp;
		*pp = p;
	}
	return err;
}

/*\ Create PARfiles from the current filelist
|*|   entry(optional): only create this entry
\*/
int
par_create(u16 *entry)
{
	pfile_t *p;

	if (entry)
		return PAR_ERR_IMPL;
	for (p = volumes; p; p = p->next) {
		if (p->f) {
			file_close(p->f);
			p->f = 0;
		}
		if (!p->vol_number) {
			par_t *par;
			par = create_par_header(p->filename, 0);
			par->files = files;
			write_par_header(par);
			par->files = 0;
			free_par(par);
		}
		if (!p->fnrs)
			p->fnrs = file_numbers(&files, &files);
	}
	if (restore_files(files, volumes) < 0)
		return PAR_ERR_FAILED;
	return PAR_OK;
}
