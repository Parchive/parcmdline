/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Backend operations
\*/
#ifndef BACKEND_H
#define BACKEND_H

#include "types.h"
#include "par.h"

char * basename(u16 *path);
int unicode_cmp(u16 *a, u16 *b);
int unicode_gt(u16 *a, u16 *b);
hfile_t * hfile_add(u16 *filename);
void hash_directory(char *dir);
int hash_file(hfile_t *file, char type);
int find_file(pfile_t *file, int displ);
hfile_t * find_file_name(u16 *path, int displ);
hfile_t * find_volume(u16 *name, i64 vol);
int move_away(u16 *file, const u8 *ext);
int rename_away(u16 *src, u16 *dst);
u16 * file_numbers(pfile_t **list, pfile_t **files);
int find_volumes(par_t *par, int tofind);
void find_par_files(pfile_t **volumes, pfile_t **files, int part);
par_t * find_all_par_files(void);
int par_control_check(par_t *par);

#endif /* BACKEND_H */
