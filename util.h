/*\
|*| Some utility macro's
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
\*/
#ifndef UTIL_H
#define UTIL_H

#define HEXDIGIT(i) (((i) + (((i) < 0xA) ? '0' : ('a' - 0xA))))

#define NEW(ptr, size) ((ptr) = (malloc(sizeof(*(ptr)) * (size))))
#define CNEW(ptr, size) ((ptr) = (calloc(sizeof(*(ptr)), (size))))
#define RENEW(ptr, size) ((ptr) = (realloc((ptr), sizeof(*(ptr)) * (size))))
#define COPY(tgt, src, nel) (memcpy((tgt), (src), ((nel) * sizeof(*(src)))))

#endif /* UTIL_H */
