
/* hw_config.c
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/

/*
This file should be tailored to match the hardware design.

See
https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/tree/main#customizing-for-the-hardware-configuration
*/

#include "hw_config.h"

/* Configuration of hardware SPI object */
static spi_t spi = {
    .hw_inst = spi0, // SPI component
    .sck_gpio = 22,  // GPIO number (not Pico pin number)
    .mosi_gpio = 23,
    .miso_gpio = 20,
    .baud_rate = 25000000,
    //.baud_rate = 125 * 1000 * 1000 / 8  // 15625000 Hz
    //.baud_rate = 125 * 1000 * 1000 / 6  // 20833333 Hz
    //.baud_rate = 125 * 1000 * 1000 / 4 // 31250000 Hz
    //.baud_rate = 125 * 1000 * 1000 / 2  // 62500000 Hz
};

/* SPI Interface */
static sd_spi_if_t spi_if = {
    .spi = &spi,  // Pointer to the SPI driving this card
    .ss_gpio = 21 // The SPI slave select GPIO for this SD card
};

static sd_sdio_if_t sdio_if = {
    .CMD_gpio = 35,
    .D0_gpio = 36,
    .SDIO_PIO = pio1,
    .baud_rate = 25000000,
    .DMA_IRQ_num = DMA_IRQ_1,
};

static sd_card_t sdio_card = {
    .type = SD_IF_SDIO,
    .sdio_if_p = &sdio_if,
    .use_card_detect = false,
};

/* Configuration of the SD Card socket object */
static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if // Pointer to the SPI interface driving this card
};

/* ********************************************************************** */

size_t sd_get_num() { return 2; }

/**
 * @brief Get a pointer to an SD card object by its number.
 *
 * @param[in] num The number of the SD card to get.
 *
 * @return A pointer to the SD card object, or @c NULL if the number is invalid.
 */
sd_card_t *sd_get_by_num(size_t num)
{
    if (num == 0)
    {
        return &sd_card;
    }
    if (num == 1)
    {
        return &sdio_card;
    }
    else
    {
        // The number is invalid. Return @c NULL.
        return NULL;
    }
}

/* [] END OF FILE */
