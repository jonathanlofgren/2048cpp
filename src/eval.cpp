#include "eval.h"

#include <cmath>

#include "bitboard.h"

// Tile weight: rank * 2^rank (super-exponential).
// Makes high-tile placement astronomically more important:
//   rank 1 (tile 2)    -> 2
//   rank 5 (tile 32)   -> 160
//   rank 10 (tile 1024) -> 10240
//   rank 11 (tile 2048) -> 22528
//   rank 13 (tile 8192) -> 106496
static float tile_weight[16];

// Snake path weights: largest in one corner, zigzag to smallest in opposite.
static const float Snake[ROW_N][4] = {
    {15, 14, 13, 12},
    { 8,  9, 10, 11},
    { 7,  6,  5,  4},
    { 0,  1,  2,  3},
};

static float RowEval[SHIFTED_ROWS];
static float ColEval[UNIQUE_ROWS];

void Eval::init() {
    tile_weight[0] = 0;
    for (int r = 1; r < 16; ++r)
        tile_weight[r] = (float)(r * (1 << r));

    for (int b = 0; b < UNIQUE_ROWS; ++b) {
        int r[4] = {
            (b >>  0) & 0xF,
            (b >>  4) & 0xF,
            (b >>  8) & 0xF,
            (b >> 12) & 0xF,
        };

        // Empty cells: survival
        float empty = 0;
        for (int i = 0; i < 4; ++i)
            if (r[i] == 0) empty += 1.0f;

        // Merges: adjacent equal non-zero tiles
        float merges = 0;
        for (int i = 0; i < 3; ++i)
            if (r[i] != 0 && r[i] == r[i + 1])
                merges += 1.0f;

        // Monotonicity: rank^MONO_POWER penalty for out-of-order pairs.
        float mono_l = 0, mono_r = 0;
        for (int i = 0; i < 3; ++i) {
            float a = std::pow((float)r[i],   MONO_POWER);
            float b_ = std::pow((float)r[i+1], MONO_POWER);
            if (a > b_) mono_l += a - b_;
            else        mono_r += b_ - a;
        }
        float mono = -std::min(mono_l, mono_r);

        // ColEval: monotonicity + merges (no positional info)
        ColEval[b] = MONO_WEIGHT * mono + MERGE_WEIGHT * merges;

        // RowEval: add snake gradient + empty bonus (row-position dependent)
        for (Row row = ROW_1; row <= ROW_4; ++row) {
            float grad = 0;
            for (int c = 0; c < 4; ++c)
                grad += Snake[row][c] * tile_weight[r[c]];

            RowEval[UNIQUE_ROWS * row + b] =
                GRAD_WEIGHT * grad + ColEval[b] + EMPTY_WEIGHT * empty;
        }
    }
}

float Eval::evaluate(Bitboard board) {
    Bitboard t = transpose(board);
    return RowEval[                   (board        & 0xFFFF)]
         + RowEval[    UNIQUE_ROWS + ((board >> 16) & 0xFFFF)]
         + RowEval[2 * UNIQUE_ROWS + ((board >> 32) & 0xFFFF)]
         + RowEval[3 * UNIQUE_ROWS +  (board >> 48)          ]
         + ColEval[ t        & 0xFFFF]
         + ColEval[(t >> 16) & 0xFFFF]
         + ColEval[(t >> 32) & 0xFFFF]
         + ColEval[ t >> 48          ];
}
