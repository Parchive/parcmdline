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
#ifndef CHECKPAR_H
#define CHECKPAR_H

#include "types.h"
#include "par.h"

int restore_file(pxx_t *pxx);
int check_par(par_t *par);

#endif /* CHECKPAR_H */
