/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|   Type definitions
\*/
#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed long long i64;
typedef u8 md5[16];

typedef struct par_s par_t;
typedef struct pxx_s pxx_t;
typedef struct pfile_entr_s pfile_entr_t;
typedef struct pfile_s pfile_t;
typedef struct hfile_s hfile_t;

typedef struct file_s *file_t;

#endif /* TYPES_H */
