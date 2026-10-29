#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../boards/stopwatch/MotoNav/RasterContinuity.h"

static uint8_t getPx(const uint8_t *buf, uint16_t w, int x, int y) {
    uint32_t a = rasterNibbleAddr(w, x, y);
    return (a & 1) ? (buf[a >> 1] & 15) : (buf[a >> 1] >> 4);
}

static size_t encodeRle(const uint8_t *grid, int n, uint8_t *out) {
    size_t p = 0;
    int i = 0;
    while (i < n) {
        uint8_t c = grid[i] & 15;
        int run = 1;
        while (i + run < n && run < 271 && (grid[i + run] & 15) == c) run++;
        if (run <= 15) {
            out[p++] = (uint8_t)(((run - 1) << 4) | c);
        } else {
            out[p++] = (uint8_t)((15 << 4) | c);
            out[p++] = (uint8_t)(run - 16);
        }
        i += run;
    }
    return p;
}

int main() {
    const uint16_t W = 64, H = 64;
    const int N = W * H;
    uint8_t gridA[N], gridB[N], xorGrid[N];
    uint8_t rleA[N * 2], rleDelta[N * 2];
    uint8_t bufA[N / 2], bufB[N / 2];

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            gridA[y * W + x] = (uint8_t)((x * 3 + y * 5) & 7);
        }
    }
    size_t lenA = encodeRle(gridA, N, rleA);
    assert(rasterDecode(rleA, lenA, bufA, W, H, 7, false));
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            assert(getPx(bufA, W, x, y) == gridA[y * W + x]);
        }
    }

    int shifts[3][2] = {{0, 0}, {16, -16}, {7, -11}};
    for (int s = 0; s < 3; s++) {
        int ox = shifts[s][0], oy = shifts[s][1];
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int sx = x + ox, sy = y + oy;
                uint8_t base = (sx >= 0 && sx < W && sy >= 0 && sy < H) ? gridA[sy * W + sx] : 0;
                uint8_t actual = (uint8_t)(((x + ox) * 3 + (y + oy) * 5 + ((x & 7) == 0 ? 4 : 0)) & 7);
                gridB[y * W + x] = actual;
                xorGrid[y * W + x] = (sx >= 0 && sx < W && sy >= 0 && sy < H) ? (actual ^ base) : actual;
            }
        }
        size_t lenD = encodeRle(xorGrid, N, rleDelta);
        rasterCopyReference(bufB, bufA, W, H, ox, oy);
        assert(rasterDecode(rleDelta, lenD, bufB, W, H, 7, true));
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                assert(getPx(bufB, W, x, y) == gridB[y * W + x]);
            }
        }
    }
    printf("PASS: RasterContinuity full & delta decode\n");
    return 0;
}

