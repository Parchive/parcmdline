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

#include "types.h"
#include "par.h"

par_t * create_par_header(u16 *file, i64 vol);
par_t * read_par_header(u16 *file, int create, i64 vol, int silent);
void free_file_list(pfile_t *list);
void free_par(par_t *par);
file_t write_par_header(par_t *par);
int restore_files(pfile_t *files, pfile_t *volumes, sub_t *sub);

void dump_par(par_t *par);

#endif /* READWRITEPAR_H */
