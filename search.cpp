#include "search.h"

#include <vector>
#include <algorithm>
#include "types.h"
#include "bitboard.h"
#include <utility>
#include <limits>

const double DiagLinGrad[SQUARE_N] = {1.00, 0.83, 0.66, 0.50,
						       		  0.84, 0.67, 0.51, 0.33,
						        	  0.68, 0.52, 0.34, 0.17,
						        	  0.53, 0.35, 0.18, 0.00};

const double StepGrad[SQUARE_N] = {15, 14 ,13 , 12,
								    8,  9, 10,  11,
								    7,  6,  5,   4,
									0,  1,  2,   3};

double RowValue[SHIFTED_ROWS];

const int MAX_DEPTH = 4;
const double PROBABILITY_CUTOFF = 0.0001;

int evaluation_count = 0;
int probability_cutoffs = 0;

using namespace Search;

void Search::init() {

	for (Bitboard b = 0x0ULL; b < UNIQUE_ROWS; ++b) {
		int empty = empty_squares(b) - 12;
		double empty_score = 1.0 + 1.0*empty/100;

		// set up row values
		for (Row r = ROW_1; r <= ROW_4; ++r) {
			Bitboard row = b << RowOffset[r];

			double value = 0;
			for (Square s = SQ_11; s <= SQ_44; ++s)
				value += DiagLinGrad[s]*DiagLinGrad[s]*bits_to_value(get_bits(row, s)) * empty_score;

			RowValue[UNIQUE_ROWS*r+b] = value;
		}
	}
}

double gradient_value_map(Bitboard board) {
	double value = 0;
	for (Row r = ROW_1; r <= ROW_4; ++r) {
		value += RowValue[UNIQUE_ROWS*r+get_bits(board, r)];
	}

	return value;
}

std::vector<Expansion> Search::expand_board(Bitboard b) {
	std::vector<Expansion> expanded;
	expanded.reserve(32); // Doing this reserve halves the running time!

	for (Square s = SQ_11; s <= SQ_44; ++s) {
		if (!(b & SquareMask[s])) {	
			Expansion exp2(b | (0x1ULL << SquareOffset[s]), 0.9); 	// Set a 2 in the empty square (probability 0.9)
			Expansion exp4(b | (0x2ULL << SquareOffset[s]), 0.1);	// Set a 4 in the empty square (probability 0.1)

			expanded.push_back(exp2);
			expanded.push_back(exp4);
		}
	}

	return expanded;
}


void expand_inplace(Bitboard b, Bitboard *expanded) {
	int i = 0;
	for (Square s = SQ_11; s <= SQ_44; ++s) {
		if (!(b & SquareMask[s])) {	
			expanded[i++] = b | (0x1ULL << SquareOffset[s]); 	// Set a 2 in the empty square (probability 0.9)
			expanded[i++] = b | (0x2ULL << SquareOffset[s]);	// Set a 4 in the empty square (probability 0.1)
		}
	}
	expanded[i] = 0;	// make sure to set zero to indicate end
	expanded[31] = i; 	// indicate how many were empty
}

double gradient_value(Bitboard b) {
	double sum = 0;

	for (Square s = SQ_11; s <= SQ_44; ++s) {
		sum += DiagLinGrad[s]*DiagLinGrad[s]*bits_to_value(get_bits(b, s));
	}

	return sum;
}


double Search::evaluate(Bitboard b) {
	return gradient_value_map(b);
}


double Search::_value_expected_node(Bitboard board, int depth, double prob) {	
	Bitboard expanded[32];
	expand_inplace(board, expanded);

	double expected_value = 0;
	double prob_sum = (double)expanded[31]/2.0;
	double prob2 = 0.9/prob_sum;
	double prob4 = 0.1/prob_sum;

	Bitboard *curr = expanded;
	while (*curr) {
		expected_value += prob2*_value_max_node(*(curr++), depth+1, prob*prob2) + 
						  prob4*_value_max_node(*(curr++), depth+1, prob*prob4);
	}

	return expected_value;
}


double Search::_value_max_node(Bitboard board, int depth, double prob) {
	if (depth >= MAX_DEPTH || prob < PROBABILITY_CUTOFF)
		return evaluate(board);

	auto possible = possible_moves(board);

	if (possible.size() == 0) {
		return 0.0;
	}

	double max = std::numeric_limits<double>::lowest();

	for (auto it = possible.begin(); it != possible.end(); ++it) {
		double val = _value_expected_node(it->board, depth, prob);
		if (val > max) max = val;
	}

	return max;
}

Result Search::expectimax(Bitboard board) {
	auto possible = possible_moves(board);

	if (possible.size() == 0) {
		return {NULL_MOVE, 0};
	}

	std::map<Move, double> move_values;

	for (const PossibleMove & pm: possible) {
		move_values[pm.move] = _value_expected_node(pm.board, 0, 1);
	}

	auto max = std::max_element(move_values.begin(), move_values.end(),
    		[](const std::pair<Move, double>& p1, const std::pair<Move, double>& p2) {
        		return p1.second < p2.second; });

	return {max->first, max->second};
}

Result Search::expectimax_parallel(Bitboard board) {
	auto possible = possible_moves(board);
	int n = possible.size();

	if (n == 0) {
		return {NULL_MOVE, 0};
	}

	double values[MOVE_N];
	#pragma omp parallel for num_threads(n)
	for (int i = 0; i < n; i++) {
		values[possible[i].move] = _value_expected_node(possible[i].board, 0, 1);
	}

	Move best_move = NULL_MOVE;
	double max_value = std::numeric_limits<double>::lowest();

	for (const PossibleMove & pm: possible) {
		if (values[pm.move] > max_value) {
			best_move = pm.move;
			max_value = values[pm.move];
		}
	}

	return {best_move, max_value};
}


Result Search::expected_value(State & st) {
	// First base case: we reached depth
	if (st.depth == MAX_DEPTH) {
		return {NULL_MOVE, evaluate(st.board)};
	}

	// Second base case: we are below probablity cutoff
	if (st.prob < PROBABILITY_CUTOFF) {
		++probability_cutoffs;
		return {NULL_MOVE, evaluate(st.board)};
	}

	auto possible = possible_moves(st.board);

	// Third base case: there are no possible moves
	if (possible.size() ==  0) {
		return {NULL_MOVE, 0};
	}


	std::map<Move, double> move_values;
	double value = 0;
	double prob_sum = 0;
	double prob = 0;

	for (const PossibleMove pm: possible) { // For each possible move we want to find the expected value
		value = 0;

		// Expand the board fully to all possible states
		std::vector<Expansion> expanded = expand_board(pm.board);
		prob_sum = (double)expanded.size() / 2.0;

		for (const Expansion exp: expanded) {
			prob = exp.prob/prob_sum;
			State next_state(exp.board, st.depth+1, st.prob*prob);

			Result r = expected_value(next_state);
			value += prob*r.value;
		}

		move_values[pm.move] = value;
	}

	auto max = std::max_element(move_values.begin(), move_values.end(),
    		[](const std::pair<Move, double>& p1, const std::pair<Move, double>& p2) {
        		return p1.second < p2.second; });

	return {max->first, max->second};
}
