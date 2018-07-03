#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <map>
#include <chrono>
#include <random>

#include "types.h"


/* 
    Mapping for Squares, Rows and Cols to Bitboard Masks

    Example:
    RowMask[ROW_2] = 0x0000,0000,FFFF,0000
*/
extern Bitboard SquareMask[SQUARE_N];
extern Bitboard ColMask[COL_N];
extern Bitboard RowMask[ROW_N];

extern int SquareOffset[SQUARE_N];
extern int RowOffset[ROW_N];
extern int SquareColNormalize[SQUARE_N];

extern Bitboard RowMoveLeft[UNIQUE_ROWS];
extern Bitboard RowMoveRight[UNIQUE_ROWS];

extern std::map<int, Bitboard> ValueToBits;

// Random generator stuff
extern std::default_random_engine generator;
extern std::uniform_int_distribution<Bitboard> random_board;
extern std::uniform_int_distribution<Bitboard> random_row;


namespace Bitboards {
    void init();
    std::string pretty(Bitboard b);
    std::string pretty(Move m);

    Vector bitboard_to_vector(Bitboard b);
    Bitboard vector_to_bitboard(Vector &v);
    
    Vector move_vector_left(Vector &row);
    Vector move_vector_right(Vector row);
}


namespace Random {
    inline Bitboard board() {return random_board(generator);}
    inline Bitboard row() {return random_row(generator);}
}


Bitboard move_left(Bitboard b);
Bitboard move_right(Bitboard b);
Bitboard move_up(Bitboard b);
Bitboard move_down(Bitboard b);
Bitboard make_move(Bitboard b, Move m);
Bitboard place_random(Bitboard b);

int empty_squares(Bitboard b);
int max_value(Bitboard b);

std::vector<PossibleMove> possible_moves(Bitboard b);
Bitboard row_to_col(Bitboard b, Col c);

inline int bits_to_value(Bitboard s) {
    return 2 << (s - 1);    // if s=0 then s-1=UINT_MAX so this returns 0
}

inline Bitboard value_to_bits(int value) {
    return ValueToBits[value];
}

inline Square make_square(Row r, Col c) {
    return Square(c | r << 2); // multply row by 4 and add c
}


inline Bitboard get_bits(Bitboard b, Row r) {
    return (b & RowMask[r]) >> RowOffset[r];
}


inline Bitboard get_bits(Bitboard b, Square s) {
    return (b & SquareMask[s]) >> SquareOffset[s];
}


inline Bitboard get_bits(Bitboard b, Col c) {
    Bitboard bits = 0x0ULL;

    for (Row r = ROW_1; r <= ROW_4; ++r) {
        Square s = make_square(r, c);
        bits |= (b & SquareMask[s]) >> SquareColNormalize[s];
    }

    return bits;
}


#endif