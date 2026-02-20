#include "search.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <future>
#include <limits>
#include <unordered_map>
#include <vector>

#include "types.h"
#include "bitboard.h"

namespace {
    thread_local uint64_t tl_node_counter{0};
    std::atomic<uint64_t> global_node_counter{0};

}

// Snake gradient: values decrease along a zigzag path from corner.
// Row 1: left to right, Row 2: right to left, etc.
// This keeps the largest tile anchored in the corner with a natural
// path for building chains.
const double SnakeGrad[ROW_N][4] = {
    {15, 14, 13, 12},  // ROW_1: left to right
    { 8,  9, 10, 11},  // ROW_2: right to left
    { 7,  6,  5,  4},  // ROW_3: left to right
    { 0,  1,  2,  3},  // ROW_4: right to left
};

// Combined evaluation lookup tables (weights baked in at init)
double RowEval[SHIFTED_ROWS];   // per-row: grad + mono + smooth + empty + merge
double ColEval[UNIQUE_ROWS];    // per-col: mono + smooth + merge

const double PROBABILITY_CUTOFF = 0.001;

// Adaptive depth: more distinct tiles = more complex board = deeper search.
int current_max_depth;

// Transposition table: keyed on board state, stores value per depth level.
// Thread-local to avoid contention between parallel async tasks.
// Cleared at the start of each top-level move evaluation.
static constexpr int TT_DEPTHS = 8;
thread_local std::unordered_map<Bitboard, double> tt[TT_DEPTHS];

void tt_clear() {
    for (int i = 0; i < TT_DEPTHS; ++i)
        tt[i].clear();
}

int count_distinct_tiles(Bitboard board) {
    uint16_t seen = 0;
    while (board) {
        seen |= (1 << (board & 0xF));
        board >>= 4;
    }
    seen &= ~1;  // exclude 0 (empty)
    return __builtin_popcount(seen);
}
const double GRAD_WEIGHT   = 5.0;
const double MONO_WEIGHT   = 1.0;
const double SMOOTH_WEIGHT = 0.1;
const double EMPTY_WEIGHT  = 100.0;
const double MERGE_WEIGHT  = 10.0;

void Search::init() {

    for (Bitboard b = 0x0ULL; b < UNIQUE_ROWS; ++b) {

        // Extract nibble exponents
        int exp[4];
        exp[0] = (b >>  0) & 0xF;
        exp[1] = (b >>  4) & 0xF;
        exp[2] = (b >>  8) & 0xF;
        exp[3] = (b >> 12) & 0xF;

        // Monotonicity: penalty using actual tile power values
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
        ColEval[b] = MONO_WEIGHT * mono + SMOOTH_WEIGHT * smooth
                   + MERGE_WEIGHT * merge;

        // Row eval: column eval + gradient + empty (both are row-dependent)
        for (Row r = ROW_1; r <= ROW_4; ++r) {
            double grad = 0;
            for (int c = 0; c < 4; ++c) {
                double tile_val = (exp[c] > 0) ? (double)(1 << exp[c]) : 0;
                grad += SnakeGrad[r][c] * tile_val;
            }
            RowEval[UNIQUE_ROWS * r + b] = GRAD_WEIGHT * grad + ColEval[b]
                                          + EMPTY_WEIGHT * empty;
        }
    }
}

double evaluate_board(Bitboard board) {
    double score = 0;
    Bitboard t = transpose(board);

    for (Row r = ROW_1; r <= ROW_4; ++r) {
        score += RowEval[UNIQUE_ROWS * r + get_bits(board, r)];
        score += ColEval[get_bits(t, r)];
    }

    return score;
}

