#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../uxn11.h"
#include "file.h"

static FATFS fs[2];
static FIL fp[2];

extern Uint8 ram[BANKS_CAP];
extern Uint8 dev[0x100];
static UxnFile ufs[2];
static int rL1, rL2;

static void reset(UxnFile *c)
{

	f_close(&c->f);
	f_closedir(&c->dir);

	c->state = IDLE;
}

static Uint16 get_entry(char *p, Uint16 len, const char *pathname, const char *basename, int fail_nonzero)
{
	FILINFO st;
	if (len < strlen(basename) + 7)
		return 0;
	if (f_stat(pathname, &st))
		return fail_nonzero ? sprintf(p, "!!!! %s\n", basename) : 0;
	else if (st.fattrib == AM_DIR)
		return sprintf(p, "---- %s\n", basename);
	else if (st.fsize < 0x10000)
		return sprintf(p, "%04x %s\n", (unsigned int)st.fsize, basename);
	else
		return sprintf(p, "???? %s\n", basename);
}

static Uint16 get_stat(char *p, Uint16 len, const char *pathname)
{
	FILINFO st;
	if (f_stat(pathname, &st))
		snprintf(p, len + 1, "!!!!");
	else if (st.fattrib == AM_DIR)
		snprintf(p, len + 1, "----");
	else if (st.fsize < 0x10000)
		snprintf(p, len + 1, "%04x", (unsigned int)st.fsize);
	else
		snprintf(p, len + 1, "????");
	return len;
}

static Uint16 file_read_dir(UxnFile *c, char *dest, Uint16 len)
{
	static char pathname[4356];
	FILINFO st;
	char *p = dest;
	f_readdir(&c->dir, &st);

	for (; st.fname[0] != '\0'; f_readdir(&c->dir, &st))
	{
		Uint16 n;
		if (st.fname[0] == '.' && st.fname[1] == '\0')
			continue;
		if (strlen(c->current_filename) + 1 + strlen(st.fname) < sizeof(pathname))
			sprintf(pathname, "%s/%s", c->current_filename, st.fname);
		else
			pathname[0] = '\0';
		n = get_entry(p, len, pathname, st.fname, 1);
		if (!n)
			break;
		p += n;
		len -= n;
	}
	return p - dest;
}

Uint16 file_init(UxnFile *c, char drive, char *filename)
{
	char *p = c->current_filename;
	size_t len = sizeof(c->current_filename);
	reset(c);
	*p++ = drive;
	len--;
	*p++ = ':';
	len--;
	while (len)
	{
		if ((*p++ = *filename++) == '\0')
			return 0;
		len--;
	}
	c->current_filename[0] = '\0';
	return 0;
}

Uint16 file_read(UxnFile *c, void *dest, Uint16 len)
{
	UINT bytesRead = 0;
	if (c->state != FILE_READ && c->state != DIR_READ)
	{
		reset(c);
		if ((f_opendir(&c->dir, c->current_filename) == FR_OK))
			c->state = DIR_READ;
		else if (f_open(&c->f, c->current_filename, (FA_READ | FA_WRITE)) == FR_OK)
			c->state = FILE_READ;
	}
	if (c->state == FILE_READ)
	{
		f_read(&c->f, dest, len, &bytesRead);
		return bytesRead;
	}
	if (c->state == DIR_READ)
		return file_read_dir(c, dest, len);
	return 0;
}

Uint16 file_write(UxnFile *c, void *src, Uint16 len, Uint8 flags)
{
	Uint16 ret = 0;
	UINT bytesWritten = 0;
	if (c->state != FILE_WRITE)
	{
		reset(c);
		if (FR_OK == f_open(&c->f, c->current_filename, (flags & 0x01) ? (FA_OPEN_APPEND | FA_WRITE | FA_READ) : (FA_CREATE_ALWAYS | FA_WRITE | FA_READ)))
			c->state = FILE_WRITE;
	}
	if (c->state == FILE_WRITE)
	{
		if (FR_OK == f_write(&c->f, src, len, &bytesWritten))
			if (f_sync(&c->f) == FR_OK)
				ret = bytesWritten;
	}
	return ret;
}

