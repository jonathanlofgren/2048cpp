#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include "types.h"

namespace Search {

    void init();
    void init_pool(int num_threads = 0);  // 0 = hardware_concurrency
    void shutdown_pool();

    struct Result {
        Move move;
        float value;
    };

    float evaluate(Bitboard b);
    Result expectimax_parallel(Bitboard board);

    uint64_t get_nodes();
    void reset_nodes();
}

#endif
