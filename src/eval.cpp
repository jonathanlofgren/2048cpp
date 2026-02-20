#include "eval.h"

#include <algorithm>
#include <cmath>

#include "bitboard.h"

// Snake gradient: values decrease along a zigzag path from corner.
// Row 1: left to right, Row 2: right to left, etc.
// This keeps the largest tile anchored in the corner with a natural
// path for building chains.
static const double SnakeGrad[ROW_N][4] = {
    {15, 14, 13, 12},  // ROW_1: left to right
    { 8,  9, 10, 11},  // ROW_2: right to left
    { 7,  6,  5,  4},  // ROW_3: left to right
    { 0,  1,  2,  3},  // ROW_4: right to left
};

// Combined evaluation lookup tables (weights baked in at init)
static float RowEval[SHIFTED_ROWS];   // per-row: grad + mono + smooth + empty + merge
static float ColEval[UNIQUE_ROWS];    // per-col: mono + smooth + merge

void Eval::init() {
    for (Bitboard b = 0x0ULL; b < UNIQUE_ROWS; ++b) {

        // Extract nibble exponents
        int exp[4];
        exp[0] = (b >>  0) & 0xF;
        exp[1] = (b >>  4) & 0xF;
        exp[2] = (b >>  8) & 0xF;
        exp[3] = (b >> 12) & 0xF;

        // Monotonicity: penalty using actual tile power values.
        // Power values create strong pressure to keep large tiles in order.
        double left_pen = 0, right_pen = 0;
        for (int i = 0; i < 3; ++i) {
            double v_cur  = (exp[i]   > 0) ? (double)(1 << exp[i])   : 0;
            double v_next = (exp[i+1] > 0) ? (double)(1 << exp[i+1]) : 0;
            if (v_next > v_cur) left_pen  += v_next - v_cur;
            if (v_cur > v_next) right_pen += v_cur - v_next;
        }
        double mono = -std::min(left_pen, right_pen);

        // Smoothness: penalty for differences between adjacent non-zero tiles
        double smooth = 0;
        for (int i = 0; i < 3; ++i) {
            if (exp[i] != 0 && exp[i + 1] != 0)
                smooth -= std::abs(exp[i] - exp[i + 1]);
        }

        // Merge potential: count adjacent equal non-zero tiles
        double merge = 0;
        for (int i = 0; i < 3; ++i) {
            if (exp[i] != 0 && exp[i] == exp[i + 1])
                ++merge;
        }

        // Empty count
        double empty = 0;
        for (int i = 0; i < 4; ++i)
            if (exp[i] == 0) ++empty;

        // Column eval: row-independent components only
        double col_eval = MONO_WEIGHT * mono + SMOOTH_WEIGHT * smooth
                        + MERGE_WEIGHT * merge;
        ColEval[b] = (float)col_eval;

        // Row eval: column eval + gradient + empty (both are row-dependent)
        for (Row r = ROW_1; r <= ROW_4; ++r) {
            double grad = 0;
            for (int c = 0; c < 4; ++c) {
                double tile_val = (exp[c] > 0) ? (double)(1 << exp[c]) : 0;
                grad += SnakeGrad[r][c] * tile_val;
            }
            RowEval[UNIQUE_ROWS * r + b] = (float)(GRAD_WEIGHT * grad + col_eval
                                                   + EMPTY_WEIGHT * empty);
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
