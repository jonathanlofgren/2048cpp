#include <iostream>

#include "types.h"
#include "bitboard.h"
#include "search.h"
#include <omp.h>

void play() {
    Bitboard board = place_random(place_random(0x0ULL));

    auto possible = possible_moves(board);
    int moves = 0;

    double time_par = 0;

    while (possible.size() > 0) {
        moves++;

        double start = omp_get_wtime();
        Search::Result result = Search::expectimax_parallel(board);
        time_par += (omp_get_wtime() - start);

        // Make move and place a new square
        board = make_move(board, result.move);
        board = place_random(board);

        // Show board
        std::cout << "Move " << moves  << ": " << Bitboards::pretty(result.move) << std::endl;
        std::cout << "Value: " << result.value << std::endl;
        std::cout << Bitboards::pretty(board) << std::endl;

        possible = possible_moves(board);
    }

    std::cout << "Game Over." << std::endl;
    std::cout << "Max: " <<  max_value(board) << std::endl;

    std::cout << "Time taken parallel: " << 1000*time_par/moves << " ms/move" << std::endl;
}


int main() {
    Bitboards::init();
    Search::init();
    play();
}