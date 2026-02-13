#include "search.h"

#include <atomic>
#include <future>
#include <limits>
#include <vector>

#include "types.h"
#include "bitboard.h"

namespace {
    thread_local uint64_t tl_node_counter{0};
    std::atomic<uint64_t> global_node_counter{0};
}

const double DiagLinGrad[SQUARE_N] = {
    1.00, 0.83, 0.66, 0.50,
    0.84, 0.67, 0.51, 0.33,
    0.68, 0.52, 0.34, 0.17,
    0.53, 0.35, 0.18, 0.00
};

double RowValue[SHIFTED_ROWS];

const int MAX_DEPTH = 4;
const double PROBABILITY_CUTOFF = 0.001;

void Search::init() {

    for (Bitboard b = 0x0ULL; b < UNIQUE_ROWS; ++b) {
        int empty = empty_squares(b) - 12;
        double empty_score = 1.0 + 1.0*empty/100;

        // set up row values
        for (Row r = ROW_1; r <= ROW_4; ++r) {
            Bitboard row = b << RowOffset[r];

            double value = 0;
            for (Square s = SQ_11; s <= SQ_44; ++s)
                value += DiagLinGrad[s] * DiagLinGrad[s] * bits_to_value(get_bits(row, s)) * empty_score;

            RowValue[UNIQUE_ROWS * r + b] = value;
        }
    }
}

double gradient_value_map(Bitboard board) {
    double value = 0;
    for (Row r = ROW_1; r <= ROW_4; ++r) {
        value += RowValue[UNIQUE_ROWS * r + get_bits(board, r)];
    }

    return value;
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
    return gradient_value_map(b);
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
