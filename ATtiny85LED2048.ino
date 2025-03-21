#include <Arduino.h>
#include <avr/power.h>
#include <Adafruit_NeoPixel.h>

#define SimpleWire_SCL_PORT B
#define SimpleWire_SCL_POS  2
#define SimpleWire_SDA_PORT B
#define SimpleWire_SDA_POS  0

#include "SimpleWire.h"
#define SIMPLEWIRE          SimpleWire<SimpleWire_1M>

/*  Defines  */

#define PIXELS_PIN      3
#define NUMBER_PIXELS   16
#define HUE_MAX         1536

#define ADXL345_ADDR 0x53
#define DATA_FORMAT 0x31
#define POWER_CTL 0x2D
#define DATAX0 0x32
#define FULL_RES_16G 0x0B
#define BIT10_16G 0x03
#define MEASURE 0x08

/*  Grobal Functions  */

void initGame(void);
void updateGame(int8_t vx, int8_t vy);
void getGamePixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);

/*  Local Variables  */

static Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMBER_PIXELS, PIXELS_PIN, NEO_GRB + NEO_KHZ800);

/*------------------------------------------------------------------------*/

static void initAccels(void)
{
    SIMPLEWIRE::begin();
    uint8_t data = FULL_RES_16G;
    SIMPLEWIRE::writeWithCommand(ADXL345_ADDR, DATA_FORMAT, &data, 1);
    data = MEASURE;
    SIMPLEWIRE::writeWithCommand(ADXL345_ADDR, POWER_CTL, &data, 1);
}

static bool getTilt(int8_t &vx, int8_t &vy)
{
    static bool isTilted = true;
    static int16_t lastVx = 0, lastVy = 0, lastVz = 0;
    bool ret = false;
    uint8_t dac[6];
    if (SIMPLEWIRE::readWithCommand(ADXL345_ADDR, DATAX0, dac, sizeof(dac)) > 0) {
        lastVx = (dac[1] << 8) | dac[0];
        lastVy = (dac[3] << 8) | dac[2];
        lastVz = (dac[5] << 8) | dac[4];
        if (isTilted) {
            if (abs(lastVx) < 32 && abs(lastVy) < 32) isTilted = false;
        } else {
            if (abs(lastVx) > 64 || abs(lastVy) > 64) isTilted = ret = true;
        }
    }
    if (isTilted) {
        if (abs(lastVx) > abs(lastVy)) {
            vx = (lastVx > 0) ? 1 : -1;
            vy = 0;
        } else {
            vx = 0;
            vy = (lastVy > 0) ? 1 : -1;
        }
    } else {
        vx = vy = 0;
    }
    return ret;
}

static void initPixels(void)
{
    pixels.begin();
    pixels.fill();
    pixels.setBrightness(128); // TODO
}

static void drawGame(void)
{
    for (uint8_t i = 0; i < NUMBER_PIXELS; i++) {
        int8_t y = i >> 2;
        int8_t x = (y & 1) ? (i & 3) : 3 - (i & 3);
        uint8_t r, g, b;
        getGamePixel(x, y, r, g, b);
        pixels.setPixelColor(i, pixels.Color(r, g, b));
    }
    pixels.show();
}

/*------------------------------------------------------------------------*/

void setup(void)
{
    initAccels();
    initPixels();
    initGame();
    drawGame();
}

void loop(void)
{
    int8_t vx, vy;
    if (getTilt(vx, vy)) {
        updateGame(-vx, vy);
        drawGame();
    }
    delay(100);
}
