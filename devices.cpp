#include "common.h"
#define SimpleWire_SCL_PORT B
#define SimpleWire_SCL_POS  2
#define SimpleWire_SDA_PORT B
#define SimpleWire_SDA_POS  1
#include "SimpleWire.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

/*  Defines  */

#define BUTTON_PIN          0
#define BUTTON_FRAMES_SAVE  100 // 5 seconds

#define ADXL345_I2C_ADDR            0x53
#define ADXL345_REG_OFSX            0x1E
#define ADXL345_REG_BW_RATE         0x2C
#define ADXL345_REG_POWER_CTL       0x2D
#define ADXL345_REG_DATA_FORMAT     0x31
#define ADXL345_REG_DATAX0          0x32
#define ADXL345_VAL_LOW_POWER_25HZ  0x18
#define ADXL345_VAL_FULL_RES_2G     0x08
#define ADXL345_VAL_MEASURE         0x08

#define TILT_ON             80
#define TILT_OFF            30
#define TILT_1G             256
#define TILT_TOLERANCE      12
#define TILT_OFFSET_SAMPLES 32

#define PIXELS_PIN          3
#define PIXELS_NUMBER       (BOARD_SIZE * BOARD_SIZE)
#define BRIGHTNESS_MAX      4

/*  Typedefs  */

typedef SimpleWire<SimpleWire_1M> SimpleWire1M;
typedef void (*PixelFunc)(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);

/*  Local Functions  */

static void readEEPROM(uint8_t address, uint8_t *pData, uint8_t len);
static void writeEEPROM(uint8_t address, uint8_t *pData, uint8_t len);
static void manageCalibration(int16_t x, int16_t y, int16_t z);
static void controlBrightness(void);
static void saveConfig(void);
static void getDPadPixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);
static void getDPadPixelSub(int8_t current, int8_t last, uint8_t &r, uint8_t &g, uint8_t &b);

/*  Local Variables  */

static Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXELS_NUMBER, PIXELS_PIN, NEO_GRB + NEO_KHZ800);
static int8_t lastVx, lastVy, currentVx, currentVy, brightness;
static bool isCalibrated;

/*---------------------------------------------------------------------------*/

void initDevices(void)
{
    uint8_t data[4];
    readEEPROM(0, data, 4);
    brightness = data[3];
    if (brightness > BRIGHTNESS_MAX) brightness = 1;

    /*  Button  */
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    /*  Accelerometer  */
    SimpleWire1M::begin();
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_OFSX, data, 3);
    data[0] = ADXL345_VAL_LOW_POWER_25HZ;
    data[1] = ADXL345_VAL_MEASURE;
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_BW_RATE, data, 2);
    data[0] = ADXL345_VAL_FULL_RES_2G;
    SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATA_FORMAT, data, 1);
    currentVx = currentVy = 0;
    isCalibrated = false;

    /*  NeoPixel  */
    pixels.begin();
    pixels.fill(pixels.Color(1, 1, 1));
    pixels.show();
    delay(MILLIS_PER_FRAME * 8);
    brightness--;
    controlBrightness();

    /*  Random Seed  */
    SimpleWire1M::readWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATAX0, data, 4);
    randomSeed((unsigned long)*data);
}

void getDPad(int8_t &vx, int8_t &vy)
{
    lastVx = currentVx;
    lastVy = currentVy;
    uint8_t dac[6];
    if (SimpleWire1M::readWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATAX0, dac, sizeof(dac)) > 0) {
        int16_t x = (dac[1] << 8) | dac[0];
        int16_t y = (dac[3] << 8) | dac[2];
        int16_t z = (dac[5] << 8) | dac[4];
        if (!isCalibrated) manageCalibration(x, y, z);
        int16_t tiltX = y, tiltY = x; // Convert to real coordinates
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

void refreshPixels(void)
{
    PixelFunc func = (isCalibrated) ? getDPadPixel : getGamePixel;
    for (uint8_t i = 0; i < PIXELS_NUMBER; i++) {
        int8_t x = i % BOARD_SIZE, y = i / BOARD_SIZE;
        if (y & 1) x = BOARD_SIZE - 1 - x;
        uint8_t r, g, b;
        func(x, y, r, g, b);
        pixels.setPixelColor(i, r, g, b);
    }
    pixels.show();
}

void manageConfigByButton(void)
{
    static uint8_t lastButtonState = HIGH, idle = BUTTON_FRAMES_SAVE;
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (lastButtonState == HIGH) controlBrightness();
        lastButtonState = LOW;
        idle = 0;
    } else {
        lastButtonState = HIGH;
        if (idle < BUTTON_FRAMES_SAVE && ++idle == BUTTON_FRAMES_SAVE) saveConfig();
    }
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

static void manageCalibration(int16_t x, int16_t y, int16_t z)
{
    static int16_t lastX = 0, lastY = 0, lastZ = 0;
    static int16_t offsetX = 0, offsetY = 0, offsetZ = 0;
    static uint8_t samples = 0;
    if (z < -TILT_1G / 2 &&
        abs(x - lastX) + abs(y - lastY) + abs(z - lastZ) < TILT_TOLERANCE) {
        offsetX += x;
        offsetY += y;
        offsetZ += z + TILT_1G;
        if (++samples == TILT_OFFSET_SAMPLES) {
            uint8_t data[3];
            readEEPROM(0, data, 3);
            data[0] -= offsetX / TILT_OFFSET_SAMPLES / 4;
            data[1] -= offsetY / TILT_OFFSET_SAMPLES / 4;
            data[2] -= offsetZ / TILT_OFFSET_SAMPLES / 4;
            SimpleWire1M::writeWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_OFSX, data, 3);
            writeEEPROM(0, data, 3);
            isCalibrated = true;
        }
    } else if (samples > 0){
        offsetX = offsetY = offsetZ = 0;
        samples = 0;
    }
    lastX = x;
    lastY = y;
    lastZ = z;
}

void controlBrightness(void)
{
    if (++brightness >= BRIGHTNESS_MAX) brightness = 0;
    pixels.setBrightness(brightness << 6 | 0x3F);
}

static void saveConfig(void)
{
    uint8_t data = brightness;
    writeEEPROM(3, &data, 1);
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
