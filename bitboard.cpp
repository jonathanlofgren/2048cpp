#include "bitboard.h"

#include <sstream> 
#include <iomanip>
#include <algorithm>

#include <iostream>

Bitboard SquareMask[SQUARE_N];
Bitboard ColMask[COL_N];
Bitboard RowMask[ROW_N];

int SquareOffset[SQUARE_N];
int RowOffset[ROW_N];
// This array contains the offset needed to shift a square
// so that its position in the first row is the same as 
// its position in its column.
int SquareColNormalize[SQUARE_N] = { 0,  4,  8, 12,
									12, 16, 20, 24,
									24, 28, 32, 36,
									36, 40, 44, 48};


Bitboard RowToCol[SHIFTED_COLS];


Bitboard MoveLeftMap[SHIFTED_ROWS];
Bitboard MoveRightMap[SHIFTED_ROWS];
Bitboard MoveUpMap[SHIFTED_COLS];
Bitboard MoveDownMap[SHIFTED_COLS];

std::map<int, Bitboard> ValueToBits;

std::default_random_engine generator;
std::uniform_int_distribution<Bitboard> random_board;
std::uniform_int_distribution<Bitboard> random_row;


void Bitboards::init() {
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	
	generator = std::default_random_engine(seed);
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

		Bitboard bl = vector_to_bitboard(vl);
		Bitboard br = vector_to_bitboard(vr);

		for (Row r = ROW_1; r <= ROW_4; ++r) {
			MoveLeftMap[UNIQUE_ROWS*r+b] = bl << RowOffset[r];
			MoveRightMap[UNIQUE_ROWS*r+b] = br << RowOffset[r];
		}

		for (Col c = COL_1; c <= COL_4; ++c) {
			RowToCol[UNIQUE_ROWS*c+b] = row_to_col(b, c);
			MoveUpMap[UNIQUE_ROWS*c+b] = row_to_col(bl, c);
			MoveDownMap[UNIQUE_ROWS*c+b] = row_to_col(br, c);
		}

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
	Vector vec{0,0,0,0};

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
	Bitboard left = 0x0ULL;

	for (Row r = ROW_1; r <= ROW_4; ++r) {
		Bitboard row = get_bits(b, r);
		left |= MoveLeftMap[UNIQUE_ROWS*r+row];
	}

	return left;
}


Bitboard move_right(Bitboard b) {
	Bitboard right = 0x0ULL;

	for (Row r = ROW_1; r <= ROW_4; ++r) {
		Bitboard row = get_bits(b, r);
		right |= MoveRightMap[UNIQUE_ROWS*r+row];
	}

	return right;
}


Bitboard move_up(Bitboard b) {
	Bitboard up = 0x0ULL;

	for (Col c = COL_1; c <= COL_4; ++c) {
		Bitboard col = get_bits(b, c);
		up |= MoveUpMap[UNIQUE_ROWS*c+col];
	}

	return up;
}

Bitboard move_down(Bitboard b) {
	Bitboard down = 0x0ULL;

	for (Col c = COL_1; c <= COL_4; ++c) {
		Bitboard col = get_bits(b, c);
		down |= MoveDownMap[UNIQUE_ROWS*c+col];
	}

	return down;
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
	}
}



std::vector<PossibleMove> possible_moves(Bitboard b) {
	std::vector<PossibleMove> moves;
	moves.reserve(4);

	for (Move m = LEFT; m <= RIGHT; ++m) {
		Bitboard bm = make_move(b, m);

		if (b != bm)
			moves.push_back({m, bm});
	}

	return moves;
}