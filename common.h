#pragma once

#include <Arduino.h>

/*  Defines  */

#define BOARD_SIZE          4
#define MILLIS_PER_FRAME    50

/*  Global Functions  */

void initDevices(void);
void manageConfigByButton(void);
void getDPad(int8_t &vx, int8_t &vy);
void refreshPixels(void);

void initGame(void);
void updateGame(int8_t vx, int8_t vy);
void getGamePixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);
