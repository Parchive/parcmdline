/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  This file holds an interface abstraction,
|*|   to be called by different user interfaces.
\*/

#ifndef INTERFACE_H
#define INTERFACE_H

#include "types.h"

/*\ Errorcodes \*/
#define PAR_OK				0
#define PAR_ERR_ERRNO			1
#define PAR_ERR_EXIST			2
#define PAR_ERR_NOT_FOUND		3
#define PAR_ERR_CORRUPT			4
#define PAR_ERR_FAILED			5
#define PAR_ERR_ALREADY_LOADED		6
#define PAR_ERR_IMPL			7
#define PAR_ERR_CLASH			8
#define PAR_ERR_INVALID			9

#define PARFLAG_MOVE	0x1
#define PARFLAG_CASE	0x2
#define PARFLAG_CTRL	0x4
#define PARFLAG_KEEP	0x8

/*\ Get the current flags
|*|  Returns: Current flags
\*/
int par_flags(void);

/*\ Set some flags
|*|  Returns: Current flags
\*/
int par_setflags(int flags);

/*\ Set some flags
|*|  Returns: Current flags
\*/
int par_unsetflags(int flags);

/*\ Add a PARfile to the current parlist.
|*|   filename: Name of file to load
\*/
int par_load(u16 *filename);

/*\ Search for additional PARfiles matching the current ones
|*|   partial: Load files that only match partially.
\*/
int par_search(int partial);

/*\ Remove a PARfile from the current parlist.
|*|   entry: Name of file to remove
\*/
int par_unload(u16 *entry);

/*\ List the current PARfiles
|*|  Returns: Array of filenames, NULL-terminated.
|*|   Notes: Caller should free() returned array, but not the filenames.
|*|          These pointers should be used as 'entry' arguments.
\*/
u16 ** par_parlist(void);

/*\ List the current filelist
|*|  Returns: Array of filenames, NULL-terminated.
|*|   Notes: Caller should free() returned array, but not the filenames.
|*|          These pointers should be used as 'entry' arguments.
\*/
u16 ** par_filelist(void);

/*\ Check the MD5 sum of a file
|*|   entry: file to check
\*/
int par_check(u16 *entry);

/*\ Find a file by its MD5 sum
|*|   entry: the file to find
|*|  Returns: matching filename
|*|   Notes: Returned filename should NOT be free()d.
\*/
u16 * par_find(u16 *entry);

/*\ Fix incorrect filenames
|*|   entry(optional): only fix this entry
\*/
int par_fixname(u16 *entry);

/*\ Get status word
|*|   entry: file to get status of
|*|  Returns: status word for entry.
\*/
i64 par_getstatus(u16 *entry);

/*\ Set status word
|*|   entry: file to set status for
\*/
int par_setstatus(u16 *entry, i64 status);

/*\ Recover missing files
|*|   entry(optional): only recover this entry
\*/
int par_recover(u16 *entry);

/*\ Add a file to the current filelist
|*|   filename: file to add
\*/
int par_addfile(u16 *filename);

/*\ Remove a file from the current filelist
|*|   entry: file to remove
\*/
int par_removefile(u16 *entry);

/*\ Add new PARfiles to the current parlist
|*|   number: the highest volume number to create
\*/
int par_addpars(u16 *entry, int number);

/*\ Create PARfiles from the current filelist
|*|   entry(optional): only create this entry
\*/
int par_create(u16 *entry);

/*\ List the current directories
|*|  Returns: Array of filenames, NULL-terminated.
|*|   Notes: Calles should free() returned array.
\*/
u16 ** par_dirlist(void);

/*\
|*| Set the smart renaming pattern
|*|   Note: Optional arguments are to specify pattern,
|*|         otherwise the pattern is taken from the current list.
\*/
int par_setsmart(u16 *from, u16 *to);

#endif /*\ INTERFACE_H \*/
