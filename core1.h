#ifndef _CORE1_H_
#define _CORE1_H_

#include <stdint.h>

#define UXN_PADDING 16
#define UXN_WIDTH (640)
#define UXN_HEIGHT (480)

typedef void (*screen_update_callback_t)(void);

void core1_setPalette(uint16_t color[4]);
void core1_setBackground(uint8_t color);
void core1_setResolution(uint16_t height, uint16_t width);
void core1_setPixel(uint16_t x, uint16_t y, uint8_t mask, uint8_t color);
void core1_fillPixels(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint8_t mask, uint8_t color);
void core1_init(screen_update_callback_t callback);

#endif