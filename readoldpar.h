/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Read old version PAR and PXX files
\*/

#ifndef READOLDPAR_H
#define READOLDPAR_H

int is_old_par(void *data);
par_t * read_old_par(file_t f, u16 *file, int silent);

#endif /* READOLDPAR_H */
