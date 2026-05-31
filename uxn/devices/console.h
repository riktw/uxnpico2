#ifndef UXN_DEVICES_CONSOLE_H
#define UXN_DEVICES_CONSOLE_H

#include <stdint.h>
#include "hardware/uart.h"

void InitConsole(uart_inst_t *uartId, uint32_t txPin, uint32_t rxPin, uint32_t baudrate);

void console_deo_vector(void);
void console_deo_stdout(void);
void console_deo_stderr(void);
void console_deo_hb(void);
void console_deo_lb(void);

#endif