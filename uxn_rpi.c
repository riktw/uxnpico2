#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"

#include "core1.h"

#include "uxn/uxn11.h"

static void run()
{
    static int counter = 0;
    // int c = getchar_timeout_us(16667);
    // if(c >= 0)
    //     console_input(u, (Uint8)c);
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

int main()
{
    stdio_init_all();

    core1_init();

    uxn_load_rom("catclock.rom");

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
