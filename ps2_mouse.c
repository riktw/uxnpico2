
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include "ps2_mouse.h"

static uint ps2_sm;
static PIO ps2_pio;
static uint ps2_pio_irq;
static void ps2_ihandler();
static uint8_t movementArray[3] = {0};
static uint8_t movementCounter = 0;
static mouseEvent_t mouseEvent;
static bool event = false;
static uint initCounter = 0;
static bool initDone = false;

#define MOUSE_MAX_EVENTS 16
static mouseEvent_t eventArray[MOUSE_MAX_EVENTS];
static uint8_t eventHead = 0;
static uint8_t eventTail = 0;

// info: https://www-ug.eecg.utoronto.ca/desl/nios_devices_SoC/datasheets/PS2%20Mouse%20Protocol.htm

uint32_t ps2_frame(uint8_t byte)
{
    bool parity = 1;
    for (uint8_t i = 0; i < 8; i++)
    {
        parity = parity ^ (byte >> i & 1);
    }
    return ((1 << 10) | (parity << 9) | (byte << 1)) ^ 0x7ff;
}

// ISR
void __not_in_flash_func(ps2_ihandler)(void)
{
    if (!pio_sm_is_rx_fifo_empty(ps2_pio, ps2_sm))
    {
        if (initDone)
        {
            movementArray[movementCounter] = (pio_sm_get(ps2_pio, ps2_sm) >> 23) & 0xFF;
            movementCounter++;
            if (movementCounter >= 3)
            {
                movementCounter = 0;
                mouseEvent.movementX = movementArray[1];
                mouseEvent.movementY = movementArray[2];

                mouseEvent.leftButton = CheckBit(movementArray[0], 0);
                mouseEvent.rightButton = CheckBit(movementArray[0], 1);
                mouseEvent.middleButton = CheckBit(movementArray[0], 2);

                mouseEvent.movementX = CheckBit(movementArray[0], 4) ? 0xFF00 | mouseEvent.movementX : mouseEvent.movementX;
                mouseEvent.movementY = CheckBit(movementArray[0], 5) ? 0xFF00 | mouseEvent.movementY : mouseEvent.movementY;
                if (CheckBit(movementArray[0], 6))
                    printf("mouse X overflow!\n");
                if (CheckBit(movementArray[0], 7))
                    printf("mouse Y overflow!\n");

                eventArray[eventHead] = mouseEvent;
                eventHead = (eventHead < MOUSE_MAX_EVENTS) ? eventHead + 1 : eventHead;
                printf("x: %i, y: %i\n", mouseEvent.movementX, mouseEvent.movementY);
            }
        }
        else
        {
            pio_sm_get(ps2_pio, ps2_sm);
            initCounter++;
        }
    }
    pio_interrupt_clear(ps2_pio, 0);
}

void initPS2Mouse(void)
{
    ps2_pio = pio1;
    ps2_pio_irq = PIO1_IRQ_0;
    ps2_sm = pio_claim_unused_sm(ps2_pio, true);
    ps2in_init(ps2_pio, ps2_sm, 6, PS2FREQ);
    pio_set_irq0_source_enabled(ps2_pio, pis_interrupt0, true);
    irq_set_exclusive_handler(ps2_pio_irq, ps2_ihandler);

    pio_sm_set_enabled(ps2_pio, ps2_sm, true);
    // get rid of noise on pins when the PS/2 keyboard is enabled
    sleep_ms(1);
    pio_sm_clear_fifos(ps2_pio, ps2_sm);
    irq_set_enabled(ps2_pio_irq, true);

    pio_sm_put(ps2_pio, ps2_sm, ps2_frame(0xFF));
    while (initCounter < 3)
    {
        sleep_ms(10);
    }
    sleep_ms(10);

    pio_sm_put(ps2_pio, ps2_sm, ps2_frame(0xF6));
    while (initCounter < 4)
    {
        sleep_ms(10);
    }
    sleep_ms(10);

    pio_sm_put(ps2_pio, ps2_sm, ps2_frame(0xF4));
    while (initCounter < 5)
    {
        sleep_ms(10);
    }
    sleep_ms(10);

    initDone = true;
}

bool ps2MouseNewEvent()
{
    return (eventTail < eventHead);
}

mouseEvent_t ps2MouseGetEvent()
{
    mouseEvent_t event = eventArray[eventTail];
    eventTail++;
    if (eventHead == eventTail)
    {
        eventTail = 0;
        eventHead = 0;
    }
    return event;
}