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
#include "backend.h"
#include "rwpar.h"
#include "checkpar.h"
#include "fileops.h"

/*\
|*| Check the data files in a PXX file, and maybe restore missing files
\*/
int
check_par(par_t *par)
{
	int m;
	pfile_t *p;

	/*\ Look for all the data files \*/
	for (m = 0, p = par->files; p; p = p->next) {
		if (!find_file(p, 1) && USE_FILE(p))
			m++;
	}
	if (m == 0) {
		fprintf(stderr, "All files found\n");
		return 0;
	}
	if ((cmd.action != ACTION_MIX) && (find_volumes(par, m) < m)) {
		fprintf(stderr, "\nToo many missing files:\n");
		for (p = par->files; p; p = p->next) {
			if (!p->match && USE_FILE(p))
				fprintf(stderr, " %s\n", basename(p->filename));
		}
		return -1;
	}
	if (cmd.action != ACTION_CHECK) {
		fprintf(stderr, "\nRestoring:\n");
		return restore_files(par->files, par->volumes);
	}
	fprintf(stderr, "\nRestorable:\n");
	for (p = par->files; p; p = p->next) {
		if ((!p->match) && USE_FILE(p))
			fprintf(stderr, "  %-40s - can be restored\n",
				basename(p->filename));
	}
	return 1;
}
