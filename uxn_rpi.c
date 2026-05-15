#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/uart.h"

#include "core1.h"
#include "devices/screen.h"

#include "uxn/uxn11.h"

#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define UART_TX_PIN 0
#define UART_RX_PIN 1

// RX interrupt handler
void on_uart_rx()
{
    while (uart_is_readable(UART_ID))
    {
        uint8_t ch = uart_getc(UART_ID);
        if (ch > 0)
        {
            console_input((Uint8)ch, 1);
        }
    }
}

static void run()
{
    static int counter = 0;

    while (1)
    {
        sleep_us(16667);
        screen_update();
        counter++;
        if (counter >= 60)
        {
            SecondPasssed();
            counter = 0;
        }
    }
}

/*
    TODO:
    - fix console properly, irq for receiving data, send data possibly non blocking too.
    - timing for screen. can likely be done via hdmi timing callback!
    - go over any missing implementations in uxn11.
    - Check with latest uxn11 to see what's updated.
    - optimize some

    When pc board arrives:
    - Second storage
    - Sound
    - USB for keeb/mouse
    - Game port
*/
int main()
{
    // Set system clock to 252 MHz for HSTX timing
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(252000, true);

    uart_init(UART_ID, 2400);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    uart_set_fifo_enabled(UART_ID, false); // Fifo's disabled as uart is used per character.

    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    uxn_init();

    uxn_load_rom("boot.rom");

    core1_init();

    if (!uxn_eval(PAGE_PROGRAM))
    {
        printf("Init Failed\n");
    }

    run();

    while (true)
    {
        sleep_ms(1000);
    }
}
