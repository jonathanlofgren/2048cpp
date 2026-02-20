#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include "types.h"

namespace TT {

void clear();
bool probe(Bitboard board, int depth, float& value);
void store(Bitboard board, int depth, float value);
void set_enabled(bool enabled);

}

#endif
