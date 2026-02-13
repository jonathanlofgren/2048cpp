#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include "types.h"

namespace Search {

    void init();

    struct Result {
        Move move;
        double value;
    };

    double evaluate(Bitboard b);
    double _value_expected_node(Bitboard board, int depth, double prob);
    double _value_max_node(Bitboard board, int depth, double prob);
    Result expectimax_parallel(Bitboard board);
}

#endif