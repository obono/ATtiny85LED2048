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
#define BUTTON_FRAMES_SOUND 20 // 1 second
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
#define TILT_TOLERANCE      16
#define TILT_OFFSET_SAMPLES 32

#define PIXELS_PIN          3
#define PIXELS_NUMBER       (BOARD_SIZE * BOARD_SIZE)
#define BRIGHTNESS_MAX      4

#define SPEAKER_PIN         4
#define SPEAKER_PIN_PORT    B
#define SPEAKER_PIN_POS     4

/*  Typedefs  */

typedef SimpleWire<SimpleWire_1M> SimpleWire1M;
typedef void (*PixelFunc)(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);

/*  Local Functions  */

static void readEEPROM(uint8_t address, uint8_t *pData, uint8_t len);
static void writeEEPROM(uint8_t address, uint8_t *pData, uint8_t len);
static void manageCalibration(int16_t x, int16_t y, int16_t z);
static void controlBrightness(void);
static void toggleSound(void);
static void saveConfig(void);
static void getDPadPixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b);
static void getDPadPixelSub(int8_t current, int8_t last, uint8_t &r, uint8_t &g, uint8_t &b);
static void forwardSoundScore(void);
static void setupSoundTimer(uint16_t frequency, uint16_t duration);

/*  Local Functions (Macros)  */

#define enableSoundTimer()      bitSet(TIMSK, OCIE1A)
#define disableSoundTimer()     bitClear(TIMSK, OCIE1A)
#define isSoundTimerActive()    bitRead(TIMSK, OCIE1A)

/*  Local Constants  */

PROGMEM static const uint16_t noteFrequency[] = {
    8372, 8870, 9397, 9956, 10548, 11175, 11840, 12544, 13290, 14080, 14917, 15804
};

PROGMEM static const uint8_t soundOn[] = {
    73, 10, 85, 10, 97, 10, 0xFF
};
PROGMEM static const uint8_t soundOff[] = {
    94, 5, 70, 5, 0xFF
};

/*  Local Variables  */

static Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXELS_NUMBER, PIXELS_PIN, NEO_GRB + NEO_KHZ800);
static int8_t lastVx, lastVy, currentVx, currentVy, brightness;
static bool isSoundEnable, isCalibrated;

static volatile uint32_t toneToggleCount;
static volatile const uint8_t *pSoundScore;
static volatile uint8_t soundValue;

/*---------------------------------------------------------------------------*/

void initDevices(void)
{
    uint8_t data[4];
    readEEPROM(0, data, 4);
    brightness = data[3] & 0x03;
    isSoundEnable = data[3] & 0x80;

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

    /*  Speaker  */
    pinMode(SPEAKER_PIN, OUTPUT);
    digitalWrite(SPEAKER_PIN, LOW);
    disableSoundTimer();
    soundValue = 0;

    /*  Random Seed  */
    SimpleWire1M::readWithCommand(ADXL345_I2C_ADDR, ADXL345_REG_DATAX0, data, 4);
    randomSeed((unsigned long)*data);
}

void getDPad(int8_t &vx, int8_t &vy)
{
    lastVx = currentVx;
    lastVy = currentVy;
    uint8_t dac[6];
    if (!isSoundTimerActive() && SimpleWire1M::readWithCommand(
            ADXL345_I2C_ADDR, ADXL345_REG_DATAX0, dac, sizeof(dac)) > 0) {
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
    static uint8_t hold = BUTTON_FRAMES_SOUND, idle = BUTTON_FRAMES_SAVE;
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (hold < BUTTON_FRAMES_SOUND && ++hold == BUTTON_FRAMES_SOUND) toggleSound();
        idle = 0;
    } else {
        if (hold > 0 && hold < BUTTON_FRAMES_SOUND) controlBrightness();
        if (idle < BUTTON_FRAMES_SAVE && ++idle == BUTTON_FRAMES_SAVE) saveConfig();
        hold = 0;
    }
}

void playTone(uint16_t frequency, uint16_t duration, uint8_t value)
{
    if (isSoundEnable && value >= soundValue) {
        disableSoundTimer();
        pSoundScore = NULL;
        soundValue = value;
        setupSoundTimer(frequency, duration);
    }
}

void playScore(const uint8_t *pScore, uint8_t value)
{
    if ((isSoundEnable || pScore == soundOff) && value >= soundValue) {
        disableSoundTimer();
        pSoundScore = pScore;
        if (pScore != NULL) {
            soundValue = value;
            forwardSoundScore();
        }
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

void toggleSound(void)
{
    isSoundEnable = !isSoundEnable;
    playScore((isSoundEnable) ? soundOn : soundOff, 255);
}

static void saveConfig(void)
{
    uint8_t data = isSoundEnable << 7 | brightness;
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

static void forwardSoundScore(void)
{
    uint8_t note = pgm_read_byte(pSoundScore++);
    if (bitRead(note, 7)) {
        pSoundScore = NULL;
        soundValue = 0;
        return;
    }
    uint16_t frequency = pgm_read_word(&noteFrequency[note % 12]);
    frequency >>= (131 - note) / 12;
    setupSoundTimer(frequency, pgm_read_byte(pSoundScore++) * 8);
}

static void setupSoundTimer(uint16_t frequency, uint16_t duration)
{
    frequency *= 2;
    toneToggleCount = (uint32_t)duration * frequency / 1000 + 1;
    uint32_t ocr = F_CPU / frequency;
    uint8_t prescalarBits = 0b0001;
    while (ocr > 0xff && prescalarBits < 0b1111) {
        prescalarBits++;
        ocr >>= 1;
    }
    TCCR1 = 0b10000000 | prescalarBits; // CTC1=1, PWM1A=0, COM1A=00
    OCR1C = ocr - 1;
    TCNT1 = 0;
    enableSoundTimer();
}

ISR(TIMER1_COMPA_vect)
{
    if (toneToggleCount > 0) {
        if (--toneToggleCount) {
            PORTB ^= _BV(SPEAKER_PIN_POS);
        } else {
            PORTB &= ~_BV(SPEAKER_PIN_POS);
            disableSoundTimer();
            if (pSoundScore != NULL) {
                forwardSoundScore();
            } else {
                soundValue = 0;
            }
        }
    }
}
