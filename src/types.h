#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

#include <cstdint>
#include <vector>

/*
Board Representation:

Col4 Col3 Col2 Col1
 X    X    X    X   Row4
 X    X    X    X   Row3
 X    X    X    X   Row2
 X    X    X    X   Row1

 X = 4 bits (0-15)
 Row = 16 bits (0-32768)
 Square_Value(X) = 2^X (0-32768)

 Bitboard = Row4 Row3 Row2 Row1

*/
typedef uint64_t Bitboard;
typedef std::vector<int> Vector;

enum Row {
    ROW_1, ROW_2, ROW_3, ROW_4, 

    ROW_N = 4
};

enum Col {
    COL_1, COL_2, COL_3, COL_4, 

    COL_N = 4
};

enum Move {
    LEFT, UP, DOWN, RIGHT, 
    MOVE_N = 4,

    NULL_MOVE
};

enum Square {
    SQ_11, SQ_12, SQ_13, SQ_14,
    SQ_21, SQ_22, SQ_23, SQ_24,
    SQ_31, SQ_32, SQ_33, SQ_34,
    SQ_41, SQ_42, SQ_43, SQ_44,

    SQUARE_N = 16
};

struct PossibleMove {
    Move move;
    Bitboard board;
};

const int UNIQUE_ROWS = 65536;
const int SHIFTED_ROWS = UNIQUE_ROWS*ROW_N;
const int SHIFTED_COLS = UNIQUE_ROWS*COL_N;


/*
    Define some needed operators on our types
*/
#define ENABLE_OPERATORS_ON(T)                                  \
inline T operator+(T d1, T d2) { return T(int(d1) + int(d2)); } \
inline T operator-(T d1, T d2) { return T(int(d1) - int(d2)); } \
inline T& operator++(T& d) { return d = T(int(d) + 1); }        \
inline T& operator--(T& d) { return d = T(int(d) - 1); }        \
inline T operator*(int i, T d) { return T(i * int(d)); }        \
inline T operator*(T d, int i) { return T(int(d) * i); }        \
inline T operator-(T d) { return T(-int(d)); }                  \
inline T& operator+=(T& d1, T d2) { return d1 = d1 + d2; }      \
inline T& operator-=(T& d1, T d2) { return d1 = d1 - d2; }      \
inline T& operator*=(T& d, int i) { return d = T(int(d) * i); }

ENABLE_OPERATORS_ON(Square)
ENABLE_OPERATORS_ON(Row)
ENABLE_OPERATORS_ON(Col)
ENABLE_OPERATORS_ON(Move)

#undef ENABLE_OPERATORS_ON

#endif