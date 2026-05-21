#include "core1.h"
#include "string.h"

#include "pico/stdlib.h"
#include <pico/multicore.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include <hardware/irq.h>

#include "pico_hdmi/hstx_data_island_queue.h"
#include "pico_hdmi/hstx_packet.h"
#include "pico_hdmi/video_output.h"

static int uxnHeight = UXN_HEIGHT;
static int uxnWidth = UXN_WIDTH;

static char __attribute__((aligned(4), section(".ram_fb"))) uxnbuf[(UXN_HEIGHT * UXN_WIDTH) / 2] = {0};

static screen_update_callback_t vsync_callback = NULL;

// Precalculate a 256 entry LUT for doing 2 pixels in one go!
static uint32_t palette[256];
void core1_setPalette(uint16_t color[4])
{
    palette[0] = color[0];
    palette[1] = color[1];
    palette[2] = color[2];
    palette[3] = color[3];

    for (size_t i = 4; i < 16; ++i)
    {
        palette[i] = palette[i / 4];
    }

    for (size_t i = 16; i <= 240; i += 16)
    {
        palette[i] = palette[i / 16] << 16 | palette[0];
        palette[i + 1] = palette[i / 16] << 16 | palette[1];
        palette[i + 2] = palette[i / 16] << 16 | palette[2];
        palette[i + 3] = palette[i / 16] << 16 | palette[3];
        palette[i + 4] = palette[i / 16] << 16 | palette[4];
        palette[i + 5] = palette[i / 16] << 16 | palette[5];
        palette[i + 6] = palette[i / 16] << 16 | palette[6];
        palette[i + 7] = palette[i / 16] << 16 | palette[7];
        palette[i + 8] = palette[i / 16] << 16 | palette[8];
        palette[i + 9] = palette[i / 16] << 16 | palette[9];
        palette[i + 10] = palette[i / 16] << 16 | palette[10];
        palette[i + 11] = palette[i / 16] << 16 | palette[11];
        palette[i + 12] = palette[i / 16] << 16 | palette[12];
        palette[i + 13] = palette[i / 16] << 16 | palette[13];
        palette[i + 14] = palette[i / 16] << 16 | palette[14];
        palette[i + 15] = palette[i / 16] << 16 | palette[15];
    }
    for (size_t i = 0; i < 16; ++i)
    {
        palette[i] = palette[0] << 16 | palette[i];
    }
}

void core1_setBackground(uint8_t color)
{
    memset(uxnbuf, (color << 4) | color, sizeof(uxnbuf));
}

void core1_setResolution(uint16_t height, uint16_t width)
{
    memset(uxnbuf, 0x00, sizeof(uxnbuf));
}

void __not_in_flash_func(core1_setPixel)(uint16_t x, uint16_t y, uint8_t mask, uint8_t color)
{
    uint32_t uxnlocation = (y * (UXN_WIDTH)) + x;
    uint32_t uxnActualLocation = uxnlocation >> 1;

    // One byte contains two pixels due to ram limits.
    // one pixel has 2 bits for foreground, 2 for background. ffbb. Hence one byte contains ffbbffbb
    // shitty hack to store 2 pixels per byte!
    if (uxnlocation & 0x0001)
    {
        uxnbuf[uxnActualLocation] &= ((mask << 4) + 0x0F);
        uxnbuf[uxnActualLocation] += (color << 4);
    }
    else
    {
        uxnbuf[uxnActualLocation] &= (0xF0 + mask);
        uxnbuf[uxnActualLocation] += color;
    }
}

void __not_in_flash_func(core1_fillPixels)(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint8_t mask, uint8_t color)
{
    uint32_t uxnlocation = (y1 * (UXN_WIDTH)) + x1;
    uint32_t uxnActualLocation = uxnlocation >> 1;

    uint8_t dualMask = (mask << 4) + mask;
    uint8_t dualPixel = (color << 4) + color;

    if (((x2 - x1) % 2 == 0)) // 2 pixels per byte, if we happen to draw even amounts, nice speedup
    {
        for (int y = y1; y < y2; y++)
        {
            for (int x = x1; x < x2; x += 2)
            {
                uxnlocation = (y * (UXN_WIDTH)) + x;
                uxnActualLocation = uxnlocation >> 1;
                uxnbuf[uxnActualLocation] &= dualMask;
                uxnbuf[uxnActualLocation] += dualPixel;
            }
        }
    }
    else
    {
        for (int y = y1; y < y2; y++)
        {
            for (int x = x1; x < x2; x++)
            {

                uxnlocation = (y * (UXN_WIDTH)) + x;
                uxnActualLocation = uxnlocation >> 1;
                if (uxnlocation & 0x0001)
                {
                    uxnbuf[uxnActualLocation] &= ((mask << 4) + 0x0F);
                    uxnbuf[uxnActualLocation] += (color << 4);
                }
                else
                {
                    uxnbuf[uxnActualLocation] &= (0xF0 + mask);
                    uxnbuf[uxnActualLocation] += color;
                }
            }
        }
    }
}

void __scratch_x("") scanline_callback(uint32_t v_scanline, uint32_t active_line, uint32_t *dst)
{
    (void)v_scanline;

    uint32_t uxnlocationStart = ((active_line) * (UXN_WIDTH)) / 2;
    for (int i = 0; i < UXN_WIDTH / 2; ++i)
    {
        uint8_t pixels = uxnbuf[uxnlocationStart + i];
        dst[i] = palette[(pixels)];
    }
}

void core1_init(screen_update_callback_t callback)
{
    memset(uxnbuf, 0x00, sizeof(uxnbuf));
    // Initialize HDMI output
    hstx_di_queue_init();
    video_output_set_dvi_mode(true);
    video_output_init(640, 480);
    video_output_set_scanline_callback(scanline_callback);
    video_output_set_vsync_callback(callback);

    multicore_launch_core1(video_output_core1_run);
}
