/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
\*/
#ifndef PAR_H
#define PAR_H

#include "types.h"

#define PAR_FIX_HEAD_SIZE	0x60
#define FILE_ENTRY_FIX_SIZE	0x38

struct par_s {
	i64 magic;
	u32 version;
	u32 client;
	md5 control_hash;
	md5 set_hash;
	i64 vol_number;
	i64 num_files;
	i64 file_list;
	i64 file_list_size;
	i64 data;
	i64 data_size;

	pfile_t *files;
	pfile_t *volumes;
	u16 *filename;
	u16 *comment;
	file_t f;
};

struct pfile_entr_s {
	i64 size;
	i64 status;
	i64 file_size;
	md5 hash;
	md5 hash_16k;
	u16 filename[1];
};

struct pfile_s {
	pfile_t *next;
	i64 status;
	i64 file_size;
	md5 hash_16k;
	md5 hash;
	i64 vol_number;
	u16 *filename;
	file_t f;
	hfile_t *match;
	u16 *fnrs;
};

extern struct cmdline {
	int action;
	int loglevel;
	int volumes;	/*\ Number of volumes to create \*/

	int pervol : 1;	/*\ volumes is actually files per volume \*/
	int plus :1;	/*\ Turn on or off options (with + or -) \*/
	int move :1;	/*\ Move away files that are in the way \*/
	int fix :1;	/*\ Fix files with bad filenames \*/
	int usecase :1;	/*\ Compare filenames without case \*/
	int dupl :1;	/*\ Check for duplicate files \*/
	int add :1;	/*\ Don't add files to PXX volumes \*/
	int pxx :1;	/*\ Create PXX volumes \*/
	int ctrl :1;	/*\ Check/create control hash \*/
	int keep :1;	/*\ Keep broken files \*/
	int close :1;	/*\ Workaround open file limit \*/
	int dash :1;	/*\ End of cmdline switches \*/
} cmd;

#define ACTION_CHECK	01	/*\ Check PAR files \*/
#define ACTION_RESTORE	02	/*\ Restore missing files \*/
#define ACTION_MIX	03	/*\ Try to use a mix of all PAR files \*/
#define ACTION_ADD	11	/*\ Create a PAR archive ... \*/
#define ACTION_ADDING	12	/*\ ... and add files to it. \*/
#define ACTION_TEXT_UI	20	/*\ Interactive text interface \*/

#define PAR_MAGIC (*((i64 *)"PAR\0\0\0\0\0"))
#define IS_PAR(x) (((x).magic) == PAR_MAGIC)

#define CMP_MD5(a,b) (!memcmp((a), (b), sizeof(md5)))

#define USE_FILE(p) ((p)->status & 0x1)

#endif /* PAR_H */
