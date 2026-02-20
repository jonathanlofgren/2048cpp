#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "types.h"

namespace Eval {

// Tunable weights
constexpr float GRAD_WEIGHT  = 5.0f;    // Snake gradient positioning
constexpr float MONO_WEIGHT  = 47.0f;   // Monotonicity violation penalty
constexpr float MONO_POWER   = 4.0f;    // Exponent for monotonicity scaling
constexpr float EMPTY_WEIGHT = 270.0f;  // Empty cell bonus
constexpr float MERGE_WEIGHT = 700.0f;  // Adjacent-equal-tile bonus

void init();
float evaluate(Bitboard board);

}

#endif
