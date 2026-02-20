#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "types.h"

namespace Eval {

// Tunable weights (settable at runtime before init())
extern float GRAD_WEIGHT;    // Snake gradient positioning
extern float MONO_WEIGHT;    // Monotonicity violation penalty
extern float MONO_POWER;     // Exponent for monotonicity scaling
extern float EMPTY_WEIGHT;   // Empty cell bonus
extern float MERGE_WEIGHT;   // Adjacent-equal-tile bonus

void set_weights(float grad, float mono, float mono_power, float empty, float merge);
void init();
float evaluate(Bitboard board);

}

#endif
