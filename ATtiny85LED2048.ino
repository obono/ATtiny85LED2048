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
    getTilt(vx, vy) ? updateGame(vx, vy) : updateGame(0, 0);
    refreshPixels(getGamePixel);
#else
    getTilt(vx, vy);
    refreshPixels(getCalibrationPixel);
#endif
    delay(50);
}
