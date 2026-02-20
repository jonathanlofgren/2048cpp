#include <iostream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <map>
#include <numeric>
#include <vector>

#include "types.h"
#include "bitboard.h"
#include "eval.h"
#include "search.h"
#include "tt.h"

struct Options {
    bool verbose = false;
    int num_games = 1;
    int seed = -1;
    int num_threads = 0;
    int tt_bits = 21;
    bool tt_enabled = true;
    float grad_weight  = -1;
    float mono_weight  = -1;
    float mono_power   = -1;
    float empty_weight = -1;
    float merge_weight = -1;
};

Options parse_args(int argc, char *argv[]) {
    Options opts;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
            opts.verbose = true;
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            opts.seed = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--games") == 0 && i + 1 < argc) {
            opts.num_games = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--no-tt") == 0) {
            opts.tt_enabled = false;
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            opts.num_threads = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--tt-bits") == 0 && i + 1 < argc) {
            opts.tt_bits = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--grad-weight") == 0 && i + 1 < argc) {
            opts.grad_weight = std::stof(argv[++i]);
        } else if (std::strcmp(argv[i], "--mono-weight") == 0 && i + 1 < argc) {
            opts.mono_weight = std::stof(argv[++i]);
        } else if (std::strcmp(argv[i], "--mono-power") == 0 && i + 1 < argc) {
            opts.mono_power = std::stof(argv[++i]);
        } else if (std::strcmp(argv[i], "--empty-weight") == 0 && i + 1 < argc) {
            opts.empty_weight = std::stof(argv[++i]);
        } else if (std::strcmp(argv[i], "--merge-weight") == 0 && i + 1 < argc) {
            opts.merge_weight = std::stof(argv[++i]);
        }
    }
    return opts;
}

struct GameResult {
    int moves;
    int max_tile;
    int score;
    double total_time;
    uint64_t nodes;
};

GameResult play_game(bool verbose) {
    Bitboard board = place_random(place_random(0x0ULL));

    auto possible = possible_moves(board);
    int moves = 0;

    double total_time = 0;
    double min_time = std::numeric_limits<double>::max();
    double max_time = 0;

    Search::reset_nodes();

    while (possible.size() > 0) {
        moves++;

        auto start = std::chrono::high_resolution_clock::now();
        Search::Result result = Search::expectimax_parallel(board);
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();

        total_time += elapsed;
        double elapsed_ms = elapsed * 1000;
        min_time = std::min(min_time, elapsed_ms);
        max_time = std::max(max_time, elapsed_ms);

        // Make move — check for 65536 merge before placing random tile.
        // Two rank-15 tiles (32768) merging overflows 4-bit representation,
        // so detect it by counting rank-15 tiles before and after the move.
        Bitboard prev = board;
        board = make_move(board, result.move);

        if (verbose) {
            std::cout << "Move " << moves  << ": " << Bitboards::pretty(result.move) << std::endl;
            std::cout << "Value: " << result.value << std::endl;
        }

        if (max_value(prev) == 32768 && max_value(board) < max_value(prev)) {
            // 32768 + 32768 merge detected (overflowed to 0)
            uint64_t nodes = Search::get_nodes();
            if (verbose)
                std::cout << "65536 tile reached!" << std::endl;
            return {moves, 65536, board_score(prev), total_time, nodes};
        }

        board = place_random(board);

        if (verbose)
            std::cout << Bitboards::pretty(board) << std::endl;

        possible = possible_moves(board);
    }

    uint64_t nodes = Search::get_nodes();

    return {moves, max_value(board), board_score(board), total_time, nodes};
}

void print_single_result(const GameResult &r) {
    std::cout << "Game Over." << std::endl;
    std::cout << "Total moves:  " << r.moves << std::endl;
    std::cout << "Max tile:     " << r.max_tile << std::endl;
    std::cout << "Final score:  " << r.score << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total time:   " << r.total_time << " s" << std::endl;
    std::cout << "Avg ms/move:  " << 1000 * r.total_time / r.moves << std::endl;
    std::cout << "Nodes:        " << r.nodes << std::endl;
    std::cout << "Nodes/sec:    " << static_cast<uint64_t>(r.nodes / r.total_time) << std::endl;
}

