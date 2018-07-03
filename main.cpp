#include <iostream>
#include <ctime>
#include <cassert>

#include "types.h"
#include "bitboard.h"
#include "search.h"
#include "omp.h"

extern int evaluation_count;
extern int probability_cutoffs;


// Tests symmetry of vector to/from bitboard function
bool test_bitboard_conversion() {
    int num_tests = 10000;

    for (int i = 0; i < num_tests; ++i) {
        Bitboard b = Random::row();
        
        Vector v = Bitboards::bitboard_to_vector(b);
        Bitboard bb = Bitboards::vector_to_bitboard(v);

        if (b != bb) {
            return false;
        }
    }

    return true;
}


bool time_left_right() {
    int num_tests = 10000;

    for (int i = 0; i < num_tests; ++i) {
        Bitboard b = Random::board();

        Bitboard l = move_left(b);
        Bitboard r = move_right(b);
    }

    return true;
}


void run_tests(bool verbose) {
    using namespace std;

    auto pass_string = [](bool p) {
        if (p) 
            return string(" : passed "); 
        else
            return string(" : failed ");
    };

    cout << "Runnings tests....." << std::endl;

    auto start = std::clock();
    bool t1 = test_bitboard_conversion();
    int c1 = (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
    string s1 = "test_bitboard_conversion";
    cout << s1 << pass_string(t1) << "("<< c1 << " ms)" << std::endl;

    start = std::clock();
    bool t5 = time_left_right();
    int c5 = (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
    string s5 = "time_left_right";
    cout << s5 << pass_string(t5) << "("<< c5 << " ms)" << std::endl;
}


void play() {
    Bitboard board = place_random(place_random(0x0ULL));

    auto possible = possible_moves(board);
    int moves = 0;

    double start = std::clock();

    double time_par = 0;
    double time_seq = 0;

    while (possible.size() > 0) {
        moves++;

        //Search for move
        // Search::State state(board);
        // start = omp_get_wtime();
        // Search::Result result_old = Search::expected_value(state);
        // time_seq += (omp_get_wtime() - start);

        start = omp_get_wtime();
        Search::Result result = Search::expectimax_parallel(board);
        time_par += (omp_get_wtime() - start);

        // assert(result.move == result_old.move);
    
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

    std::cout << "Time taken parallel: " << 1000*time_par/moves << " ms/move" <<std::endl;
    // std::cout << "Time taken old: " << 1000*time_seq/moves << " ms/move"  << std::endl;   
}


int main() {
    Bitboards::init();
    Search::init();
    run_tests(true);
    play();
}