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

#define PAR_FIX_HEAD_SIZE	0x36
#define PXX_FIX_HEAD_SIZE	0x40
#define FILE_ENTRY_FIX_SIZE	0x3A

struct par_s {
	u32 magic;
	u16 version;
	md5 set_hash;
	i64 file_list;
	i64 volume_list;
	md5 control_hash;

	u16 *comment;
	pfile_t *files;
	pfile_t *volumes;
	u8 *data;
	u16 *filename;
	file_t f;
} __attribute__ ((packed));

struct pxx_s {
	u32 magic;
	u16 version;
	md5 set_hash;
	u16 vol_number;
	i64 file_list;
	i64 parity_data;
	i64 parity_data_size;
	md5 control_hash;

	u16 *comment;
	pfile_t *files;
	pfile_t *volumes;
	u8 *data;
	u16 *filename;
	file_t f;
} __attribute__ ((packed));

struct pfile_entr_s {
	i64 size;
	i64 status;
	i64 file_size;
	md5 hash_16k;
	md5 hash;
	u16 vol_number;
	u16 filename[1];
} __attribute__ ((packed));

struct pfile_s {
	pfile_t *next;
	i64 status;
	i64 file_size;
	md5 hash_16k;
	md5 hash;
	u16 vol_number;
	u16 *filename;
	u16 *comment;
	int new;
	file_t f;
	hfile_t *match;
	int crt;
};

extern struct cmdline {
	int action;
	int loglevel;
	int volumes;	/*\ Number of volumes to create \*/

	int pervol : 1;	/*\ volumes is actually files per volume \*/
	int plus :1;	/*\ Turn on or off options (with + or -) \*/
	int rvol :1;	/*\ Restore missing recovery volumes as well \*/
	int move :1;	/*\ Move away files that are in the way \*/
	int fix :1;	/*\ Fix files with bad filenames \*/
	int nocase :1;	/*\ Compare filenames without case \*/
	int dupl :1;	/*\ Check for duplicate files \*/
	int add :1;	/*\ Don't add files to PXX volumes \*/
	int pxx :1;	/*\ Create PXX volumes \*/
	int ctrl :1;	/*\ Check/create control hash \*/
	int dash :1;	/*\ End of cmdline switches \*/
} cmd;

#define ACTION_CHECK	01	/*\ Check PAR files \*/
#define ACTION_RESTORE	02	/*\ Restore missing files \*/
#define ACTION_ADD	11	/*\ Create a PAR archive ... \*/
#define ACTION_ADDING	12	/*\ ... and add files to it. \*/

#define PAR_MAGIC (*((u32 *)"PAR"))
#define PXX_MAGIC (*((u32 *)"PXX"))

#define IS_PAR(x) (((x).magic) == PAR_MAGIC)
#define IS_PXX(x) (((x).magic) == PXX_MAGIC)

#define CMP_MD5(a,b) (!memcmp((a), (b), sizeof(md5)))

#endif /* PAR_H */
