#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../uxn.h"
#include "file.h"

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

typedef struct {
	FIL f;
	DIR dir;
	char current_filename[4096];
	enum { IDLE,
		FILE_READ,
		FILE_WRITE,
		DIR_READ } state;
} UxnFile;

static UxnFile uxn_file[POLYFILEY];

FATFS fs;

static void reset(UxnFile *c)
{

	f_close(&c->f);
	f_closedir(&c->dir);

	c->state = IDLE;
}

static Uint16 get_entry(char *p, Uint16 len, const char *pathname, const char *basename, int fail_nonzero)
{
	FILINFO st;
	if(len < strlen(basename) + 7)
		return 0;
	if(f_stat(pathname, &st))
		return fail_nonzero ? sprintf(p, "!!!! %s\n", basename) : 0;
	else if(st.fattrib == AM_DIR)
		return sprintf(p, "---- %s\n", basename);
	else if(st.fsize < 0x10000)
		return sprintf(p, "%04x %s\n", (unsigned int)st.fsize, basename);
	else
		return sprintf(p, "???? %s\n", basename);
}

static Uint16 file_read_dir(UxnFile *c, char *dest, Uint16 len)
{
	static char pathname[4356];
	FILINFO st;
	char *p = dest;
	f_readdir(&c->dir, &st);

	for(; st.fname != NULL; f_readdir(&c->dir, &st)) {
		Uint16 n;
		if(st.fname[0] == '.' && st.fname[1] == '\0')
			continue;
		if(strlen(c->current_filename) + 1 + strlen(st.fname) < sizeof(pathname))
			sprintf(pathname, "%s/%s", c->current_filename, st.fname);
		else
			pathname[0] = '\0';
		n = get_entry(p, len, pathname, st.fname, 1);
		if(!n) break;
		p += n;
		len -= n;
	}
	return p - dest;
}

static Uint16 file_init(UxnFile *c, char *filename, size_t max_len)
{
	char *p = c->current_filename;
	size_t len = sizeof(c->current_filename);
	reset(c);
	if(len > max_len) len = max_len;
	while(len) {
		if((*p++ = *filename++) == '\0')
			return 0;
		len--;
	}
	c->current_filename[0] = '\0';
	return 0;
}

static Uint16 file_read(UxnFile *c, void *dest, Uint16 len)
{
	UINT bytesRead = 0;
	if(c->state != FILE_READ && c->state != DIR_READ) {
		reset(c);
		if((f_opendir(&c->dir, c->current_filename) == FR_OK))
			c->state = DIR_READ;
		else if(f_open(&c->f, c->current_filename, (FA_READ | FA_WRITE)) == FR_OK)
			c->state = FILE_READ;
	}
	if(c->state == FILE_READ)
		return f_read(&c->f, dest, len, &bytesRead);
	if(c->state == DIR_READ)
		return file_read_dir(c, dest, len);
	return 0;
}

static Uint16 file_write(UxnFile *c, void *src, Uint16 len, Uint8 flags)
{
	Uint16 ret = 0;
	UINT bytesWritten = 0;
	if(c->state != FILE_WRITE) {
		reset(c);
		if(FR_OK == f_open(&c->f, c->current_filename, (flags & 0x01) ? (FA_OPEN_APPEND | FA_WRITE | FA_READ) : (FA_CREATE_ALWAYS | FA_WRITE | FA_READ)))
			c->state = FILE_WRITE;
	}
	if(c->state == FILE_WRITE) {
		if(FR_OK == f_write(&c->f, src, len, &bytesWritten))
			if(f_sync(&c->f) == FR_OK)
				ret = 0;
	}
	return ret;
}

static Uint16 file_stat(UxnFile *c, void *dest, Uint16 len)
{
	char *basename = strrchr(c->current_filename, '/');
	if(basename != NULL)
		basename++;
	else
		basename = c->current_filename;
	return get_entry(dest, len, c->current_filename, basename, 0);
}

static Uint16 file_delete(UxnFile *c)
{
	return f_unlink(c->current_filename);
}

static UxnFile * file_instance(Device *d)
{
	return &uxn_file[d - &d->u->dev[DEV_FILE0]];
}

/* IO */

void file_deo(Device *d, Uint8 port)
{
	UxnFile *c = file_instance(d);
	Uint16 addr, len, res;
	switch(port) {
	case 0x5:
		DEVPEEK16(addr, 0x4);
		DEVPEEK16(len, 0xa);
		if(len > 0x10000 - addr)
			len = 0x10000 - addr;
		res = file_stat(c, &d->u->ram[addr], len);
		DEVPOKE16(0x2, res);
		break;
	case 0x6:
		res = file_delete(c);
		DEVPOKE16(0x2, res);
		break;
	case 0x9:
		DEVPEEK16(addr, 0x8);
		res = file_init(c, (char *)&d->u->ram[addr], 0x10000 - addr);
		DEVPOKE16(0x2, res);
		break;
	case 0xd:
		DEVPEEK16(addr, 0xc);
		DEVPEEK16(len, 0xa);
		if(len > 0x10000 - addr)
			len = 0x10000 - addr;
		res = file_read(c, &d->u->ram[addr], len);
		DEVPOKE16(0x2, res);
		break;
	case 0xf:
		DEVPEEK16(addr, 0xe);
		DEVPEEK16(len, 0xa);
		if(len > 0x10000 - addr)
			len = 0x10000 - addr;
		res = file_write(c, &d->u->ram[addr], len, d->dat[0x7]);
		DEVPOKE16(0x2, res);
		break;
	}
}

Uint8 file_dei(Device *d, Uint8 port)
{
	UxnFile *c = file_instance(d);
	Uint16 res;
	switch(port) {
	case 0xc:
	case 0xd:
		res = file_read(c, &d->dat[port], 1);
		DEVPOKE16(0x2, res);
		break;
	}
	return d->dat[port];
}

/* Boot */

int load_rom(Uxn *u, char *filename)
{
	int ret;
	FRESULT fr = f_mount(&fs, "", 1);
	if (FR_OK != fr) {
		ret = 1;
		return ret;
    }
	file_init(uxn_file, filename, strlen(filename) + 1);
	ret = file_read(uxn_file, &u->ram[PAGE_PROGRAM], 0x10000 - PAGE_PROGRAM);
	reset(uxn_file);
	return ret;
}

void unmount()
{
	f_unmount("");
}