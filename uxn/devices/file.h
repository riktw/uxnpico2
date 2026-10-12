/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#ifndef UXN_DEVICES_FILE_H
#define UXN_DEVICES_FILE_H

#include "../uxntypes.h"
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"

#define DRIVE_A '0'
#define DRIVE_B '1'

typedef struct
{
	FIL f;
	DIR dir;
	char current_filename[4096];
	enum
	{
		IDLE,
		FILE_READ,
		FILE_WRITE,
		DIR_READ
	} state;
} UxnFile;

Uint16 file_init(UxnFile *c, char drive, char *filename);
Uint16 file_read(UxnFile *c, void *dest, Uint16 len);
Uint16 file_write(UxnFile *c, void *src, Uint16 len, Uint8 flags);
Uint16 file_stat(UxnFile *c, void *dest, Uint16 len);
Uint16 file_delete(UxnFile *c);

void filea_deo_stat(void);
void filea_deo_delete(void);
void filea_deo_name(void);
void filea_deo_length(void);
void filea_deo_read(void);
void filea_deo_write(void);

void fileb_deo_stat(void);
void fileb_deo_delete(void);
void fileb_deo_name(void);
void fileb_deo_length(void);
void fileb_deo_read(void);
void fileb_deo_write(void);

bool init_fs(void);
int load_rom(Uint8 *ram, char *filename);
void unmount();

#endif