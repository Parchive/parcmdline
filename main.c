/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  In short: Files are XORed with each other and the result is stored.
|*|            From this, one missing file can be recreated.
\*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "fileops.h"
#include "rwpar.h"
#include "checkpar.h"
#include "makepar.h"
#include "interface.h"

struct cmdline cmd;

/*\
|*| Print usage
\*/
int
usage(void)
{
	printf(
"Usage:\n"
"   par c(heck)   [options] <par file>         : Check parity archive\n"
"   par r(ecover) [options] <par file>         : Restore missing volumes\n"
"   par a(dd)     [options] <par file> [files] : Add files to parity archive\n"
" Advanced:\n"
"   par m(ix)    [options] : Try to restore from all parity files at once\n"
"\n"
"Options: (Can be turned off with '+')\n"
"    -m   : Move existing files out of the way\n"
"    -r   : Recover missing parity volumes as well\n"
"    -f   : Fix faulty filenames\n"
"    -p<n>: Number of files per parity volume\n"
" or -n<n>: Number of parity volumes to create\n"
"    -d   : Search for duplicate files\n"
"    -k   : Keep broken files\n"
"    +i   : Do not add following files to parity volumes\n"
"    +c   : Do not create parity volumes\n"
"    +C   : Ignore case in filename comparisons\n"
"    +H   : Do not check control hashes\n"
"    -O   : Work around open file limit\n"
"    -v,+v: Increase or decrease verbosity\n"
"    -h,-?: Display this help\n"
"    --   : Always treat following arguments as files\n"
"\n"
	);
	return 0;
}

/*\ Sanity check \*/
static int
check_sizes(void)
{
	int fail = 0;
	if (sizeof(u8) != 1) {
		fprintf(stderr, "u8 isn't 8 bits wide.\n");
		fail++;
	}
	if (sizeof(u16) != 2) {
		fprintf(stderr, "u16 isn't 16 bits wide.\n");
		fail++;
	}
	if (sizeof(u32) != 4) {
		fprintf(stderr, "u32 isn't 32 bits wide.\n");
		fail++;
	}
	if (sizeof(i64) != 8) {
		fprintf(stderr, "u64 isn't 64 bits wide.\n");
		fail++;
	}
	return fail;
}

/*\ In ui_text.h \*/
void ui_text(void);

/*\
|*| Main loop.  Simple stuff.
\*/
int
main(int argc, char *argv[])
{
	par_t *par = 0;
	int fail = 0;
	char *p;

	if (check_sizes())
		return -1;

	/*\ Some defaults \*/
	memset(&cmd, 0, sizeof(cmd));
	cmd.volumes = 10;
	cmd.pervol = 1;
	cmd.pxx = 1;
	cmd.ctrl = 1;
	cmd.add = 1;
	cmd.usecase = 1;

	if (argc == 1) return usage();

	for (; argc > 1; argc--, argv++) {
		if (((argv[1][0] == '-') || (argv[1][0] == '+')) &&
		    argv[1][1] && !cmd.dash) {
			for (p = argv[1]; *p; p++) switch (*p) {
			case '-':
				if (p[1] == '-')
					cmd.dash = 1;
				else
					cmd.plus = 1;
				break;
			case '+':
				cmd.plus = 0;
				break;
			case 'm':
				cmd.move = cmd.plus;
				break;
			case 'i':
				cmd.add = cmd.plus;
				break;
			case 'f':
				cmd.fix = cmd.plus;
				break;
			case 'c':
				cmd.pxx = cmd.plus;
				break;
			case 'd':
				cmd.dupl = cmd.plus;
				break;
			case 'v':
				if (cmd.plus)
					cmd.loglevel++;
				else
					cmd.loglevel--;
				break;
			case 'p':
			case 'n':
				cmd.pervol = (*p == 'p');
				while (isspace(*++p))
					;
				if (!*p) {
					argv++;
					argc--;
					p = argv[1];
				}
				if (!isdigit(*p)) {
					fprintf(stderr, "Value expected!\n");
				} else {
					cmd.volumes = 0;
					do {
						cmd.volumes *= 10;
						cmd.volumes += *p - '0';
					} while (isdigit(*++p));
					p--;
				}
				break;
			case 'H':
				cmd.ctrl = cmd.plus;
				break;
			case 'C':
				cmd.usecase = cmd.plus;
				break;
			case 'O':
				cmd.close = cmd.plus;
				break;
			case 'k':
				cmd.keep = cmd.plus;
				break;
			case '?':
			case 'h':
				return usage();
			default:
				fprintf(stderr,
					"Unknown switch: '%c'\n", *p);
				break;
			}
			continue;
		}
		if (!cmd.action) {
			switch (argv[1][0]) {
			case 'c':
			case 'C':
				cmd.action = ACTION_CHECK;
				break;
			case 'm':
			case 'M':
				cmd.action = ACTION_MIX;
				break;
			case 'r':
			case 'R':
				cmd.action = ACTION_RESTORE;
				break;
			case 'a':
			case 'A':
				cmd.action = ACTION_ADD;
				break;
			case 'i':
			case 'I':
				cmd.action = ACTION_TEXT_UI;
				break;
			default:
				fprintf(stderr, "Unknown command: '%s'\n",
						argv[1]);
				break;
			}
			continue;
		}
		switch (cmd.action) {
		default:
			cmd.action = ACTION_CHECK;
			/*\ FALLTHROUGH \*/
		case ACTION_CHECK:
		case ACTION_RESTORE:
			fprintf(stderr, "Checking %s\n", argv[1]);
			par = read_par_header(unist(argv[1]), 0, 0, 0);
			if (!par) {
				fail = 2;
				continue;
			}
			if (check_par(par) < 0) fail = 1;
			free_par(par);
			par = 0;
			break;
		case ACTION_ADD:
			fprintf(stderr, "Adding to %s\n", argv[1]);
			par = read_par_header(unist(argv[1]), 1, 0, 0);
			if (!par) return 2;
			cmd.action = ACTION_ADDING;
			break;
		case ACTION_ADDING:
			par_add_file(par, find_file_name(unist(argv[1]), 1));
			break;
		case ACTION_MIX:
			fprintf(stderr, "Unknown argument: '%s'\n", argv[1]);
			break;
		case ACTION_TEXT_UI:
			par_load(unist(argv[1]));
			break;
		}
	}
	if (cmd.action == ACTION_TEXT_UI) {
		ui_text();
		return 0;
	}
	if (cmd.action == ACTION_MIX) {
		par = find_all_par_files();
		if (par) {
			fprintf(stderr, "\nChecking:\n");
			if (check_par(par) < 0)
				fail = 1;
			free_par(par);
			par = 0;
		} else {
			fail = 2;
		}
	}
	if (par) {
		if (cmd.pxx && !par_make_pxx(par))
			fail |= 1;
		if (!par->vol_number && !write_par_header(par))
			fail |= 1;
		free_par(par);
	}
	return fail;
}
