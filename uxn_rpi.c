#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

#include "core1.h"

#include "uxn/src/uxn.h"
#include "devices/system.h"
#include "devices/file.h"
#include "devices/datetime.h"

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
static int blanking = 0;
static uint8_t RAM[0x10000] = {0};

static Uint8 blending[5][16] = {
    {0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
    {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
    {1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
    {2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2},
    {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}};

static Uxn u;
static Device *devscreen;
static Device *devctrl;
static Device *devmouse;


// 3:3:2 RGB
static void screen_palette(Device *d) 
{
    uint8_t colors[4];
    for(size_t i = 0; i < 4; ++i) 
    {
        Uint8 r = ((d->dat[0x8 + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
        Uint8 g = ((d->dat[0xa + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
        Uint8 b = ((d->dat[0xc + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;

        colors[i] = ((r & 0xE0) + ((g & 0xE0) >> 3) + ((b & 0xC0) >> 6));
    }
    core1_setPalette(colors);
    core1_setBackground(0);
}


void system_deo_special(Device *d, Uint8 port)
{
    if(port > 0x7 && port < 0xe) 
    {
        screen_palette(d);
    }
}

static void console_deo(Device *d, Uint8 port)
{
	putchar(d->dat[port]);
}

static Uint8 nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}


static void nil_deo(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
}

static int console_input(Uxn *u, char c)
{
	Device *d = &u->dev[1];
	d->dat[0x2] = c;
	return uxn_eval(u, GETVECTOR(d));
}

Uint8 screen_dei(Device *d, Uint8 port) {
    switch(port) {
        case 0x2: return 512 >> 8;
        case 0x3: return (512 & 0xFF);
        case 0x4: return 360 >> 8;
        case 0x5: return (360 & 0xFF);
        default: return d->dat[port];
    }
}


void ppu_1bpp(Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy, Uint8 layer) 
{
    Uint16 v, h;
    for(v = 0; v < 8; v++)
    {
        for(h = 0; h < 8; h++) 
        {
            Uint8 ch1 = (sprite[v] >> (7 - h)) & 0x1;
            if(ch1 || blending[4][color])
                core1_setPixel(x + (flipx ? 7 - h : h), y + (flipy ? 7 - v : v), blending[ch1][color], layer);
        }
    }
}

void ppu_2bpp(Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy, Uint8 layer) 
{
    Uint16 v, h;
    for(v = 0; v < 8; v++)
    {
        for(h = 0; h < 8; h++) {
            Uint8 ch1 = ((sprite[v] >> (7 - h)) & 0x1);
            Uint8 ch2 = ((sprite[v + 8] >> (7 - h)) & 0x1);
            Uint8 ch = ch1 + ch2 * 2;
            if(ch || blending[4][color])
                core1_setPixel(x + (flipx ? 7 - h : h), y + (flipy ? 7 - v : v), blending[ch][color], layer);
        }
    }
}

void screen_deo(Device *d, Uint8 port) {
    switch(port) {
        // case 0x3:
        //     if(!FIXED_SIZE) {
        //         Uint16 w;
        //         DEVPEEK16(w, 0x2);
        //         screen_resize(&uxn_screen, clamp(w, 1, 1024), uxn_screen.height);
        //     }
        //     break;
        // case 0x5:
        //     if(!FIXED_SIZE) {
        //         Uint16 h;
        //         DEVPEEK16(h, 0x4);
        //         screen_resize(&uxn_screen, uxn_screen.width, clamp(h, 1, 1024));
        //     }
        //     break;
        case 0xe: {
            Uint16 x, y;
            Uint8 layer = d->dat[0xe] & 0x40;
            Uint8 color = d->dat[0xe] & 0x03;
            DEVPEEK16(x, 0x8);
            DEVPEEK16(y, 0xa);
            core1_setPixel(x, y, color, layer);
            if(d->dat[0x6] & 0x01) DEVPOKE16(0x8, x + 1); /* auto x+1 */
            if(d->dat[0x6] & 0x02) DEVPOKE16(0xa, y + 1); /* auto y+1 */
        } break;
        case 0xf: {
            Uint16 x, y, dx, dy, addr;
            Uint8 twobpp = !!(d->dat[0xf] & 0x80);
            DEVPEEK16(x, 0x8);
            DEVPEEK16(y, 0xa);
            DEVPEEK16(addr, 0xc);
            Uint8 n = d->dat[0x6] >> 4;
            dx = (d->dat[0x6] & 0x01) << 3;
            dy = (d->dat[0x6] & 0x02) << 2;
            if(addr > 0x10000 - ((n + 1) << (3 + twobpp))) {
                return;
            }
            Uint8 color = d->dat[0xf] & 0xf;
            Uint8 flipx = d->dat[0xf] & 0x10;
            Uint8 flipy = d->dat[0xf] & 0x20;
            Uint8 layer = d->dat[0xe] & 0x40;
            for(size_t i = 0; i <= n; i++) {
                Uint8 *sprite = &d->u->ram[addr];
                if (twobpp) {
                    ppu_2bpp(x + dy * i, y + dx * i, sprite, color, flipx, flipy, layer);
                } else {
                    ppu_1bpp(x + dy * i, y + dx * i, sprite, color, flipx, flipy, layer);
                }
                addr += (d->dat[0x6] & 0x04) << (1 + twobpp);
            }
            DEVPOKE16(0xc, addr);   // auto addr+length 
            DEVPOKE16(0x8, x + dx); // auto x+8 
            DEVPOKE16(0xa, y + dy); // auto y+8 
            
        } break;
	}
}


static void run(Uxn *u)
{
    static int counter = 0;
	Device *d = &u->dev[0];
    Device *screen = &u->dev[2];
	while(!d->dat[0xf]) {
        int c = getchar_timeout_us(16667);
        if(c >= 0)
            console_input(u, (Uint8)c);
            
        uxn_eval(u, GETVECTOR(screen));
        counter++;
        if(counter >= 60) {
            SecondPasssed();
            counter = 0;
        }
	}
}

int uxn_interrupt(void)
{
	return 1;
}

static int start(Uxn *u)
{
	if(!uxn_boot(u, RAM))
		return 0;
	/* system   */ uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ uxn_port(u, 0x2, screen_dei, screen_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file0    */ uxn_port(u, 0xa, file_dei, file_deo);
	/* file1    */ uxn_port(u, 0xb, file_dei, file_deo);
	/* datetime */ uxn_port(u, 0xc, datetime_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);
	return 1;
}


int main()
{
    stdio_init_all();

    core1_init();
    Uxn u;
    if(!start(&u))
    {
		printf("Start Failed\n");
    }

    load_rom(&u, "catclock.rom");

	if(!uxn_eval(&u, PAGE_PROGRAM))
    {
		printf("Init Failed\n");
    }

	run(&u);

    //unmount();
    while (true) {
        sleep_ms(1000);
    }
}
