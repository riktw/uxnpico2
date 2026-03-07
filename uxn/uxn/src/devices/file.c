#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../uxn.h"
#include "file.h"



/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/
FATFS fs;
FIL fp;

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

Uint16 file_init(UxnFile *c, char *filename)
{
	char *p = c->current_filename;
	size_t len = sizeof(c->current_filename);
	reset(c);
	while(len) {
		if((*p++ = *filename++) == '\0')
			return 0;
		len--;
	}
	c->current_filename[0] = '\0';
	return 0;
}

Uint16 file_read(UxnFile *c, void *dest, Uint16 len)
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

Uint16 file_write(UxnFile *c, void *src, Uint16 len, Uint8 flags)
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

Uint16 file_stat(UxnFile *c, void *dest, Uint16 len)
{
	char *basename = strrchr(c->current_filename, '/');
	if(basename != NULL)
		basename++;
	else
		basename = c->current_filename;
	return get_entry(dest, len, c->current_filename, basename, 0);
}

Uint16 file_delete(UxnFile *c)
{
	return f_unlink(c->current_filename);
}


/* Boot */

int load_rom(Uint8* ram, char *filename)
{
	int ret = 0;
	UINT bytesRead = 0;

	FRESULT fr = f_mount(&fs, "", 1);
	if (FR_OK != fr) {
		ret = 1;
		return ret;
    }
	fr = f_open(&fp, filename, FA_READ);
	if (FR_OK != fr) {
		ret = 1;
		return ret;
    }

	fr = f_read(&fp, &ram[PAGE_PROGRAM], 0x10000 - PAGE_PROGRAM, &bytesRead);
	if (FR_OK != fr) {
		ret = fr;
		return ret;
    }

	return ret;
}

void unmount()
{
	f_unmount("");
}