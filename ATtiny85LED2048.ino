#include "common.h"

void setup(void)
{
    initDevices();
    initGame();
}

void loop(void)
{
    int8_t vx, vy;
    getDPad(vx, vy);
    updateGame(vx, vy);
    refreshPixels();
    manageConfigByButton();
    delay(MILLIS_PER_FRAME);
}
