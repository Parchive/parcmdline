/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Text User Interface
\*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "interface.h"

static u16 **parlist = 0;
static u16 **filelist = 0;

enum {
	CMD_LOAD,
	CMD_SEARCH,
	CMD_UNLOAD,
	CMD_PARLIST,
	CMD_FILELIST,
	CMD_CHECK,
	CMD_FIND,
	CMD_FIXNAME,
	CMD_GETSTATUS,
	CMD_SETSTATUS,
	CMD_RECOVER,
	CMD_ADDFILE,
	CMD_REMOVEFILE,
	CMD_ADDPARS,
	CMD_CREATE,
	CMD_HELP,
	CMD_QUIT,
	CMD_UNKNOWN,
	CMD_AMBIGUOUS
};

static struct cmds {
	char *str;
	int cmd;
} cmds[] = {
	{ "LOAD",	CMD_LOAD },
	{ "SEARCH",	CMD_SEARCH },
	{ "UNLOAD",	CMD_UNLOAD },
	{ "PARLIST",	CMD_PARLIST },
	{ "FILELIST",	CMD_FILELIST },
	{ "CHECK",	CMD_CHECK },
	{ "FIND",	CMD_FIND },
	{ "FIXNAME",	CMD_FIXNAME },
	{ "GETSTATUS",	CMD_GETSTATUS },
	{ "SETSTATUS",	CMD_SETSTATUS },
	{ "RECOVER",	CMD_RECOVER },
	{ "ADDFILE",	CMD_ADDFILE },
	{ "REMOVEFILE",	CMD_REMOVEFILE },
	{ "ADDPARS",	CMD_ADDPARS },
	{ "CREATE",	CMD_CREATE },
	{ "HELP",	CMD_HELP },
	{ "QUIT",	CMD_QUIT }
};

#define NO_CMDS (sizeof(cmds) / sizeof(*cmds))

static void
print_help(void)
{
	printf(
"LOAD <filename>    : Add a (new) PAR file to the current list.\n"
"SEARCH             : Search for PAR files matching the current filelist.\n"
"UNLOAD <entry>     : Remove a PAR file from the list.\n"
"PARLIST            : Show the current list of PAR files.\n"
"FILELIST           : Show the current list of data files.\n"
"CHECK <entry>      : Check the MD5sum of a file.\n"
"FIND <entry>       : Find a file by its filename.\n"
"FIXNAME [<entry>]  : Fix faulty filenames [of <entry>].\n"
"GETSTATUS <entry>  : Get the status bits of an entry.\n"
"SETSTATUS <entry> <status> : Set the status bits of an entry.\n"
"RECOVER [<entry>]  : Recover missing files [only <entry>]\n"
"ADDFILE <filename> : Add a data file to the current filelist.\n"
"REMOVEFILE <entry> : Remove a data file from the current filelist.\n"
"ADDPARS <number>   : Add new PAR files until there are <number>.\n"
"CREATE [<entry>]   : Create PAR files [only <entry>].\n"
"HELP               : Show this help.\n"
"QUIT               : Quit.\n"
		);
}

static int cc = '\n';

#define EOL(x) (((x) == '\n') || ((x) == EOF))

static void
sort_cmds(void)
{
	int i, j, k;

	k = NO_CMDS - 1;
	do {
		for (i = j = 0; i < k; i++) {
			if (strcmp(cmds[i].str, cmds[i + 1].str) > 0) {
				struct cmds tmp;
				tmp = cmds[i + 1];
				cmds[i + 1] = cmds[i];
				cmds[i] = tmp;
				j = i;
			}
		}
		k = j;
	} while (k > 0);
}

static int
get_cmd(void)
{
	struct cmds *l, *r;
	int c;
	int idx;

	do {
		while (!EOL(cc))
			cc = getchar();

		if (cc == EOF)
			return CMD_QUIT;

		putchar('>'); fflush(stdout);

		l = cmds;
		r = cmds + NO_CMDS - 1;
		for (idx = 0;; idx++) {
			cc = getchar();
			if (!isalnum(cc))
				break;
			c = toupper(cc);
			while (c > l->str[idx])
				if (++l > r)
					return CMD_UNKNOWN;
			while (c < r->str[idx])
				if (--r < l)
					return CMD_UNKNOWN;
		}
	} while (idx == 0);
	if (r > l)
		return CMD_AMBIGUOUS;
	return l->cmd;
}

static int bufidx = 0;

