/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  Fix endianness, if needed
\*/
#ifndef ENDIAN_H
#define ENDIAN_H

static inline i64
read_i64(void *data)
{
#ifdef NOT_LITTLE_ENDIAN
	size_t i;
	i64 r = 0;
	u8 *ptr = data;

	for (i = sizeof(i64); --i >= 0; ) {
		r += (i64)ptr[i];
		r <<= 8;
	}
	return r;
#else
	return (*(i64 *)data);
#endif
}

static inline u16
read_u16(void *data)
{
#ifdef NOT_LITTLE_ENDIAN
	size_t i;
	u16 r = 0;
	u8 *ptr = data;

	for (i = < sizeof(u16); --i >= 0; ) {
		r += (u16)ptr[i];
		r <<= 8;
	}
	return r;
#else
	return (*(u16 *)data);
#endif
}

static inline void
write_i64(i64 v, void *data)
{
#ifdef NOT_LITTLE_ENDIAN
	size_t i;
	u8 *ptr = data;

	for (i = 0; i < sizeof(i64); i++) {
		ptr[i] = r & 0xFF;
		r >>= 8;
	}
#else
	(*(i64 *)data) = v;
#endif
}

static inline void
write_u16(u16 v, void *data)
{
#ifdef NOT_LITTLE_ENDIAN
	size_t i;
	u8 *ptr = data;

	for (i = 0; i < sizeof(u16); i++) {
		ptr[i] = r & 0xFF;
		r >>= 8;
	}
#else
	(*(u16 *)data) = v;
#endif
}

/*\ Change endianness to host byte order
|*| NB: This is a fix in place.  Don't call this twice!
\*/
static inline void
pxx_endian_read(pxx_t *pxx)
{
#ifdef NOT_LITTLE_ENDIAN
	pxx->version = read_u16(&pxx->version);
	pxx->vol_number = read_u16(&pxx->vol_number);
	pxx->file_list = read_i64(&pxx->file_list);
	pxx->parity_data = read_i64(&pxx->parity_data);
	pxx->parity_data_size = read_i64(&pxx->parity_data_size);
#endif
}

/*\ Change endianness to host byte order
|*| NB: This is a fix in place.  Don't call this twice!
\*/
static inline void
par_endian_read(par_t *par)
{
#ifdef NOT_LITTLE_ENDIAN
	par->version = read_u16(&par->version);
	par->file_list = read_i64(&par->file_list);
	par->volume_list = read_i64(&par->volume_list);
#endif
}

static inline void
pxx_endian_write(pxx_t *pxx, void *data)
{
	pxx_t *p = (pxx_t *)data;
	memcpy(p, pxx, PXX_FIX_HEAD_SIZE);
#ifdef NOT_LITTLE_ENDIAN
	write_u16(pxx->version, &p->version);
	write_u16(pxx->vol_number, &p->vol_number);
	write_i64(pxx->file_list, &p->file_list);
	write_i64(pxx->parity_data, &p->parity_data);
	write_i64(pxx->parity_data_size, &p->parity_data_size);
#endif
}

static inline void
par_endian_write(par_t *par, void *data)
{
	par_t *p = (par_t *)data;
	memcpy(p, par, PAR_FIX_HEAD_SIZE);
#ifdef NOT_LITTLE_ENDIAN
	write_u16(par->version, &p->version);
	write_i64(par->file_list, &p->file_list);
	write_i64(par->volume_list, &p->volume_list);
#endif
}

#endif /* ENDIAN_H */
