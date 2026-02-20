#include "search.h"

#include <algorithm>
#include <atomic>
#include <future>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "types.h"
#include "bitboard.h"
#include "eval.h"
#include "tt.h"
#include "threadpool.h"

namespace {

thread_local uint64_t tl_node_counter{0};
std::atomic<uint64_t> global_node_counter{0};

void flush_tl_nodes() {
    global_node_counter.fetch_add(tl_node_counter, std::memory_order_relaxed);
    tl_node_counter = 0;
}

const double PROBABILITY_CUTOFF = 0.001;

// Adaptive depth: more distinct tiles = more complex board = deeper search.
int current_max_depth;

// Thread pool
std::unique_ptr<ThreadPool> pool;

int count_distinct_tiles(Bitboard board) {
    uint16_t seen = 0;
    while (board) {
        seen |= (1 << (board & 0xF));
        board >>= 4;
    }
    seen &= ~1;  // exclude 0 (empty)
    return __builtin_popcount(seen);
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

// Forward declarations for mutual recursion
float value_expected_node(Bitboard board, int depth, double prob);
float value_max_node(Bitboard board, int depth, double prob);

float value_expected_node(Bitboard board, int depth, double prob) {
    Bitboard expanded[32];
    int n_empty = generate_tile_placements(board, expanded);

    float expected_value = 0;
    double prob2 = 0.9 / n_empty;
    double prob4 = 0.1 / n_empty;

    // Early out: if the most probable child is already below cutoff,
    // all children will be leaves — skip the expansion entirely.
    if (prob * prob2 < PROBABILITY_CUTOFF)
        return Search::evaluate(board);

    bool search4 = prob * prob4 >= PROBABILITY_CUTOFF;

    for (int i = 0; i < n_empty * 2; i += 2) {
        expected_value += (float)(prob2 * value_max_node(expanded[i], depth + 1, prob * prob2));
        if (search4)
            expected_value += (float)(prob4 * value_max_node(expanded[i+1], depth + 1, prob * prob4));
        else
            expected_value += (float)(prob4 * Search::evaluate(expanded[i+1]));
    }

    return expected_value;
}

float value_max_node(Bitboard board, int depth, double prob) {
    if (depth >= current_max_depth || prob < PROBABILITY_CUTOFF)
        return Search::evaluate(board);

    float cached;
    if (TT::probe(board, depth, cached))
        return cached;

    auto possible = possible_moves(board);

    float result;
    if (possible.size() == 0) {
        result = 0.0f;
    } else {
        result = std::numeric_limits<float>::lowest();
        for (auto it = possible.begin(); it != possible.end(); ++it) {
            float val = value_expected_node(it->board, depth, prob);
            if (val > result) result = val;
        }
    }

    TT::store(board, depth, result);
    return result;
}

} // anonymous namespace


void Search::init() {
    Eval::init();
}

void Search::init_pool(int num_threads) {
    if (num_threads <= 0)
        num_threads = std::max(1, (int)std::thread::hardware_concurrency());
    pool = std::make_unique<ThreadPool>(num_threads);
}

void Search::shutdown_pool() {
    pool.reset();
}

float Search::evaluate(Bitboard b) {
    ++tl_node_counter;
    return Eval::evaluate(b);
}

uint64_t Search::get_nodes() {
    return global_node_counter.load(std::memory_order_relaxed) + tl_node_counter;
}

void Search::reset_nodes() {
    global_node_counter.store(0, std::memory_order_relaxed);
    tl_node_counter = 0;
}

Search::Result Search::expectimax_parallel(Bitboard board) {
    auto possible = possible_moves(board);
    int n = possible.size();

    if (n == 0) {
        return {NULL_MOVE, 0};
    }

    // Adaptive depth: complex boards get deeper search.
    // Base depth from distinct tile count, boosted when board is nearly full.
    int distinct = count_distinct_tiles(board);
    int empties = empty_squares(board);
    int base_depth = std::max(3, std::min(8, distinct - 2));
    current_max_depth = (empties <= 4) ? std::min(base_depth + 1, 9) : base_depth;

    TT::clear();

    // For each legal move, expand the tile-placement level on the main thread
    // and submit individual value_max_node calls to the pool. This creates
    // ~20 tasks per move (~80 total) instead of ~4 tasks per search step.
    float move_values[MOVE_N] = {};

    struct TaskResult {
        float value;
        int move_idx;
        float weight;  // prob2 or prob4
    };

    std::vector<std::future<TaskResult>> futures;
    futures.reserve(128);

    for (int mi = 0; mi < n; mi++) {
        Bitboard moved = possible[mi].board;

        Bitboard expanded[32];
        int n_empty = generate_tile_placements(moved, expanded);

        float prob2 = 0.9f / n_empty;
        float prob4 = 0.1f / n_empty;

        // If the most probable child is below cutoff, all children are leaves.
        // Compute evaluate() inline — no need to submit tasks.
        if (prob2 < PROBABILITY_CUTOFF) {
            move_values[mi] = evaluate(moved);
            continue;
        }

        bool search4 = prob4 >= PROBABILITY_CUTOFF;

        for (int i = 0; i < n_empty * 2; i += 2) {
            Bitboard child2 = expanded[i];
            Bitboard child4 = expanded[i + 1];
            int idx = mi;

            // Submit 2-tile child
            futures.push_back(pool->submit([child2, prob2, idx]() -> TaskResult {
                float val = value_max_node(child2, 1, prob2);
                flush_tl_nodes();
                return {val, idx, prob2};
            }));

            // 4-tile child: only submit to pool if above cutoff,
            // otherwise evaluate as leaf on the main thread.
            if (search4) {
                futures.push_back(pool->submit([child4, prob4, idx]() -> TaskResult {
                    float val = value_max_node(child4, 1, prob4);
                    flush_tl_nodes();
                    return {val, idx, prob4};
                }));
            } else {
                move_values[idx] += prob4 * evaluate(child4);
            }
        }
    }

    // Collect results
    for (auto &fut : futures) {
        TaskResult tr = fut.get();
        move_values[tr.move_idx] += tr.weight * tr.value;
    }

    Move best_move = NULL_MOVE;
    float max_value = std::numeric_limits<float>::lowest();

    for (int i = 0; i < n; i++) {
        if (move_values[i] > max_value) {
            best_move = possible[i].move;
            max_value = move_values[i];
        }
    }

    return {best_move, max_value};
}