static u16 *
buf_add(u16 c)
{
	static u16 *buf = 0;
	static int bufsz = 0;

	if (bufidx >= bufsz) {
		bufsz += 256;
		buf = realloc(buf, bufsz * sizeof(*buf));
	}
	buf[bufidx++] = c;
	return buf;
}

static u16 *
get_str(void)
{
	bufidx = 0;
	while (isspace(cc)) {
		if (EOL(cc))
			return buf_add(0);
		cc = getchar();
	}
	do {
		buf_add(cc);
		cc = getchar();
	} while (!EOL(cc));
	return buf_add(0);
}

static i64
get_number(void)
{
	i64 ret;
	int base = 10;

	while (isspace(cc)) {
		if (EOL(cc))
			return 0;
		cc = getchar();
	}
	ret = 0;
	if (cc == '0') {
		base = 8;
		cc = getchar();
	}
	if ((cc == 'x') || (cc == 'X') || (cc == '$')) {
		base = 16;
		cc = getchar();
	}
	for (;;) {
		int c;

		c = toupper(cc);
		if (c >= 'a') c -= 'a';
		else c -= '0';
		if ((c < 0) || (c >= base)) return ret;
		ret = ret * base + c;
		cc = getchar();
	}
}

static u16 *
get_entry(u16 **list)
{
	i64 num, i;

	i = get_number();
	if (!list) return 0;
	for (num = 0; list[num]; num++)
		;
	if ((i < 0) || (i > num))
		i = 0;
	if (i == 0) return 0;
	return list[i - 1];
}

static void
print_errcode(int code)
{
	static char *err_list[] = {
		"OK",
		"ERROR: File error",
		"ERROR: Does not exist",
		"ERROR: Not found",
		"ERROR: Corrupt",
		"ERROR: Failed",
		"ERROR: Already loaded",
		"ERROR: Not Implemented",
		"ERROR: Name Clash",
		"ERROR: Invalid Argument"
	};
	printf("%s\n", err_list[code]);
}

static void
print_string(u16 *str)
{
	while (*str)
		putchar(*str++);
	putchar('\n');
}

static void
print_number(i64 num)
{
	printf("0x%llx\n", num);
}

static void
print_list(u16 **list)
{
	int i;

	for (i = 1; *list; list++, i++) {
		printf("%3d: ", i);
		print_string(*list);
	}
}

/*\ Parse commands until QUIT is read \*/
void
ui_text(void)
{
	u16 *e;

	sort_cmds();
	cc = '\n';

	for (;;) switch (get_cmd()) {
	case CMD_LOAD:
		print_errcode(par_load(get_str()));
		break;
	case CMD_SEARCH:
		print_errcode(par_search(get_number()));
		break;
	case CMD_UNLOAD:
		print_errcode(par_unload(get_entry(parlist)));
		break;
	case CMD_PARLIST:
		if (parlist) free(parlist);
		parlist = par_parlist();
		print_list(parlist);
		break;
	case CMD_FILELIST:
		if (filelist) free(filelist);
		filelist = par_filelist();
		print_list(filelist);
		break;
	case CMD_CHECK:
		e = get_entry(filelist);
		if (e) {
			print_errcode(par_check(e));
		} else if (filelist) {
			int i;
			for (i = 0; filelist[i]; i++) {
				printf("CHECK ");
				print_string(filelist[i]);
				print_errcode(par_check(filelist[i]));
			}
		} else {
			printf("ERROR: No filelist.\n");
		}
		break;
	case CMD_FIND:
		print_string(par_find(get_entry(filelist)));
		break;
	case CMD_FIXNAME:
		print_errcode(par_fixname(get_entry(filelist)));
		break;
	case CMD_GETSTATUS:
		print_number(par_getstatus(get_entry(filelist)));
		break;
	case CMD_SETSTATUS:
		e = get_entry(filelist);
		print_errcode(par_setstatus(e, get_number()));
		break;
	case CMD_RECOVER:
		print_errcode(par_recover(get_entry(filelist)));
		break;
	case CMD_ADDFILE:
		print_errcode(par_addfile(get_str()));
		break;
	case CMD_REMOVEFILE:
		print_errcode(par_removefile(get_entry(filelist)));
		break;
	case CMD_ADDPARS:
		e = get_entry(parlist);
		print_errcode(par_addpars(e, get_number()));
		break;
	case CMD_CREATE:
		print_errcode(par_create(get_entry(parlist)));
		break;
	case CMD_QUIT:
		return;
	case CMD_HELP:
		print_help();
		break;
	case CMD_AMBIGUOUS:
		printf("Ambiguous command.\n");
		break;
	default:
		printf("Unknown command.\n");
		break;
	}
}
