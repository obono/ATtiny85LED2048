#include <Arduino.h>

/*  Defines  */

#define SIZE    4

/*  Global Functions  */

void initGame(void);
void updateGame(int8_t vx, int8_t vy);

/*  Local Functions  */

static int8_t getTile(int8_t x, int8_t y);
static void setTile(int8_t x, int8_t y, int8_t tile);
static void addRandomTile(void);
static bool moveTiles(int8_t vx, int8_t vy);
static bool isGameOver(void);

/*  Local Constants  */

PROGMEM static const uint16_t tileColors[] = {
    0x000, 0xE00, 0xE40, 0xCA0, 0x480, 0x041, 0x046, 0x00F, 0x20C, 0x92C, 0xC88, 0xFFF,
};
/*
    0,  0,  0,  // 0
    28, 0,  0,  // 1
    28, 8,  0,  // 2
    24, 20, 0,  // 3
    8,  16, 0,  // 4
    0,  8,  2,  // 5
    0,  8,  12, // 6
    0,  0,  32, // 7
    4,  0,  24, // 8
    18, 4,  24, // 9
    24, 16, 16, // 10
    32, 32, 32, // 11
*/

/*  Local Variables  */

static int8_t board[SIZE][SIZE];
static int8_t empty;

/*---------------------------------------------------------------------------*/

void initGame(void)
{
    memset(board, 0, sizeof(board));
    empty = SIZE * SIZE;
    addRandomTile();
    addRandomTile();
}

void updateGame(int8_t vx, int8_t vy)
{
    if ((vx != 0 ||  vy != 0) && moveTiles(vx, vy)) {
        addRandomTile();
        if (isGameOver()) {
            // TODO
        }
    }
}

void getGamePixel(int8_t x, int8_t y, uint8_t &r, uint8_t &g, uint8_t &b)
{
    int8_t tile = getTile(x, y);
    if (tile >= 0 && tile <= 11) {
        uint16_t color = pgm_read_word(&tileColors[tile]);
        r = (color >> 7) & 0x1E;
        g = (color >> 3) & 0x1E;
        b = (color << 1) & 0x1E;
    }
}

/*---------------------------------------------------------------------------*/

static int8_t getTile(int8_t x, int8_t y)
{
    return (x >= 0 && x < SIZE && y >= 0 && y < SIZE) ? board[y][x] : -1;
}

static void setTile(int8_t x, int8_t y, int8_t tile)
{
    if (x >= 0 && x < SIZE && y >= 0 && y < SIZE) board[y][x] = tile;
}

static void addRandomTile(void)
{
    int8_t position = random() % empty;
    for (int8_t y = 0; y < SIZE; y++) {
        for (int8_t x = 0; x < SIZE; x++) {
            if (getTile(x, y) == 0 && position-- == 0) {
                setTile(x, y, (random() % 10 == 0) ? 2 : 1);
                empty--;
                return;
            }
        }
    }
}

static bool moveTiles(int8_t vx, int8_t vy)
{
    bool moved = false;
    bool merged[SIZE][SIZE] = { false };
    for (int8_t i = 0; i < SIZE; i++) {
        for (int8_t j = 0; j < SIZE; j++) {
            int8_t x = (vx > 0) ? SIZE - 1 - j : j;
            int8_t y = (vy > 0) ? SIZE - 1 - i : i;
            int8_t tile = getTile(x, y);
            if (tile != 0) {
                board[y][x] = 0;
                while (getTile(x + vx, y + vy) == 0) {
                    x += vx;
                    y += vy;
                    moved = true;
                }
                if (getTile(x + vx, y + vy) == tile && !merged[y + vy][x + vx]) {
                    x += vx;
                    y += vy;
                    tile++;
                    merged[y][x] = true;
                    empty++;
                    moved = true;
                }
                setTile(x, y, tile);
            }
        }
    }
    return moved;
}

static bool isGameOver(void)
{
    if (empty > 0) return false;
    for (int8_t y = 0; y < SIZE; y++) {
        for (int8_t x = 0; x < SIZE; x++) {
            int8_t tile = board[y][x];
            if (tile == 0) return false;
            if (getTile(x - 1, y) == tile || getTile(x + 1, y) == tile ||
                getTile(x, y - 1) == tile || getTile(x, y + 1) == tile) return false;
        }
    }
    return true;
}
