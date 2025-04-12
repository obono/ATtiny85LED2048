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
    for (int8_t y = 0; y < BOARD_SIZE; y++) {
        for (int8_t x = 0; x < BOARD_SIZE; x++) {
            if (isMerged(x, y)) {
                int8_t tile = getTile(x, y) + 1;
                if (bestTile < tile) bestTile = tile;
                setTile(x, y, tile);
            }
        }
    }
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
    return true;
}
