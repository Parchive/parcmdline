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
	int i;
	i64 r = 0;
	u8 *ptr = data;

	for (i = sizeof(i64); --i >= 0; ) {
		r <<= 8;
		r += (i64)ptr[i];
	}
	return r;
}

static inline void
write_i64(i64 v, void *data)
{
	size_t i;
	u8 *ptr = data;

	for (i = 0; i < sizeof(i64); i++) {
		ptr[i] = v & 0xFF;
		v >>= 8;
	}
}

/*\ Change endianness to host byte order
|*| NB: This is a fix in place.  Don't call this twice!
\*/
static inline void
par_endian_read(par_t *par)
{
#ifdef NOT_LITTLE_ENDIAN
	par->version = read_u32(&par->version);
	par->client = read_u32(&par->client);
	par->vol_number = read_i64(&par->vol_number);
	par->num_files = read_i64(&par->num_files);
	par->file_list = read_i64(&par->file_list);
	par->file_list_size = read_i64(&par->file_list_size);
	par->data = read_i64(&par->data);
	par->data_size = read_i64(&par->data_size);
#endif
}

static inline void
par_endian_write(par_t *par, void *data)
{
	par_t *p = (par_t *)data;
	memcpy(p, par, PAR_FIX_HEAD_SIZE);
#ifdef NOT_LITTLE_ENDIAN
	write_u32(par->version, &p->version);
	write_u32(par->client, &p->client);
	write_i64(par->vol_number, &p->vol_number);
	write_i64(par->num_files, &p->num_files);
	write_i64(par->file_list, &p->file_list);
	write_i64(par->file_list_size, &p->file_list_size);
	write_i64(par->data, &p->data);
	write_i64(par->data_size, &p->data_size);
#endif
}

#endif /* ENDIAN_H */
