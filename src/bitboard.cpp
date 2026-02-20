#include "bitboard.h"

#include <sstream>
#include <iomanip>
#include <algorithm>

Bitboard SquareMask[SQUARE_N];
Bitboard ColMask[COL_N];
Bitboard RowMask[ROW_N];

int SquareOffset[SQUARE_N];
int RowOffset[ROW_N];
// This array contains the offset needed to shift a square
// so that its position in the first row is the same as 
// its position in its column.
int SquareColNormalize[SQUARE_N] = {
     0,  4,  8, 12,
    12, 16, 20, 24,
    24, 28, 32, 36,
    36, 40, 44, 48
};


Bitboard RowMoveLeft[UNIQUE_ROWS];
Bitboard RowMoveRight[UNIQUE_ROWS];

std::map<int, Bitboard> ValueToBits;

std::mt19937 generator;
std::uniform_int_distribution<Bitboard> random_board;
std::uniform_int_distribution<Bitboard> random_row;


void Bitboards::seed(unsigned s) {
    generator = std::mt19937(s);
}

void Bitboards::init() {
    seed(std::chrono::system_clock::now().time_since_epoch().count());
    random_board = std::uniform_int_distribution<Bitboard>(0, UINT64_MAX);
    random_row = std::uniform_int_distribution<Bitboard>(0, UNIQUE_ROWS-1);

    int value;
    for (Square s = SQ_11; s <= SQ_44; ++s) {
        SquareOffset[s] = s * 4;
        SquareMask[s] = 0xFULL << SquareOffset[s];
        
        if (s == 0)
            value = 0;
        else
            value = 2 << (s-1);

        ValueToBits[value] = (Bitboard) s;
    }

    for (Col c = COL_1; c <= COL_4; ++c)
        ColMask[c] = 0x000F000F000F000FULL << (c * 4);

    for (Row r = ROW_1; r <= ROW_4; ++r) {
        RowOffset[r] = r * 16;
        RowMask[r] = 0xFFFFULL << RowOffset[r];
    }

    for (Bitboard b = 0x0ULL; b < UNIQUE_ROWS; ++b) {
        Vector v = bitboard_to_vector(b);

        Vector vl = move_vector_left(v);
        Vector vr = move_vector_right(v);

        RowMoveLeft[b] = vector_to_bitboard(vl);
        RowMoveRight[b] = vector_to_bitboard(vr);
    }

}


/*
    Generate a nice string representation of a board,
    ready to be printed to output.
*/
std::string Bitboards::pretty(Bitboard b) {
    std::stringstream ss;
    std::string row_delim = "|-------+-------+-------+-------|\n";
    ss << row_delim;

    for (Row r = ROW_4; r >= ROW_1; --r) {
        ss << "| ";

        for (Col c = COL_4; c >= COL_1; --c) {
            Square s = make_square(r, c);
            ss << std::setw(5) << bits_to_value(get_bits(b, s)) << " | ";
        }
        ss << std::endl << row_delim;
    }

    return ss.str();
}

std::string Bitboards::pretty(Move m) {
    switch (m) {
        case LEFT:
            return "Left";
        case UP:
            return "Up";
        case DOWN:
            return "Down";
        case RIGHT:
            return "Right";
        default: 
            return "Invalid Move";
    }
} 


Vector Bitboards::bitboard_to_vector(Bitboard b) {
    Vector vec{0, 0, 0, 0};

    for (Square s = SQ_11; s <= SQ_14; ++s)
        vec[s] = bits_to_value(get_bits(b, s));

    std::reverse(vec.begin(), vec.end()); // we put them in in reverse order
    return vec;
}


Bitboard Bitboards::vector_to_bitboard(Vector &v) {
    Bitboard b = 0x0ULL;
    Square s = SQ_11;

    for (auto i = v.rbegin(); i != v.rend(); ++i) {
        b |= value_to_bits(*i) << SquareOffset[s];
        ++s;
    }

    return b;
}


