#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>

#include "types.h"
#include "bitboard.h"
#include "search.h"

void play(bool verbose) {
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

        // Make move and place a new square
        board = make_move(board, result.move);
        board = place_random(board);

        if (verbose) {
            std::cout << "Move " << moves  << ": " << Bitboards::pretty(result.move) << std::endl;
            std::cout << "Value: " << result.value << std::endl;
            std::cout << Bitboards::pretty(board) << std::endl;
        }

        possible = possible_moves(board);
    }

    uint64_t nodes = Search::get_nodes();

    std::cout << Bitboards::pretty(board) << std::endl;
    std::cout << "Game Over." << std::endl;
    std::cout << "Total moves:  " << moves << std::endl;
    std::cout << "Max tile:     " << max_value(board) << std::endl;
    std::cout << "Final score:  " << board_score(board) << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total time:   " << total_time << " s" << std::endl;
    std::cout << "Avg ms/move:  " << 1000 * total_time / moves << std::endl;
    std::cout << "Min ms/move:  " << min_time << std::endl;
    std::cout << "Max ms/move:  " << max_time << std::endl;
    std::cout << "Nodes:        " << nodes << std::endl;
    std::cout << "Nodes/sec:    " << static_cast<uint64_t>(nodes / total_time) << std::endl;
}


int main(int argc, char *argv[]) {
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
    }

    Bitboards::init();
    Search::init();
    play(verbose);
}
