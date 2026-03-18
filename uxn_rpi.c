#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "core1.h"
#include "ps2_keyboard.h"

#include "uxn/uxn11.h"

static void run()
{
    static int counter = 0;
    static char previousControllerButton = 0;
    // int c = getchar_timeout_us(16667);
    // if (c >= 0)
    //     console_input(u, (Uint8)c);
    while (1)
    {
        sleep_us(16667);
        keyboardStatus_t status = ps2GetChar();
        while (status.event)
        {
            if (status.ascii)
            {
                controller_key(status.ascii);
                previousControllerButton = ps2GetController();
                printf("Down: %x, Char: %x\n", previousControllerButton, status.ascii);
                controller_down(previousControllerButton);
            }
            else
            {
                if (!status.release)
                {
                    previousControllerButton = ps2GetController();
                    printf("Down: %x\n", previousControllerButton, status.ascii);
                    controller_down(previousControllerButton);
                }
                else
                {
                    controller_up(previousControllerButton);
                    printf("Up: %x\n", previousControllerButton, status.ascii);
                    previousControllerButton = 0;
                }
            }
            status = ps2GetChar();
        }
        screen_update();
        counter++;
        if (counter >= 60)
        {
            SecondPasssed();
            counter = 0;
        }
    }
}

int main()
{
    // Set system clock to 252 MHz for HSTX timing
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(252000, true);

    stdio_init_all();

    uxn_load_rom("CCCC.rom");

    core1_init();

    initPS2();

    if (!uxn_eval(PAGE_PROGRAM))
    {
        printf("Init Failed\n");
    }

    run();

    // unmount();

    while (true)
    {
        sleep_ms(1000);
    }
}
