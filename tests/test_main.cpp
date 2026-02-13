#include <iostream>
#include <string>
#include <cassert>
#include <cmath>
#include <vector>

#include "types.h"
#include "bitboard.h"
#include "search.h"

// Simple test framework: run individual tests by name from the command line.
// Returns 0 on pass, 1 on failure.

bool test_bitboard_conversion() {
    const int num_tests = 10000;

    for (int i = 0; i < num_tests; ++i) {
        Bitboard b = Random::row();
        Vector v = Bitboards::bitboard_to_vector(b);
        Bitboard bb = Bitboards::vector_to_bitboard(v);

        if (b != bb) {
            std::cerr << "Round-trip failed for bitboard: " << b
                      << " (got " << bb << ")" << std::endl;
            return false;
        }
    }

    return true;
}

bool test_move_left_right() {
    const int num_tests = 10000;

    for (int i = 0; i < num_tests; ++i) {
        Bitboard b = Random::board();

        // Moves should not crash and should produce valid boards
        Bitboard l = move_left(b);
        Bitboard r = move_right(b);
        (void)l;
        (void)r;
    }

    // Two 2-tiles in row 1: bits {1,1,0,0} = 0x0011
    // The vector representation reverses column order, so "left" in the game
    // shifts toward the high nibble side.
    Bitboard two_twos = 0x0011ULL;
    Bitboard left = move_left(two_twos);
    Bitboard right = move_right(two_twos);

    // Left and right should produce different results (tiles merge in opposite corners)
    if (left == right) {
        std::cerr << "move_left and move_right should differ for 0x0011" << std::endl;
        return false;
    }

    // Both results should have exactly one non-zero nibble (the merged 4-tile)
    if (empty_squares(left) != 15 || empty_squares(right) != 15) {
        std::cerr << "Merged result should have exactly 1 tile" << std::endl;
        return false;
    }

    // The merged tile should have value 4 (bits = 2)
    if (max_value(left) != 4 || max_value(right) != 4) {
        std::cerr << "Merged tile should have value 4" << std::endl;
        return false;
    }

    return true;
}

bool test_move_consistency() {
    // After a move, the number of non-empty tiles should not increase
    // (merges can only reduce or maintain tile count)
    const int num_tests = 1000;

    for (int i = 0; i < num_tests; ++i) {
        Bitboard b = Random::board();
        int original_tiles = 16 - empty_squares(b);

        for (Move m = LEFT; m <= RIGHT; ++m) {
            Bitboard after = make_move(b, m);
            int after_tiles = 16 - empty_squares(after);

            if (after_tiles > original_tiles) {
                std::cerr << "Move " << Bitboards::pretty(m)
                          << " increased tile count from " << original_tiles
                          << " to " << after_tiles << std::endl;
                return false;
            }
        }
    }

    // Opposite moves on a single-tile board should not change the tile's value
    Bitboard single = 0x3ULL; // A single 8-tile at SQ_11
    for (Move m = LEFT; m <= RIGHT; ++m) {
        Bitboard after = make_move(single, m);
        if (max_value(after) != max_value(single)) {
            std::cerr << "Single-tile move changed the tile value" << std::endl;
            return false;
        }
    }

    return true;
}

bool test_empty_squares() {
    // Empty board should have 16 empty squares
    if (empty_squares(0x0ULL) != 16) {
        std::cerr << "Empty board should have 16 empty squares" << std::endl;
        return false;
    }

    // Full board (all 1s in each nibble) should have 0 empty squares
    Bitboard full = 0x1111111111111111ULL;
    if (empty_squares(full) != 0) {
        std::cerr << "Full board should have 0 empty squares" << std::endl;
        return false;
    }

    // Board with one tile should have 15 empty squares
    Bitboard one_tile = 0x1ULL;
    if (empty_squares(one_tile) != 15) {
        std::cerr << "Single-tile board should have 15 empty squares" << std::endl;
        return false;
    }

    return true;
}

bool test_place_random() {
    // Placing on empty board should result in exactly 15 empty squares
    Bitboard b = place_random(0x0ULL);
    if (empty_squares(b) != 15) {
        std::cerr << "place_random on empty board should leave 15 empty squares, got "
                  << empty_squares(b) << std::endl;
        return false;
    }

    // Placing again should result in 14 empty squares
    b = place_random(b);
    if (empty_squares(b) != 14) {
        std::cerr << "Second place_random should leave 14 empty squares, got "
                  << empty_squares(b) << std::endl;
        return false;
    }

    // Place random should never modify existing tiles
    const int num_tests = 1000;
    for (int i = 0; i < num_tests; ++i) {
        Bitboard before = place_random(place_random(0x0ULL));
        Bitboard after = place_random(before);

        // All non-zero nibbles in 'before' should be identical in 'after'
        for (Square s = SQ_11; s <= SQ_44; ++s) {
            Bitboard bits_before = get_bits(before, s);
            if (bits_before != 0) {
                Bitboard bits_after = get_bits(after, s);
                if (bits_before != bits_after) {
                    std::cerr << "place_random modified existing tile at square "
                              << s << std::endl;
                    return false;
                }
            }
        }
    }

    return true;
}