/*
    Naive method for moving a vector row left.
    Only used to initialize the Bitboard move function.
    
    Ex: {2,0,8,8} -> {2,16,0,0}
        {4,4,4,4} -> {8,8,0,0}
*/
Vector Bitboards::move_vector_left(Vector &row) {
    int l = row.size();

    Vector new_row(l, 0);

    int pos = 1;
    bool added = false;

    for (int i = 0; i < l; ++i) {
        if (row[i] != 0) {
            if (new_row[pos-1] == 0) {   // just shift it to the left
                new_row[pos-1] = row[i];
                added = false;
            }
            else if (new_row[pos-1] == row[i] && !added) { // merge it
                new_row[pos-1] += row[i];
                added = true;
                pos++;
            }
            else {
                new_row[pos] = row[i];
                added = false;
                pos++;
            }
        }
    }

    return new_row;
}


Vector Bitboards::move_vector_right(Vector row) {
    std::reverse(row.begin(), row.end());
    Vector new_row = move_vector_left(row);
    std::reverse(new_row.begin(), new_row.end());

    return new_row;
}


int empty_squares(Bitboard b) {
    int count = 0;

    for (Square s = SQ_11; s <= SQ_44; ++s) {
        if (!(b & SquareMask[s])) ++count;
    }

    return count;
}


std::vector<Square> get_empty_squares(Bitboard b) {
    std::vector<Square> empty;

    for (Square s = SQ_11; s <= SQ_44; ++s) {
        if (!(b & SquareMask[s])) empty.push_back(s);
    }

    return empty;
}


int max_value(Bitboard b) {
    int max = 0;
    int value;

    for (Square s = SQ_11; s <= SQ_44; ++s) {
        value = bits_to_value(get_bits(b, s));
        max = std::max(max, value);
    }

    return max;
}


int board_score(Bitboard b) {
    int score = 0;
    for (Square s = SQ_11; s <= SQ_44; ++s)
        score += bits_to_value(get_bits(b, s));
    return score;
}


Bitboard place_random(Bitboard b) {
    auto empty = get_empty_squares(b);

    if (empty.size() == 0)
        return b;

    std::uniform_int_distribution<Bitboard> random_pos(0, empty.size()-1);
    std::uniform_real_distribution<double> rand(0, 1);

    // get a random position and value for new square
    int pos = random_pos(generator);
    Bitboard value = (rand(generator) < 0.1) ? 2 : 1;
    
    return (b | (value << SquareOffset[empty[pos]]));
}



Bitboard row_to_col(Bitboard b, Col c) {
    Bitboard bits = 0x0ULL;
    Square pos = SQ_11;

    for (Row r = ROW_1; r <= ROW_4; ++r) {
        Square dest = make_square(r, c);
        bits |= (b & SquareMask[pos]) << SquareColNormalize[dest];
        ++pos;
    }

    return bits;
}


Bitboard move_left(Bitboard b) {
    return RowMoveLeft[(b >>  0) & 0xFFFF]
         | RowMoveLeft[(b >> 16) & 0xFFFF] << 16
         | RowMoveLeft[(b >> 32) & 0xFFFF] << 32
         | RowMoveLeft[(b >> 48) & 0xFFFF] << 48;
}

Bitboard move_right(Bitboard b) {
    return RowMoveRight[(b >>  0) & 0xFFFF]
         | RowMoveRight[(b >> 16) & 0xFFFF] << 16
         | RowMoveRight[(b >> 32) & 0xFFFF] << 32
         | RowMoveRight[(b >> 48) & 0xFFFF] << 48;
}


Bitboard move_up(Bitboard b) {
    return transpose(move_left(transpose(b)));
}

Bitboard move_down(Bitboard b) {
    return transpose(move_right(transpose(b)));
}

Bitboard make_move(Bitboard b, Move m) {
    switch (m) {
        case LEFT:
            return move_left(b);
        case UP:
            return move_up(b);
        case DOWN:
            return move_down(b);
        case RIGHT:
            return move_right(b);
        default:
            return b;
    }
}



PossibleMoves possible_moves(Bitboard b) {
    PossibleMoves moves;
    Bitboard t = transpose(b);

    Bitboard bm;
    bm = move_left(b);  if (bm != b) moves.push(LEFT, bm);
    bm = move_right(b); if (bm != b) moves.push(RIGHT, bm);
    bm = transpose(move_left(t));  if (bm != b) moves.push(UP, bm);
    bm = transpose(move_right(t)); if (bm != b) moves.push(DOWN, bm);

    return moves;
}