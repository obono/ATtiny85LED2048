#pragma once

#include <Arduino.h>

/*  Defines  */

#define BOARD_SIZE    4

/*  Typedefs  */

typedef void (*PixelFunc)(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);

/*  Global Functions  */

void initDevices(void);
bool getTilt(int8_t &vx, int8_t &vy);
void getCalibrationPixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);
void refreshPixels(PixelFunc func);

void initGame(void);
void updateGame(int8_t vx, int8_t vy);
void getGamePixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);
