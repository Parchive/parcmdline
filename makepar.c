/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Create PAR and PXX files.
\*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "util.h"
#include "rwpar.h"
#include "makepar.h"
#include "fileops.h"

static u16 uni_empty[] = { 0 };

/*\
|*| Add a data file to a PXX volume
\*/
static int
pxx_add_file(pxx_t *pxx, hfile_t *file, int displ)
{
	pfile_t **p;

	if (!file)
		return 0;
	if (!hash_file(file, HASH)) {
		if (displ)
			fprintf(stderr, "    %-36s - ERROR\n",
				stuni(file->filename));
		return 0;
	}
	for (p = &pxx->files; *p; p = &((*p)->next)) {
		switch (unicode_cmp((*p)->filename, file->filename)) {
		case 0:
			if (CMP_MD5((*p)->hash, file->hash)) {
				if (displ)
					fprintf(stderr, "    %-36s - EXISTS\n",
							stuni(file->filename));
			} else {
				if (displ)
					fprintf(stderr, "    %-36s - CLASH\n",
						stuni(file->filename));
			}
			return 0;
		case 1:
			if (CMP_MD5((*p)->hash, file->hash)) {
				if (displ)
					fprintf(stderr, "    %-36s - EXISTS\n",
						stuni(file->filename));
				return 0;
			}
			break;
		}
	}
	if (cmd.noadd) {
		if (displ)
			fprintf(stderr, "    %-36s - NOT ADDED\n",
					stuni(file->filename));
		return 0;
	}
	CNEW((*p), 1);
	(*p)->filename = file->filename;
	(*p)->match = file;
	(*p)->comment = uni_empty;
	(*p)->file_size = file->file_size;
	COPY((*p)->hash_16k, file->hash_16k, sizeof(md5));
	COPY((*p)->hash, file->hash, sizeof(md5));

	if (displ)
		fprintf(stderr, "    %-36s - OK\n", stuni(file->filename));

	return 1;
}

/*\
|*| Add a data file to a PAR file
\*/
int
par_add_file(par_t *par, hfile_t *file)
{
	pfile_t **p, *v;

	if (IS_PXX(*par))
		return pxx_add_file((pxx_t *)par, file, 1);
	if (!file)
		return 0;
	if (!hash_file(file, HASH)) {
		fprintf(stderr, "  %-40s - ERROR\n",
				stuni(file->filename));
		return 0;
	}
	for (p = &par->files; *p; p = &((*p)->next)) {
		switch (unicode_cmp((*p)->filename, file->filename)) {
		case 0:
			if (CMP_MD5((*p)->hash, file->hash)) {
				fprintf(stderr, "  %-40s - ALREADY EXISTS\n",
					stuni(file->filename));
			} else {
				fprintf(stderr, "  %-40s - NAME CLASH\n",
					stuni(file->filename));
			}
			return 0;
		case 1:
			if (CMP_MD5((*p)->hash, file->hash)) {
				fprintf(stderr, "  %-40s - EXISTS\n",
					stuni(file->filename));
				return 0;
			}
			break;
		}
	}
	CNEW((*p), 1);
	(*p)->filename = file->filename;
	(*p)->match = file;
	(*p)->comment = uni_empty;
	(*p)->file_size = file->file_size;
	COPY((*p)->hash_16k, file->hash_16k, sizeof(md5));
	COPY((*p)->hash, file->hash, sizeof(md5));
	if (!cmd.noadd)
		(*p)->status |= 0x01;

	fprintf(stderr, "  %-40s - OK\n", stuni(file->filename));

	for (v = par->volumes; v; v = v->next)
		v->crt = 1;

	return 1;
}

/*\
|*| Find a PXX volume entry with a given volume number.
|*|  Create it if it doesn't exist.
\*/
static pfile_t *
par_find_volume(par_t *par, u16 vol)
{
	pfile_t **v;

	if (!vol)
		return 0;
	if (vol > 99) {
		fprintf(stderr, "    Volume number out of range: %d\n", vol);
		return 0;
	}
	for (v = &par->volumes; *v; v = &((*v)->next)) {
		if (((*v)->vol_number) == vol) {
			break;
		}
	}

	if (!(*v)) {
		CNEW((*v), 1);
		(*v)->comment = uni_empty;
		(*v)->vol_number = vol;
		(*v)->crt = 1;
	}
	if (!(*v)->match) {
		(*v)->match = find_volume(par->filename, vol);
		if ((*v)->match)
			(*v)->filename = (*v)->match->filename;
	}
	return *v;
}

/*\
|*| Create the PXX volumes from the description in the PAR archive
\*/
int
par_make_pxx(par_t *par)
{
	pfile_t *p, *v;
	u16 nf;

	if (!IS_PAR(*par))
		return 0;
	if (cmd.volumes <= 0)
		return 0;

	/*\ Create volume file entries \*/
	for (nf = 1; nf <= cmd.volumes; nf++)
		par_find_volume(par, nf);

	fprintf(stderr, "\n\nCreating PXX volumes:\n");
	for (p = par->files; p; p = p->next)
		find_file(p, 1);

	if (restore_files(par->files, par->volumes) < 0)
		return 0;

	/*\ Check the volumes \*/
	for (v = par->volumes; v; v = v->next) {
		v->match->hashed = 0;
		if (!hash_file(v->match, HASH))
			continue;
		COPY(v->hash_16k, v->match->hash_16k, sizeof(md5));
		COPY(v->hash, v->match->hash, sizeof(md5));
		v->file_size = v->match->file_size;
	}
	return 1;
}
