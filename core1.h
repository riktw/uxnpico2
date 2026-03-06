#ifndef _CORE1_H_
#define _CORE1_H_

#include <stdint.h>

void core1_setPalette(uint8_t color[4]);
void core1_setBackground(uint8_t color);
void core1_setPixel(uint16_t x, uint16_t y, uint8_t color, uint8_t layer);
void core1_init(void);
void core1_updateScreen();
void core1_test(int settest);

#endif