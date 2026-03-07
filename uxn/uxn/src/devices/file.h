#ifndef UXN_UXNFILE_H
#define UXN_UXNFILE_H

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#include "../uxntypes.h"
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"

typedef struct {
	FIL f;
	DIR dir;
	char current_filename[4096];
	enum { IDLE,
		FILE_READ,
		FILE_WRITE,
		DIR_READ } state;
} UxnFile;

Uint16 file_init(UxnFile *c, char *filename);
Uint16 file_read(UxnFile *c, void *dest, Uint16 len);
Uint16 file_write(UxnFile *c, void *src, Uint16 len, Uint8 flags);
Uint16 file_stat(UxnFile *c, void *dest, Uint16 len);
Uint16 file_delete(UxnFile *c);

Uint8 file_dei(Device *d, Uint8 port);
int load_rom(Uint8* ram, char *filename);
void unmount();

#endif