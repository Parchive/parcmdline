/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Reed-Solomon coding
\*/

#ifndef REEDSOLOMON_H
#define REEDSOLOMON_H

typedef struct xfile_s xfile_t;

struct xfile_s {
	i64 size;
	file_t f;
	u16 filenr;
	u16 volnr;
};

int recreate(xfile_t *in, int N, xfile_t *out, int M);

#endif
