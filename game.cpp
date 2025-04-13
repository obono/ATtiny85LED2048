#include "common.h"

/*  Defines  */

enum : uint8_t {
    STATE_IDLE = 0,
    STATE_MOVING,
    STATE_OVER,
};

#define TILE_MAX    11

/*  Local Functions  */

static void initBoard(void);
static int8_t getTile(int8_t x, int8_t y);
static void setTile(int8_t x, int8_t y, int8_t tile);
static void addRandomTile(void);
static void prepareTiles(void);
static bool moveTiles(int8_t vx, int8_t vy);
static void updateTiles(void);
static bool isGameOver(void);

/*  Local Functions (Macros)  */

#define setMerged(x, y)     bitSet(mergedFlags, (y) * BOARD_SIZE + (x))
#define isMerged(x, y)      bitRead(mergedFlags, (y) * BOARD_SIZE + (x))

/*  Local Constants  */

PROGMEM static const uint16_t tileColors[TILE_MAX + 1] = {
    0x000, 0xE00, 0xE40, 0xCA0, 0x480, 0x041, 0x06A, 0x00F, 0x20C, 0x92C, 0xC88, 0xFFF
};

PROGMEM static const uint8_t soundStart[] = {
    72, 12, 74, 12, 76, 12, 77, 12, 79, 36, 0xFF
};
PROGMEM static const uint8_t soundOver[] = {
    55, 10, 54, 12, 53, 14, 52, 16, 51, 18, 50, 20, 49, 22, 48, 24, 0xFF
};

PROGMEM static const uint8_t soundMove[] = {
    59, 1, 0xFF
};
PROGMEM static const uint8_t soundMerge4[] = {
    74, 2, 78, 2, 0xFF
};
PROGMEM static const uint8_t soundMerge8[] = {
    77, 2, 81, 2, 85, 2, 0xFF
};
PROGMEM static const uint8_t soundMerge16[] = {
    82, 2, 87, 2, 92, 2, 97, 2, 0xFF
};
PROGMEM static const uint8_t soundMerge32[] = {
    69, 3, 81, 3, 93, 3, 72, 3, 84, 3, 96, 3, 77, 3, 89, 3, 101, 3, 0xFF
};
PROGMEM static const uint8_t soundMerge64[] = {
    71, 4, 83, 4, 95, 4, 74, 4, 86, 4, 98, 4, 79, 4, 91, 4, 103, 4, 0xFF
};
PROGMEM static const uint8_t soundMerge128[] = {
    72, 4, 84, 4, 96, 4, 76, 4, 88, 4, 100, 4, 79, 4, 91, 4, 103, 4, 84, 4, 96, 4, 108, 4, 0xFF
};
PROGMEM static const uint8_t soundMerge256[] = {
    67, 6, 72, 6, 69, 6, 74, 6, 71, 6, 76, 6, 72, 6, 77, 6, 0xFF
};
PROGMEM static const uint8_t soundMerge512[] = {
    69, 7, 73, 7, 76, 7, 81, 7, 71, 7, 74, 7, 78, 7, 83, 7, 0xFF
};
PROGMEM static const uint8_t soundMerge1024[] = {
    71, 8, 75, 8, 78, 8, 77, 8, 75, 8, 77, 8, 78, 8, 83, 8, 0xFF
};
PROGMEM static const uint8_t soundMerge2048[] = {
    72, 10, 79, 10, 76, 10, 79, 10, 81, 10, 77, 10, 83, 10, 79, 10, 84, 15, 0xFF
};

PROGMEM static const uint8_t *const soundMergeTable[] = {
    soundMove, NULL, soundMerge4, soundMerge8, soundMerge16, soundMerge32, soundMerge64,
    soundMerge128, soundMerge256, soundMerge512, soundMerge1024, soundMerge2048
};

/*  Local Variables  */

static int8_t board[BOARD_SIZE][BOARD_SIZE];
static uint16_t mergedFlags;
static int8_t empty, state, moveVx, moveVy, bestTile;
static int8_t flash, addedX, addedY, blink;

/*---------------------------------------------------------------------------*/

void initGame(void)
{
    initBoard();
    addRandomTile();
    addRandomTile();
    prepareTiles();
    bestTile = 1;
    blink = 0;
    state = STATE_IDLE;
    playScore(soundStart, TILE_MAX);
}

