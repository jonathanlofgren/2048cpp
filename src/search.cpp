#include "search.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <future>
#include <limits>
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

// Per-row lookup tables
double GradScore[SHIFTED_ROWS];   // snake gradient
double MonoScore[UNIQUE_ROWS];    // monotonicity penalty (power-weighted)
double SmoothScore[UNIQUE_ROWS];  // smoothness penalty
double EmptyScore[UNIQUE_ROWS];   // empty tile count per row
double MergeScore[UNIQUE_ROWS];   // adjacent equal tile count

const int MAX_DEPTH = 5;
const double PROBABILITY_CUTOFF = 0.001;
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

        // Snake gradient: weighted sum of actual tile values × position weight
        for (Row r = ROW_1; r <= ROW_4; ++r) {
            double value = 0;
            for (int c = 0; c < 4; ++c) {
                double tile_val = (exp[c] > 0) ? (double)(1 << exp[c]) : 0;
                value += SnakeGrad[r][c] * tile_val;
            }
            GradScore[UNIQUE_ROWS * r + b] = value;
        }

        // Empty count for this row
        int row_empty = 0;
        for (int i = 0; i < 4; ++i)
            if (exp[i] == 0) ++row_empty;
        EmptyScore[b] = row_empty;

        // Monotonicity: penalty using actual tile power values
        // This makes inversions among high tiles much more costly
        double left_pen = 0, right_pen = 0;
        for (int i = 0; i < 3; ++i) {
            double v_cur  = (exp[i]   > 0) ? (double)(1 << exp[i])   : 0;
            double v_next = (exp[i+1] > 0) ? (double)(1 << exp[i+1]) : 0;
            if (v_next > v_cur) left_pen  += v_next - v_cur;
            if (v_cur > v_next) right_pen += v_cur - v_next;
        }
        MonoScore[b] = -std::min(left_pen, right_pen);

        // Smoothness: penalty for differences between adjacent non-zero tiles
        double smooth = 0;
        for (int i = 0; i < 3; ++i) {
            if (exp[i] != 0 && exp[i + 1] != 0) {
                smooth -= std::abs(exp[i] - exp[i + 1]);
            }
        }
        SmoothScore[b] = smooth;

        // Merge potential: count adjacent equal non-zero tiles
        int merges = 0;
        for (int i = 0; i < 3; ++i) {
            if (exp[i] != 0 && exp[i] == exp[i + 1])
                ++merges;
        }
        MergeScore[b] = merges;
    }
}

double evaluate_board(Bitboard board) {
    double grad = 0, mono = 0, smooth = 0, empty = 0, merge = 0;

    for (Row r = ROW_1; r <= ROW_4; ++r) {
        Bitboard row_bits = get_bits(board, r);
        grad   += GradScore[UNIQUE_ROWS * r + row_bits];
        mono   += MonoScore[row_bits];
        smooth += SmoothScore[row_bits];
        empty  += EmptyScore[row_bits];
        merge  += MergeScore[row_bits];
    }
    for (Col c = COL_1; c <= COL_4; ++c) {
        Bitboard col_bits = get_bits(board, c);
        mono   += MonoScore[col_bits];
        smooth += SmoothScore[col_bits];
        merge  += MergeScore[col_bits];
    }

    return GRAD_WEIGHT * grad + MONO_WEIGHT * mono + SMOOTH_WEIGHT * smooth
         + EMPTY_WEIGHT * empty + MERGE_WEIGHT * merge;
}

void expand_inplace(Bitboard b, Bitboard *expanded) {
    int i = 0;
    for (Square s = SQ_11; s <= SQ_44; ++s) {
        if (!(b & SquareMask[s])) { 
            expanded[i++] = b | (0x1ULL << SquareOffset[s]);    // Set a 2 in the empty square (probability 0.9)
            expanded[i++] = b | (0x2ULL << SquareOffset[s]);    // Set a 4 in the empty square (probability 0.1)
        }
    }
    expanded[i] = 0;    // make sure to set zero to indicate end
    expanded[31] = i;   // indicate how many were empty
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
    Bitboard expanded[32];
    expand_inplace(board, expanded);

    double expected_value = 0;
    double prob_sum = (double)expanded[31]/2.0;
    double prob2 = 0.9/prob_sum;
    double prob4 = 0.1/prob_sum;

    // Early out: if the most probable child is already below cutoff,
    // all children will be leaves — skip the expansion entirely.
    if (prob * prob2 < PROBABILITY_CUTOFF)
        return evaluate(board);

    Bitboard *curr = expanded;
    while (*curr) {
        Bitboard b2 = *(curr++);
        Bitboard b4 = *(curr++);
        expected_value += prob2*_value_max_node(b2, depth+1, prob*prob2) +
                          prob4*_value_max_node(b4, depth+1, prob*prob4);
    }

    // Flush thread-local counter when a top-level async task finishes
    if (depth == 0)
        flush_tl_nodes();

    return expected_value;
}


double Search::_value_max_node(Bitboard board, int depth, double prob) {
    if (depth >= MAX_DEPTH || prob < PROBABILITY_CUTOFF)
        return evaluate(board);

    auto possible = possible_moves(board);

    if (possible.size() == 0) {
        return 0.0;
    }

    double max = std::numeric_limits<double>::lowest();

    for (auto it = possible.begin(); it != possible.end(); ++it) {
        double val = _value_expected_node(it->board, depth, prob);
        if (val > max) max = val;
    }

    return max;
}

Search::Result Search::expectimax_parallel(Bitboard board) {
    auto possible = possible_moves(board);
    int n = possible.size();

    if (n == 0) {
        return {NULL_MOVE, 0};
    }

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
