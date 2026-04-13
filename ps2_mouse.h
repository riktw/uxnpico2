
#ifndef PS2_MOUSE_H_
#define PS2_MOUSE_H_

#include <stdio.h>
#include <stdlib.h>
#include "ps2_shared.h"

typedef struct
{
    bool middleButton;
    bool rightButton;
    bool leftButton;
    int16_t movementX;
    int16_t movementY;

} mouseEvent_t;

void initPS2Mouse(void);
bool ps2MouseNewEvent();
mouseEvent_t ps2MouseGetEvent();

#endif // PS2_MOUSE_H_