void print_batch_summary(const std::vector<GameResult> &results) {
    int n = results.size();

    // Score statistics
    std::vector<double> scores(n);
    for (int i = 0; i < n; i++) scores[i] = results[i].score;
    std::sort(scores.begin(), scores.end());

    double sum = std::accumulate(scores.begin(), scores.end(), 0.0);
    double mean = sum / n;

    double median;
    if (n % 2 == 0)
        median = (scores[n/2 - 1] + scores[n/2]) / 2.0;
    else
        median = scores[n/2];

    double sq_sum = 0;
    for (double s : scores) sq_sum += (s - mean) * (s - mean);
    double stddev = std::sqrt(sq_sum / n);

    // Max tile distribution
    std::map<int, int> tile_dist;
    for (const auto &r : results) tile_dist[r.max_tile]++;

    // Move and time totals
    double total_moves = 0, total_time = 0;
    uint64_t total_nodes = 0;
    for (const auto &r : results) {
        total_moves += r.moves;
        total_time += r.total_time;
        total_nodes += r.nodes;
    }

    std::cout << "=== Summary ===" << std::endl;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Games:         " << n << std::endl;
    std::cout << "Avg score:     " << mean << std::endl;
    std::cout << "Median score:  " << static_cast<int>(median) << std::endl;
    std::cout << "Std dev:       " << stddev << std::endl;
    std::cout << "Min score:     " << static_cast<int>(scores.front()) << std::endl;
    std::cout << "Max score:     " << static_cast<int>(scores.back()) << std::endl;
    std::cout << std::endl;

    std::cout << "Max tile distribution:" << std::endl;
    for (const auto &p : tile_dist) {
        double pct = 100.0 * p.second / n;
        std::cout << "  " << std::setw(5) << p.first << ": "
                  << std::setw(3) << p.second << " ("
                  << std::setw(5) << std::fixed << std::setprecision(1) << pct << "%)"
                  << std::endl;
    }
    std::cout << std::endl;

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Avg moves:     " << total_moves / n << std::endl;
    std::cout << std::setprecision(2);
    std::cout << "Total time:    " << total_time << " s" << std::endl;
    std::cout << "Total nodes:   " << total_nodes << std::endl;
}


int main(int argc, char *argv[]) {
    Options opts = parse_args(argc, argv);

    Bitboards::init();

    if (opts.grad_weight >= 0 || opts.mono_weight >= 0 || opts.mono_power >= 0 ||
        opts.empty_weight >= 0 || opts.merge_weight >= 0) {
        Eval::set_weights(
            opts.grad_weight  >= 0 ? opts.grad_weight  : Eval::GRAD_WEIGHT,
            opts.mono_weight  >= 0 ? opts.mono_weight  : Eval::MONO_WEIGHT,
            opts.mono_power   >= 0 ? opts.mono_power   : Eval::MONO_POWER,
            opts.empty_weight >= 0 ? opts.empty_weight : Eval::EMPTY_WEIGHT,
            opts.merge_weight >= 0 ? opts.merge_weight : Eval::MERGE_WEIGHT
        );
    }

    Search::init();
    if (!opts.tt_enabled) TT::set_enabled(false);
    TT::init(opts.tt_bits);
    int threads = Search::init_pool(opts.num_threads);
    std::cout << "Threads: " << threads << std::endl;

    if (opts.num_games == 1) {
        if (opts.seed >= 0) Bitboards::seed(static_cast<unsigned>(opts.seed));
        GameResult r = play_game(opts.verbose);
        print_single_result(r);
    } else {
        unsigned base_seed = opts.seed >= 0
            ? static_cast<unsigned>(opts.seed)
            : static_cast<unsigned>(
                std::chrono::system_clock::now().time_since_epoch().count());

        std::cout << "=== Benchmark: " << opts.num_games << " games, base seed "
                  << base_seed << " ===" << std::endl << std::endl;

        std::cout << "seed\tmoves\tmax_tile\tscore\ttime_s\tnodes" << std::endl;

        std::vector<GameResult> results;
        results.reserve(opts.num_games);

        for (int i = 0; i < opts.num_games; i++) {
            unsigned seed = base_seed + i;
            Bitboards::seed(seed);
            GameResult r = play_game(false);
            results.push_back(r);

            std::cout << seed << "\t" << r.moves << "\t" << r.max_tile
                      << "\t" << r.score << "\t" << std::fixed
                      << std::setprecision(2) << r.total_time << "\t"
                      << r.nodes << std::endl;
        }

        std::cout << std::endl;
        print_batch_summary(results);
    }
}
