#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include "types.h"

namespace Search {

	void init();

	struct State {
		Bitboard board;
		unsigned int depth;
		double prob;

		State(Bitboard b, unsigned int d, double p): board(b), depth(d), prob(p) {};
		State(Bitboard b): board(b), depth(0), prob(1) {};
	};

	struct Result {
		Move move;
		double value;
	};

	struct Expansion {
		Bitboard board;
		double prob;

		Expansion(Bitboard b, double p): board(b), prob(p) {}
	};

	std::vector<Expansion> expand_board(Bitboard b);
	Result expected_value(State & st);
	double evaluate(Bitboard b);

	// second try to split the search function in two
	double _value_expected_node(Bitboard board, int depth, double prob);
	double _value_max_node(Bitboard board, int depth, double prob);
	Result expectimax(Bitboard board);
	Result expectimax_parallel(Bitboard board);
}

#endif