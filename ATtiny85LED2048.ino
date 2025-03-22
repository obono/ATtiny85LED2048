#include "common.h"

/*  Local Variables  */

/*------------------------------------------------------------------------*/

void setup(void)
{
    initDevices();
    initGame();
    refreshPixels(getGamePixel);
}

void loop(void)
{
    int8_t vx, vy;
#if 1
    if (getTilt(vx, vy)) {
        updateGame(vx, vy);
        refreshPixels(getGamePixel);
    }
#else
    getTilt(vx, vy);
    refreshPixels(getCalibrationPixel);
#endif
    delay(100);
}
