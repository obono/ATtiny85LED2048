#include "common.h"
#define SimpleWire_SCL_PORT B
#define SimpleWire_SCL_POS  2
#define SimpleWire_SDA_PORT B
#define SimpleWire_SDA_POS  0
#include "SimpleWire.h"
#include <Adafruit_NeoPixel.h>

/*  Defines  */

#define SIMPLEWIRE      SimpleWire<SimpleWire_1M>
#define ADXL345_ADDR    0x53
#define DATA_FORMAT     0x31
#define POWER_CTL       0x2D
#define DATAX0          0x32
#define FULL_RES_16G    0x0B
#define BIT10_16G       0x03
#define MEASURE         0x08

#define TILT_X_SIGN     -1
#define TILT_Y_SIGN     1
#define TILT_Z_SIGN     1
#define TILT_ON         64
#define TILT_OFF        32


#define PIXELS_PIN          3
#define PIXELS_NUMBER       (BOARD_SIZE * BOARD_SIZE)
#define PIXELS_BRIGHTNESS   128 // TODO

/*  Local Variables  */

static Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXELS_NUMBER, PIXELS_PIN, NEO_GRB + NEO_KHZ800);
static int16_t lastVx, lastVy, lastVz;

void initDevices(void)
{
    uint8_t data;
    SIMPLEWIRE::begin();
    data = FULL_RES_16G;
    SIMPLEWIRE::writeWithCommand(ADXL345_ADDR, DATA_FORMAT, &data, 1);
    data = MEASURE;
    SIMPLEWIRE::writeWithCommand(ADXL345_ADDR, POWER_CTL, &data, 1);
    lastVx = lastVy = lastVz = 0;
    unsigned long seed;
    SIMPLEWIRE::readWithCommand(ADXL345_ADDR, DATAX0, (uint8_t *)&seed, sizeof(seed));
    randomSeed(seed);

    pixels.begin();
    pixels.fill();
    pixels.setBrightness(PIXELS_BRIGHTNESS);
}

bool getTilt(int8_t &vx, int8_t &vy)
{
    static bool isTilted = true;
    bool ret = false;
    uint8_t dac[6];
    if (SIMPLEWIRE::readWithCommand(ADXL345_ADDR, DATAX0, dac, sizeof(dac)) > 0) {
        lastVx = ((dac[1] << 8) | dac[0]) * TILT_X_SIGN;
        lastVy = ((dac[3] << 8) | dac[2]) * TILT_Y_SIGN;
        lastVz = ((dac[5] << 8) | dac[4]) * TILT_Z_SIGN;
        if (isTilted) {
            if (abs(lastVx) < TILT_OFF && abs(lastVy) < TILT_OFF) isTilted = false;
        } else {
            if (abs(lastVx) > TILT_ON || abs(lastVy) > TILT_ON) isTilted = ret = true;
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

void getCalibrationPixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b)
{
    r = g = b = 0;
    if (y > 0 && y < BOARD_SIZE - 1) {
        if (lastVx < -TILT_ON && x == 0) r = g = b = 8;
        if (lastVx > TILT_ON && x == BOARD_SIZE - 1) r = g = b = 8;
    }
    if (x > 0 && x < BOARD_SIZE - 1) {
        if (lastVy < -TILT_ON && y == 0) r = g = b = 8;
        if (lastVy > TILT_ON && y == BOARD_SIZE - 1) r = g = b = 8;
    }
}

void refreshPixels(PixelFunc func)
{
    if (func) {
        for (uint8_t i = 0; i < PIXELS_NUMBER; i++) {
            int8_t x = i % BOARD_SIZE, y = i / BOARD_SIZE;
            if ((y & 1) == 0) x = BOARD_SIZE - 1 - x;
            uint8_t r, g, b;
            func(x, y, r, g, b);
            pixels.setPixelColor(i, r, g, b);
        }
    } else {
        pixels.fill();
    }
    pixels.show();
}