bool test_board_score() {
    // Empty board should have score 0
    if (board_score(0x0ULL) != 0) {
        std::cerr << "Empty board should have score 0, got " << board_score(0x0ULL) << std::endl;
        return false;
    }

    // Board with a single 2-tile (bits=1) at SQ_11
    Bitboard one_two = 0x1ULL;
    if (board_score(one_two) != 2) {
        std::cerr << "Single 2-tile board should have score 2, got " << board_score(one_two) << std::endl;
        return false;
    }

    // Board with 2-tiles in every square (bits=1 in each nibble)
    Bitboard all_twos = 0x1111111111111111ULL;
    if (board_score(all_twos) != 32) {
        std::cerr << "All-twos board should have score 32, got " << board_score(all_twos) << std::endl;
        return false;
    }

    // Board with a single 1024-tile (bits=10) at SQ_11 and a 4-tile (bits=2) at SQ_12
    Bitboard mixed = 0x2AULL;
    if (board_score(mixed) != 1028) {
        std::cerr << "Mixed board should have score 1028, got " << board_score(mixed) << std::endl;
        return false;
    }

    return true;
}

bool test_node_counter() {
    Search::reset_nodes();
    if (Search::get_nodes() != 0) {
        std::cerr << "Node counter should be 0 after reset, got " << Search::get_nodes() << std::endl;
        return false;
    }

    // Run evaluate once — should increment
    Search::evaluate(0x0ULL);
    if (Search::get_nodes() == 0) {
        std::cerr << "Node counter should be > 0 after evaluate()" << std::endl;
        return false;
    }

    // Reset and verify back to 0
    Search::reset_nodes();
    if (Search::get_nodes() != 0) {
        std::cerr << "Node counter should be 0 after second reset" << std::endl;
        return false;
    }

    return true;
}

bool test_evaluation() {
    // Evaluation of empty board should be 0 (all tiles are 0)
    double val = Search::evaluate(0x0ULL);
    if (val != 0.0) {
        std::cerr << "Empty board should evaluate to 0, got " << val << std::endl;
        return false;
    }

    // Board with higher tiles should evaluate higher than lower tiles
    // Board with a single 2 (bits=1) at SQ_11 vs SQ_44
    Bitboard high_corner = 0x1ULL;                    // SQ_11: high weight
    Bitboard low_corner = 0x1000000000000000ULL;      // SQ_44: low weight

    double val_high = Search::evaluate(high_corner);
    double val_low = Search::evaluate(low_corner);

    if (val_high <= val_low) {
        std::cerr << "Tile at high-weight corner (" << val_high
                  << ") should evaluate higher than low-weight corner ("
                  << val_low << ")" << std::endl;
        return false;
    }

    return true;
}

struct TestCase {
    std::string name;
    bool (*func)();
};

int main(int argc, char *argv[]) {
    Bitboards::init();
    Search::init();

    std::vector<TestCase> tests = {
        {"bitboard_conversion", test_bitboard_conversion},
        {"move_left_right", test_move_left_right},
        {"move_consistency", test_move_consistency},
        {"empty_squares", test_empty_squares},
        {"place_random", test_place_random},
        {"evaluation", test_evaluation},
        {"board_score", test_board_score},
        {"node_counter", test_node_counter},
    };

    // If a test name is given, run only that test
    if (argc > 1) {
        std::string requested = argv[1];
        for (const auto &tc : tests) {
            if (tc.name == requested) {
                bool passed = tc.func();
                std::cout << tc.name << ": " << (passed ? "PASSED" : "FAILED")
                          << std::endl;
                return passed ? 0 : 1;
            }
        }
        std::cerr << "Unknown test: " << requested << std::endl;
        return 1;
    }

    // Otherwise, run all tests
    int failures = 0;
    for (const auto &tc : tests) {
        bool passed = tc.func();
        std::cout << tc.name << ": " << (passed ? "PASSED" : "FAILED")
                  << std::endl;
        if (!passed) failures++;
    }

    return failures == 0 ? 0 : 1;
}
