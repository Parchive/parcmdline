/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Create PAR files.
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
|*| Add a data file to a PAR file
\*/
int
par_add_file(par_t *par, hfile_t *file)
{
	pfile_t **p, *v;

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
	(*p)->file_size = file->file_size;
	COPY((*p)->hash, file->hash, sizeof(md5));
	COPY((*p)->hash_16k, file->hash_16k, sizeof(md5));
	if (cmd.add)
		(*p)->status |= 0x01;

	fprintf(stderr, "  %-40s - OK\n", stuni(file->filename));

	return 1;
}

/*\
|*| Create the PAR volumes from the description in the PAR archive
\*/
int
par_make_pxx(par_t *par)
{
	pfile_t *p, *v;
	int nf, M;

	if (!IS_PAR(*par))
		return 0;
	if (cmd.volumes <= 0)
		return 0;

	M = cmd.volumes;
	if (cmd.pervol) {
		for (M = 0, p = par->files; p; p = p->next)
			if (p->status & 0x1)
				M++;
		M = ((M - 1) / cmd.volumes) + 1;
	}

	/*\ Create volume file entries \*/
	for (nf = 1; nf <= M; nf++) {
		CNEW(v, 1);
		v->match = find_volume(par->filename, nf);
		v->vol_number = nf;
		v->crt = 1;
		if (v->match)
			v->filename = v->match->filename;
		v->next = par->volumes;
		par->volumes = v;
	}

	fprintf(stderr, "\n\nCreating PAR volumes:\n");
	for (p = par->files; p; p = p->next)
		if (p->status & 0x1)
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
