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
#include "backend.h"
#include "makepar.h"
#include "rwpar.h"
#include "fileops.h"

/*\
|*| Add a data file to a PAR file
\*/
int
par_add_file(par_t *par, hfile_t *file)
{
	pfile_t *p, **pp;

	if (!file)
		return 0;
	if (!hash_file(file, HASH)) {
		fprintf(stderr, "  %-40s - ERROR\n",
				basename(file->filename));
		return 0;
	}
	/*\ Check if the file exists \*/
	for (p = par->files; p; p = p->next) {
		switch (unicode_cmp(p->filename, file->filename)) {
		case 0:
			if (CMP_MD5(p->hash, file->hash)) {
				fprintf(stderr, "  %-40s - EXISTS\n",
					basename(file->filename));
			} else {
				fprintf(stderr, "  %-40s - NAME CLASH\n",
					basename(file->filename));
			}
			return 0;
		case 1:
			if (CMP_MD5(p->hash, file->hash)) {
				fprintf(stderr, "  %-40s - EXISTS\n",
					basename(file->filename));
				return 0;
			}
			break;
		}
	}

	/*\ Create new entry \*/
	CNEW(p, 1);
	p->filename = file->filename;
	p->match = file;
	p->file_size = file->file_size;
	COPY(p->hash, file->hash, sizeof(md5));
	COPY(p->hash_16k, file->hash_16k, sizeof(md5));
	if (cmd.add)
		p->status |= 0x01;

	/*\ Insert in alphabetically correct place \*/
	for (pp = &par->files; *pp; pp = &((*pp)->next))
		if (unicode_gt((*pp)->filename, p->filename))
			break;
	p->next = *pp;
	*pp = p;

	fprintf(stderr, "  %-40s - OK\n", basename(file->filename));

	return 1;
}

/*\
|*| Create the PAR volumes from the description in the PAR archive
\*/
int
par_make_pxx(par_t *par)
{
	pfile_t *p, *v;
	int M, i;

	if (!IS_PAR(*par))
		return 0;
	if (par->vol_number) {
		CNEW(v, 1);
		v->match = find_file_name(par->filename, 0);
		if (!v->match)
			v->match = find_volume(par->filename, par->vol_number);
		v->vol_number = par->vol_number;
		if (v->match)
			v->filename = v->match->filename;
		par->volumes = v;
	} else {
		if (cmd.volumes <= 0)
			return 0;

		M = cmd.volumes;
		if (cmd.pervol) {
			for (M = 0, p = par->files; p; p = p->next)
				if (USE_FILE(p))
					M++;
			M = ((M - 1) / cmd.volumes) + 1;
		}

		/*\ Create volume file entries \*/
		for (i = 1; i <= M; i++) {
			CNEW(v, 1);
			v->match = find_volume(par->filename, i);
			v->vol_number = i;
			if (v->match)
				v->filename = v->match->filename;
			v->next = par->volumes;
			par->volumes = v;
		}
	}

	fprintf(stderr, "\n\nCreating PAR volumes:\n");
	for (p = par->files; p; p = p->next)
		if (USE_FILE(p))
			find_file(p, 1);

	for (v = par->volumes; v; v = v->next)
		v->fnrs = file_numbers(&par->files, &par->files);

	if (restore_files(par->files, par->volumes, 0) < 0)
		return 0;

	return 1;
}