// Generate all possible boards resulting from the game placing a random tile.
// For each empty square on board `b`, produces two successor boards: one with
// a 2-tile (bits=1, 90% probability) and one with a 4-tile (bits=2, 10%).
//
// Output: fills `out[]` with interleaved pairs [board_with_2, board_with_4, ...].
//         For N empty squares, writes 2*N entries starting at out[0].
// Returns: the number of empty squares (N).
//
// Empty squares are found via bit manipulation: inverting the board and ANDing
// shifted copies isolates a 1-bit at the LSB of each zero nibble. These bits
// are then iterated with ctz + clear-lowest-bit.
int generate_tile_placements(Bitboard b, Bitboard *out) {
    Bitboard empty = ~b & (~b >> 1) & (~b >> 2) & (~b >> 3)
                   & 0x1111111111111111ULL;
    int i = 0;
    while (empty) {
        int bit = __builtin_ctzll(empty);
        out[i++] = b | (1ULL << bit);    // 2-tile (probability 0.9)
        out[i++] = b | (2ULL << bit);    // 4-tile (probability 0.1)
        empty &= empty - 1;              // clear lowest set bit
    }
    return i / 2;
}

double Search::evaluate(Bitboard b) {
    ++tl_node_counter;
    return evaluate_board(b);
}

uint64_t Search::get_nodes() {
    return global_node_counter.load(std::memory_order_relaxed) + tl_node_counter;
}

void Search::reset_nodes() {
    global_node_counter.store(0, std::memory_order_relaxed);
    tl_node_counter = 0;
}


static void flush_tl_nodes() {
    global_node_counter.fetch_add(tl_node_counter, std::memory_order_relaxed);
    tl_node_counter = 0;
}

double Search::_value_expected_node(Bitboard board, int depth, double prob) {
    // Clear TT at the start of each top-level async task
    if (depth == 0)
        tt_clear();

    Bitboard expanded[32];
    int n_empty = generate_tile_placements(board, expanded);

    double expected_value = 0;
    double prob2 = 0.9 / n_empty;
    double prob4 = 0.1 / n_empty;

    // Early out: if the most probable child is already below cutoff,
    // all children will be leaves — skip the expansion entirely.
    if (prob * prob2 < PROBABILITY_CUTOFF)
        return evaluate(board);

    for (int i = 0; i < n_empty * 2; i += 2) {
        expected_value += prob2 * _value_max_node(expanded[i],   depth + 1, prob * prob2) +
                          prob4 * _value_max_node(expanded[i+1], depth + 1, prob * prob4);
    }

    // Flush thread-local counter when a top-level async task finishes
    if (depth == 0)
        flush_tl_nodes();

    return expected_value;
}


double Search::_value_max_node(Bitboard board, int depth, double prob) {
    if (depth >= current_max_depth || prob < PROBABILITY_CUTOFF)
        return evaluate(board);

    // Transposition table lookup
    auto& table = tt[depth];
    auto tt_it = table.find(board);
    if (tt_it != table.end())
        return tt_it->second;

    auto possible = possible_moves(board);

    if (possible.size() == 0) {
        table[board] = 0.0;
        return 0.0;
    }

    double max = std::numeric_limits<double>::lowest();

    for (auto it = possible.begin(); it != possible.end(); ++it) {
        double val = _value_expected_node(it->board, depth, prob);
        if (val > max) max = val;
    }

    table[board] = max;
    return max;
}

Search::Result Search::expectimax_parallel(Bitboard board) {
    auto possible = possible_moves(board);
    int n = possible.size();

    if (n == 0) {
        return {NULL_MOVE, 0};
    }

    // Adaptive depth: complex boards (more distinct tiles) get deeper search
    int distinct = count_distinct_tiles(board);
    current_max_depth = std::max(3, std::min(7, distinct - 2));

    std::vector<std::future<double>> futures(n);
    for (int i = 0; i < n; i++) {
        futures[i] = std::async(std::launch::async, _value_expected_node, possible[i].board, 0, 1);
    }

    double values[MOVE_N];
    for (int i = 0; i < n; i++) {
        values[possible[i].move] = futures[i].get();
    }

    Move best_move = NULL_MOVE;
    double max_value = std::numeric_limits<double>::lowest();

    for (const PossibleMove & pm: possible) {
        if (values[pm.move] > max_value) {
            best_move = pm.move;
            max_value = values[pm.move];
        }
    }

    return {best_move, max_value};
}
