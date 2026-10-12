 ## Uxn/Varvara on a pi pico 2

A mostly working port of Uxn/Varvara for a pi pico 2. 

Default resolution is 640x480 on HDMI. A 4:3 monitor is recommended! 

It can be compiled for just a RP2350 BellsNWhistles, with PCBOARD in CMakeLists.txt set to zero. This only enabled PS/2 mouse/keyboard. 
With PCBOARD set to 1, PS/2 is not used, instead USB is used for mouse/keyboard, and audio, rtc and NES controller are also used.

More info on the blog: https://justanotherelectronicsblog.com/?p=1487  and the other blog: https://justanotherelectronicsblog.com/?p=1523
