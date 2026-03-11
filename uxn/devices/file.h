#ifndef UXN_UXNFILE_H
#define UXN_UXNFILE_H

#include "../uxntypes.h"
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"

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

Uint16 file_init(UxnFile *c, char *filename);
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
Uint8 file_dei(Device *d, Uint8 port);
int load_rom(Uint8 *ram, char *filename);
void unmount();

#endif