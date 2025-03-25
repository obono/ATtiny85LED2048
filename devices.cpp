#include "common.h"
#define SimpleWire_SCL_PORT B
#define SimpleWire_SCL_POS  2
#define SimpleWire_SDA_PORT B
#define SimpleWire_SDA_POS  0
#include "SimpleWire.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

/*  Defines  */

#define ADXL345_I2C_ADDR            0x53
#define ADXL345_REG_OFSX            0x1E
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
#define TILT_1G             256
#define TILT_TOLERANCE      8
#define TILT_OFFSET_SAMPLES 32

#define PIXELS_PIN          3
#define PIXELS_NUMBER       (BOARD_SIZE * BOARD_SIZE)
#define PIXELS_BRIGHTNESS   128 // TODO

/*  Typedefs  */

typedef SimpleWire<SimpleWire_1M> SimpleWire1M;

/*  Local Functions  */

static void readEEPROM(uint8_t address, uint8_t *pData, uint8_t len);
static void writeEEPROM(uint8_t address, uint8_t *pData, uint8_t len);
static void manageCalibration(int16_t tiltX, int16_t tiltY, int16_t tiltZ);
static void getDPadPixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);
static void getDPadPixelSub(int8_t current, int8_t last, uint8_t &r, uint8_t &g, uint8_t &b);

/*  Local Variables  */

static Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXELS_NUMBER, PIXELS_PIN, NEO_GRB + NEO_KHZ800);
static int8_t lastVx, lastVy, currentVx, currentVy;
static bool isCalibrated;

/*---------------------------------------------------------------------------*/

void initDevices(void)
{
    SimpleWire1M::begin();
    uint8_t data[4];
    readEEPROM(0, data, 3);
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_OFSX, data, 3);
    data[0] = ADXL345_VAL_LOW_POWER_25HZ;
    data[1] = ADXL345_VAL_MEASURE;
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_BW_RATE, data, 2);
    data[0] = ADXL345_VAL_FULL_RES_2G;
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATA_FORMAT, data, 1);
    SimpleWire1M::readWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATAX0, data, 4);
    randomSeed((unsigned long)*data);
    currentVx = currentVy = 0;
    isCalibrated = false;

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
        if (!isCalibrated) manageCalibration(tiltX, tiltY, tiltZ);
    }
    vx = (currentVx != lastVx) ? currentVx : 0;
    vy = (currentVy != lastVy) ? currentVy : 0;
}

void refreshPixels(PixelFunc func)
{
    if (isCalibrated) func = getDPadPixel;
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

/*---------------------------------------------------------------------------*/

static void readEEPROM(uint8_t address, uint8_t *pData, uint8_t len)
{
    while (len--) *pData++ = EEPROM.read(address++);
}

static void writeEEPROM(uint8_t address, uint8_t *pData, uint8_t len)
{
    while (len--) EEPROM.update(address++, *pData++);
}

static void manageCalibration(int16_t tiltX, int16_t tiltY, int16_t tiltZ)
{
    static int16_t lastTiltX = 0, lastTiltY = 0, lastTiltZ = 0;
    static int16_t offsetTiltX = 0, offsetTiltY = 0, offsetTiltZ = 0;
    static uint8_t samples = 0;
    if (tiltZ < -TILT_1G / 2 &&
        abs(tiltX - lastTiltX) + abs(tiltY - lastTiltY) + abs(tiltZ - lastTiltZ) < TILT_TOLERANCE) {
        offsetTiltX += tiltX;
        offsetTiltY += tiltY;
        offsetTiltZ += tiltZ + TILT_1G;
        if (++samples == TILT_OFFSET_SAMPLES) {
            uint8_t data[3];
            readEEPROM(0, data, 3);
            data[0] -= offsetTiltX / TILT_OFFSET_SAMPLES / 4 * TILT_X_SIGN;
            data[1] -= offsetTiltY / TILT_OFFSET_SAMPLES / 4 * TILT_Y_SIGN;
            data[2] -= offsetTiltZ / TILT_OFFSET_SAMPLES / 4 * TILT_Z_SIGN;
            SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_OFSX, data, 3);
            writeEEPROM(0, data, 3);
            isCalibrated = true;
        }
    } else if (samples > 0){
        offsetTiltX = offsetTiltY = offsetTiltZ = 0;
        samples = 0;
    }
    lastTiltX = tiltX;
    lastTiltY = tiltY;
    lastTiltZ = tiltZ;
}

static void getDPadPixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b)
{
    r = g = b = 0;
    if (y > 0 && y < BOARD_SIZE - 1) {
        if (x == 0) getDPadPixelSub(-currentVx, -lastVx, r, g, b);
        if (x == BOARD_SIZE - 1) getDPadPixelSub(currentVx, lastVx, r, g, b);
    }
    if (x > 0 && x < BOARD_SIZE - 1) {
        if (y == 0) getDPadPixelSub(-currentVy, -lastVy, r, g, b);
        if (y == BOARD_SIZE - 1) getDPadPixelSub(currentVy, lastVy, r, g, b);
    }
}

static void getDPadPixelSub(int8_t current, int8_t last, uint8_t &r, uint8_t &g, uint8_t &b)
{
    if (current > 0) {
        if (last <= 0) {
            r = g = 8;
        } else {
            r = g = b = 2;
        }
    } else if (last > 0) {
        b = 8;
    }
}
