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
    Result expectimax_parallel(Bitboard board);

    uint64_t get_nodes();
    void reset_nodes();
}

#endif
