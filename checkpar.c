/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Check PAR files and restore missing files.
\*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "util.h"
#include "rwpar.h"
#include "checkpar.h"
#include "fileops.h"

/*\
|*| Check the data files in a PXX file, and maybe restore missing files
\*/
static int
check_pxx(pxx_t *pxx)
{
	int m;
	pfile_t volume, *missing = 0, *p;

	file_close(pxx->f);
	pxx->f = 0;

	/*\ Look for all the data files \*/
	m = 0;
	for (p = pxx->files; p; p = p->next) {
		if (!find_file(p, 1)) {
			m++;
			missing = p;
		}
	}
	if (m == 0) {
		fprintf(stderr, "All files found\n");
		return 0;
	}
	if (find_volumes(pxx, m) < m) {
		fprintf(stderr, "\nToo many missing files:\n");
		for (p = pxx->files; p; p = p->next) {
			if (!p->match)
				fprintf(stderr, " %s\n", stuni(p->filename));
		}
		return -1;
	}
	if (cmd.action != ACTION_CHECK) {
		fprintf(stderr, "\nRestoring:\n");
		return restore_files(pxx->files, pxx->volumes);
	}
	fprintf(stderr, "\nRestorable:\n");
	for (p = pxx->files; p; p = p->next) {
		if (!p->match)
			fprintf(stderr, "  %-40s - can be restored\n",
				stuni(p->filename));
	}
	return 1;
}

/*\
|*| Check the data files in a PAR file, and maybe restore missing files
\*/
int
check_par(par_t *par)
{
	i64 i, m, mvol, tvol;
	hfile_t **match;
	pfile_t *p, *v;
	int fail = 0;
	par_t *pxx;

	if (IS_PXX(*par))
		return check_pxx((pxx_t *)par);

	/*\ First, search for all the needed files \*/
	fprintf(stderr, "Looking for PXX volumes\n");
	tvol = mvol = 0;
	for (v = par->volumes; v; v = v->next) {
		tvol++;
		if (find_file(v, 1)) {
			mvol++;
		}
	}

	fprintf(stderr, "Looking for data files\n");
	m = 0;
	for (p = par->files; p; p = p->next) {
		if (!find_file(p, 1)) {
			m++;
		}
	}
	if ((m == 0) && (!cmd.rvol || (tvol == mvol))) {
		fprintf(stderr, "\nAll files found\n");
		return 0;
	}
	/*\ We don't do old-version PAR files \*/
	if (par->version < 0x80) {
		pxx_t *pxx;
		fprintf(stderr, "\nOld-style PAR file, Checking PXX files\n");
		fail = 0;
		for (v = par->volumes; v; v = v->next) {
			if (!find_file(v, 0))
				continue;
			pxx = read_pxx_header(stuni(v->match->filename),
					0, 0, 0);
			if (!pxx) {
				fprintf(stderr, "!\n");
				continue;
			}
			fprintf(stderr, "\n %s:\n", stuni(v->match->filename));
			fail |= check_pxx(pxx);
			free_pxx(pxx);
		}
		return fail;
	}
	/*\ If more files are missing than recovery volumes exist,
	|*| the files cannot be recovered
	\*/
	if (m > tvol) {
		fprintf(stderr, "\n\nToo many missing files:\n");
		for (p = par->files; p; p = p->next) {
			if (!p->match) {
				fprintf(stderr, "  %s\n", stuni(p->filename));
			}
		}
		fprintf(stderr, "\nErrors occurred.\n\n");
		return -1;
	}
	if (m > mvol) {
		fprintf(stderr, "\n\nMissing PAR files: (%lld needed)\n",
				m - mvol);
		for (v = par->volumes; v; v = v->next) {
			if (!v->match) {
				fprintf(stderr, "  %s\n", stuni(v->filename));
			}
		}
		fprintf(stderr, "\nErrors occurred.\n\n");
		return -1;
	}
	if (cmd.action == ACTION_CHECK) {
		fprintf(stderr, "\n\nRestorable:\n");
		for (p = par->files; p; p = p->next) {
			if (!p->match) {
				fprintf(stderr, "  %-40s - restorable\n",
						stuni(p->filename));
			}
		}
		if (cmd.rvol) for (p = par->volumes; p; p = p->next) {
			if (!p->match) {
				fprintf(stderr, "  %-40s - restorable\n",
						stuni(p->filename));
			}
		}
		return 1;
	}
	fprintf(stderr, "\n\nRestoring:\n");
	return restore_files(par->files, par->volumes);
}
