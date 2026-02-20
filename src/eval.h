#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "types.h"

namespace Eval {

// Weights for evaluation components
constexpr double GRAD_WEIGHT   = 5.0;
constexpr double MONO_WEIGHT   = 1.0;
constexpr double SMOOTH_WEIGHT = 0.1;
constexpr double EMPTY_WEIGHT  = 100.0;
constexpr double MERGE_WEIGHT  = 10.0;

// Pre-compute RowEval/ColEval lookup tables. Must be called once at startup.
void init();

// O(1) board evaluation via table lookup (4 row + 4 col lookups).
float evaluate(Bitboard board);

}

#endif
