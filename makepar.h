/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Create PAR and PXX files.
\*/
#ifndef MAKEPAR_H
#define MAKEPAR_H

#include "par.h"

int par_add_file(par_t *par, hfile_t *file);
int par_make_pxx(par_t *par);

#endif /* MAKEPAR_H */
