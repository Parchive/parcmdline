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
#include "rwpar.h"
#include "checkpar.h"
#include "makepar.h"

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
"\n"
"Options: (Can be turned off with '+')\n"
"    -m   : Move existing files out of the way\n"
"    -r   : Recover missing parity volumes as well\n"
"    -f   : Fix faulty filenames\n"
"    -p<n>: Number of files per parity volume\n"
" or -n<n>: Number of parity volumes to create\n"
"    -d   : Search for duplicate files\n"
"    +i   : Do not add following files to parity volumes\n"
"    +c   : Do not create parity volumes\n"
"    +H   : Do not check control hashes\n"
"    -v,+v: Increase or decrease verbosity\n"
"    -h,-?: Display this help\n"
"    --   : Always treat following arguments as files\n"
"\n"
	);
	return 0;
}

/*\
|*| Main loop.  Simple stuff.
\*/
int
main(int argc, char *argv[])
{
	par_t *par = 0;
	int fail = 0;
	char *p;

	/*\ Some defaults \*/
	memset(&cmd, 0, sizeof(cmd));
	cmd.volumes = 10;
	cmd.pervol = 1;
	cmd.pxx = 1;
	cmd.ctrl = 1;

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
			case 'r':
				cmd.rvol = cmd.plus;
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
				cmd.action = ACTION_CHECK;
				break;
			case 'r':
				cmd.action = ACTION_RESTORE;
				break;
			case 'a':
				cmd.action = ACTION_ADD;
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
			par = read_par_header(argv[1], 0);
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
			par = read_par_header(argv[1], 1);
			if (!par) return 2;
			cmd.action = ACTION_ADDING;
			break;
		case ACTION_ADDING:
			par_add_file(par, find_file_path(argv[1]));
			break;
		}
	}
	if (par) {
		if (cmd.pxx && !par_make_pxx(par))
			fail |= 1;
		if (!fail && !write_par_header(par))
			fail |= 1;
		free_par(par);
	}
	return fail;
}
