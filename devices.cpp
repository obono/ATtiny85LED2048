#include "common.h"
#define SimpleWire_SCL_PORT B
#define SimpleWire_SCL_POS  2
#define SimpleWire_SDA_PORT B
#define SimpleWire_SDA_POS  0
#include "SimpleWire.h"
#include <Adafruit_NeoPixel.h>

/*  Defines  */

#define ADXL345_I2C_ADDR            0x53
#define ADXL345_REG_BW_RATE         0x2C
#define ADXL345_REG_POWER_CTL       0x2D
#define ADXL345_REG_DATA_FORMAT     0x31
#define ADXL345_REG_DATAX0          0x32
#define ADXL345_VAL_LOW_POWER_25HZ  0x18
#define ADXL345_VAL_FULL_RES_2G     0x08
#define ADXL345_VAL_MEASURE         0x08

#define TILT_X_SIGN         -1
#define TILT_Y_SIGN         1
#define TILT_Z_SIGN         1
#define TILT_ON             80
#define TILT_OFF            30

#define PIXELS_PIN          3
#define PIXELS_NUMBER       (BOARD_SIZE * BOARD_SIZE)
#define PIXELS_BRIGHTNESS   128 // TODO

/*  Typedefs  */

typedef SimpleWire<SimpleWire_1M> SimpleWire1M;

/*  Local Variables  */

static Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXELS_NUMBER, PIXELS_PIN, NEO_GRB + NEO_KHZ800);
static int8_t lastVx, lastVy, currentVx, currentVy;

/*---------------------------------------------------------------------------*/

void initDevices(void)
{
    SimpleWire1M::begin();
    uint8_t data[4];
    data[0] = ADXL345_VAL_LOW_POWER_25HZ;
    data[1] = ADXL345_VAL_MEASURE;
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_BW_RATE, data, 2);
    data[0] = ADXL345_VAL_FULL_RES_2G;
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATA_FORMAT, data, 1);
    currentVx = currentVy = 0;
    SimpleWire1M::readWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATAX0, data, 4);
    randomSeed((unsigned long)*data);

    pixels.begin();
    pixels.clear();
    pixels.setBrightness(PIXELS_BRIGHTNESS);
}

void getDPad(int8_t &vx, int8_t &vy)
{
    lastVx = currentVx;
    lastVy = currentVy;
    uint8_t dac[6];
    if (SimpleWire1M::readWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATAX0, dac, sizeof(dac)) > 0) {
        int16_t tiltX = ((dac[1] << 8) | dac[0]) * TILT_X_SIGN;
        int16_t tiltY = ((dac[3] << 8) | dac[2]) * TILT_Y_SIGN;
        int16_t tiltZ = ((dac[5] << 8) | dac[4]) * TILT_Z_SIGN;
        if (lastVx < 0 && tiltX >= -TILT_OFF || lastVx > 0 && tiltX <= TILT_OFF) currentVx = 0;
        if (tiltX <= -TILT_ON) currentVx = -1;
        if (tiltX >= TILT_ON) currentVx = 1;
        if (lastVy < 0 && tiltY >= -TILT_OFF || lastVy > 0 && tiltY <= TILT_OFF) currentVy = 0;
        if (tiltY <= -TILT_ON) currentVy = -1;
        if (tiltY >= TILT_ON) currentVy = 1;
    }
    vx = (currentVx != lastVx) ? currentVx : 0;
    vy = (currentVy != lastVy) ? currentVy : 0;
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
