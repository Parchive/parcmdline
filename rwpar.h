/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Read and write PAR files
\*/
#ifndef READWRITEPAR_H
#define READWRITEPAR_H

#include <limits.h>
#include "types.h"
#include "par.h"

int unicode_cmp(u16 *a, u16 *b);
int hash_file(hfile_t *file, char type);
int find_file(pfile_t *file, int displ);
hfile_t * find_file_path(char *path);
hfile_t * find_volume(u16 *name, i64 vol);
int find_volumes(par_t *par, int tofind);
int move_away(u16 *file, const u8 *ext);
void rename_away(u16 *src, u16 *dst);
par_t * read_par_header(char *file, int create, i64 vol, int silent);
void free_file_list(pfile_t *list);
void free_par(par_t *par);
file_t write_par_header(par_t *par);
int restore_files(pfile_t *files, pfile_t *volumes);

void dump_par(par_t *par);

#endif /* READWRITEPAR_H */
