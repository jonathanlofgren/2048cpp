#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include "types.h"

namespace TT {

void init(int bits = 21);  // default 2M entries (16 MB)
void clear();
bool probe(Bitboard board, int depth, float& value);
void store(Bitboard board, int depth, float value);
void set_enabled(bool enabled);
int get_bits();

}

#endif
