#include "console.h"
#include "stdio.h"
#include "pico/stdlib.h"
#include "../uxn11.h"

extern Uint8 dev[0x100];
static int console_vector = 0;
static uart_inst_t *uartInstance;

#define CONSOLE_STD 0x1
#define CONSOLE_ARG 0x2
#define CONSOLE_EOA 0x3
#define CONSOLE_END 0x4

#define CONSOLE_DATA_BITS 8
#define CONSOLE_STOP_BITS 1
#define CONSOLE_PARITY UART_PARITY_NONE

// RX interrupt handler
void on_uart_rx()
{
    while (uart_is_readable(uartInstance))
    {
        uint8_t ch = uart_getc(uartInstance);
        if (ch > 0)
        {
            if (ch == EOF)
                ch = 0;
            dev[0x12] = ch, dev[0x17] = CONSOLE_STD;
            if (console_vector)
                uxn_eval(console_vector);
        }
    }
}

void InitConsole(uart_inst_t *uartId, uint32_t txPin, uint32_t rxPin, uint32_t baudrate) // todo: pass what uart/pins
{
    uartInstance = uartId;
    uart_init(uartInstance, 2400);
    gpio_set_function(txPin, UART_FUNCSEL_NUM(uartInstance, txPin));
    gpio_set_function(rxPin, UART_FUNCSEL_NUM(uartInstance, rxPin));
    uart_set_baudrate(uartInstance, baudrate);
    uart_set_hw_flow(uartInstance, false, false);
    uart_set_format(uartInstance, CONSOLE_DATA_BITS, CONSOLE_STOP_BITS, CONSOLE_PARITY);

    uart_set_fifo_enabled(uartInstance, false); // Fifo's disabled as uart is used per character.

    int UART_IRQ = uartInstance == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(uartInstance, true, false);
}

void console_deo_vector(void)
{
    console_vector = peek2(&dev[0x10]);
}

void console_deo_stdout(void)
{
    uart_putc(uartInstance, dev[0x18]);
}

void console_deo_stderr(void)
{
    uart_putc(uartInstance, dev[0x19]);
}
