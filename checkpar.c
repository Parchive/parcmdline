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
	u16 *str;
	sub_t *sub = 0;

	/*\ Look for all the data files \*/
	for (m = 0, p = par->files; p; p = p->next) {
		if (!find_file(p, 1) && USE_FILE(p))
			m++;
	}
	if (cmd.smart)
		sub = find_best_sub(par->files, 2);
	if (cmd.fix) {
		for (p = par->files; p; p = p->next) {
			if (!find_file(p, 0))
				continue;
			str = do_sub(p->filename, sub);
			if (!unicode_cmp(str, p->match->filename))
				continue;
			rename_away(p->match->filename, str);
		}
	}
	if (m == 0) {
		fprintf(stderr, "All files found\n");
		free_sub(sub);
		return 0;
	}
	if ((cmd.action != ACTION_MIX) && (find_volumes(par, m) < m)) {
		fprintf(stderr, "\nToo many missing files:\n");
		for (p = par->files; p; p = p->next) {
			if (!p->match && USE_FILE(p))
				fprintf(stderr, " %s\n", basename(p->filename));
		}
		free_sub(sub);
		return -1;
	}
	if (cmd.action != ACTION_CHECK) {
		fprintf(stderr, "\nRestoring:\n");
		m = restore_files(par->files, par->volumes, sub);
		free_sub(sub);
		return m;
	}
	fprintf(stderr, "\nRestorable:\n");
	for (p = par->files; p; p = p->next) {
		if ((!p->match) && USE_FILE(p))
			fprintf(stderr, "  %-40s - can be restored\n",
				basename(do_sub(p->filename, sub)));
	}
	free_sub(sub);
	return 1;
}