void updateGame(int8_t vx, int8_t vy)
{
    switch (state) {
        case STATE_IDLE:
            if (flash > 0) flash--;
            if (vx != 0 && vy == 0 || vx == 0 && vy != 0) {
                prepareTiles();
                if (moveTiles(vx, vy)) {
                    moveVx = vx;
                    moveVy = vy;
                    state = STATE_MOVING;
                    flash = 8;
                }
            }
            break;
        case STATE_MOVING:
            if (!moveTiles(moveVx, moveVy)) {
                updateTiles();
                addRandomTile();
                state = (isGameOver()) ? STATE_OVER : STATE_IDLE;
            }
            break;
        case STATE_OVER:
        default:
            break;
    }
    blink = (blink + 1) % (TILE_MAX * 2);
}

void getGamePixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b)
{
    int8_t tile = getTile(x, y);
    if (tile >= 0 && tile <= TILE_MAX) {
        uint16_t color = pgm_read_word(&tileColors[tile]);
        uint8_t w = 0, d = 0;
        if (state == STATE_OVER) {
            if (tile != bestTile && blink <= 8) d = 5 - abs(4 - blink);
        } else {
            if (isMerged(x, y)) w = flash;
            if (tile == (blink >> 1) + 1 && (blink & 1) == 0) d = 1;
            if ((flash & 1) && x == addedX && y == addedY) d = (flash >> 1);
        }
        r = (((color >> 7) & 0x1E) >> d) + w;
        g = (((color >> 3) & 0x1E) >> d) + w;
        b = (((color << 1) & 0x1E) >> d) + w;
    }
}

/*---------------------------------------------------------------------------*/

static void initBoard(void)
{
    memset(board, 0, sizeof(board));
    empty = BOARD_SIZE * BOARD_SIZE;
}

static int8_t getTile(int8_t x, int8_t y)
{
    return (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) ? board[y][x] : -1;
}

static void setTile(int8_t x, int8_t y, int8_t tile)
{
    if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) board[y][x] = tile;
}

static void addRandomTile(void)
{
    int8_t position = random() % empty;
    for (int8_t y = 0; y < BOARD_SIZE; y++) {
        for (int8_t x = 0; x < BOARD_SIZE; x++) {
            if (getTile(x, y) == 0 && position-- == 0) {
                setTile(x, y, (random() % 10 == 0) ? 2 : 1);
                addedX = x;
                addedY = y;
                empty--;
                return;
            }
        }
    }
}

static void prepareTiles(void)
{
    mergedFlags = 0;
    addedX = addedY = -1;
    flash = 0;
}

static bool moveTiles(int8_t vx, int8_t vy)
{
    bool moved = false;
    for (int8_t i = 0; i < BOARD_SIZE; i++) {
        for (int8_t j = 0; j < BOARD_SIZE; j++) {
            int8_t x = (vx > 0) ? BOARD_SIZE - 1 - j : j;
            int8_t y = (vy > 0) ? BOARD_SIZE - 1 - i : i;
            int8_t tile = getTile(x, y);
            if (tile != 0) {
                int8_t nextTile = getTile(x + vx, y + vy);
                if (nextTile == 0) {
                    setTile(x, y, 0);
                    setTile(x + vx, y + vy, tile);
                    moved = true;
                } else if (nextTile == tile && !isMerged(x + vx, y + vy)) {
                    setTile(x, y, 0);
                    setMerged(x + vx, y + vy);
                    empty++;
                    moved = true;
                }
            }
        }
    }
    return moved;
}

static void updateTiles(void)
{
    uint8_t soundValue = 0;
    for (int8_t y = 0; y < BOARD_SIZE; y++) {
        for (int8_t x = 0; x < BOARD_SIZE; x++) {
            if (isMerged(x, y)) {
                int8_t tile = getTile(x, y) + 1;
                if (bestTile < tile) bestTile = tile;
                if (soundValue < tile) soundValue = tile;
                setTile(x, y, tile);
            }
        }
    }
    playScore((const uint8_t *)pgm_read_word(&soundMergeTable[soundValue]), soundValue);
}

static bool isGameOver(void)
{
    if (bestTile == TILE_MAX) return true;
    if (empty > 0) return false;
    for (int8_t y = 0; y < BOARD_SIZE; y++) {
        for (int8_t x = 0; x < BOARD_SIZE; x++) {
            int8_t tile = getTile(x, y);
            if (tile == 0) return false;
            if (getTile(x - 1, y) == tile || getTile(x + 1, y) == tile ||
                getTile(x, y - 1) == tile || getTile(x, y + 1) == tile) return false;
        }
    }
    playScore(soundOver, TILE_MAX);
    return true;
}