Uint16 file_stat(UxnFile *c, void *dest, Uint16 len)
{
	char *basename = strrchr(c->current_filename, '/');
	if (basename != NULL)
		basename++;
	else
		basename = c->current_filename;
	return get_stat(dest, len, c->current_filename);
}

Uint16 file_delete(UxnFile *c)
{
	if (f_unlink(c->current_filename) == FR_OK)
	{
		return 1;
	}
	return 0;
}

/* Boot */

bool init_fs(void)
{
	sd_init_driver();
	const char *drive = sd_get_drive_prefix(sd_get_by_num(0));
	FRESULT fr = f_mount(&fs[0], drive, 1);

#ifdef ENABLE_PCBOARD
	if (FR_OK == fr)
	{
		const char *drive_b = sd_get_drive_prefix(sd_get_by_num(1));
		fr = f_mount(&fs[1], drive_b, 1);
		return 1;
	}
#endif
	return 0;
}

int load_rom(Uint8 *ram, char *filename)
{
	int ret = 0;
	UINT bytesRead = 0;

	FRESULT fr = f_open(&fp[0], filename, FA_READ);
	if (FR_OK != fr)
	{
		ret = 1;
		return ret;
	}

	fr = f_read(&fp[0], &ram[PAGE_PROGRAM], 0x10000 - PAGE_PROGRAM, &bytesRead);
	if (FR_OK != fr)
	{
		ret = fr;
		return ret;
	}

	return ret;
}

void unmount()
{
	const char *drive = sd_get_drive_prefix(sd_get_by_num(0));
	f_unmount(drive);
}

void filea_deo_stat(void)
{
	int location = peek2(&dev[0xa4]);
	int length = (0x10000 - location < rL1) ? 0x10000 - location : rL1;
	poke2(&dev[0xa2], file_stat(&ufs[0], &ram[location], length));
}

void filea_deo_delete(void)
{
	poke2(&dev[0xa2], file_delete(&ufs[0]));
}

void filea_deo_name(void)
{
	poke2(&dev[0xa2], file_init(&ufs[0], DRIVE_A, (char *)&ram[peek2(&dev[0xa8])]));
}

void filea_deo_length(void)
{
	rL1 = peek2(&dev[0xaa]);
}

void filea_deo_read(void)
{
	int location = peek2(&dev[0xac]);
	int length = (0x10000 - location < rL1) ? 0x10000 - location : rL1;
	poke2(&dev[0xa2], file_read(&ufs[0], &ram[location], length));
}

void filea_deo_write(void)
{
	int location = peek2(&dev[0xae]);
	int length = (0x10000 - location < rL1) ? 0x10000 - location : rL1;
	poke2(&dev[0xa2], file_write(&ufs[0], &ram[location], length, dev[0xa7]));
}

void fileb_deo_stat(void)
{
	int location = peek2(&dev[0xa4]);
	int length = (0x10000 - location < rL1) ? 0x10000 - location : rL1;
	poke2(&dev[0xa2], file_stat(&ufs[1], &ram[location], length));
}

void fileb_deo_delete(void)
{
	poke2(&dev[0xa2], file_delete(&ufs[1]));
}

void fileb_deo_name(void)
{
	poke2(&dev[0xa2], file_init(&ufs[1], DRIVE_B, (char *)&ram[peek2(&dev[0xa8])]));
}

void fileb_deo_length(void)
{
	rL1 = peek2(&dev[0xaa]);
}

void fileb_deo_read(void)
{
	int location = peek2(&dev[0xac]);
	int length = (0x10000 - location < rL1) ? 0x10000 - location : rL1;
	poke2(&dev[0xa2], file_read(&ufs[1], &ram[location], length));
}

void fileb_deo_write(void)
{
	int location = peek2(&dev[0xae]);
	int length = (0x10000 - location < rL1) ? 0x10000 - location : rL1;
	poke2(&dev[0xa2], file_write(&ufs[0], &ram[location], length, dev[0xa7]));
}