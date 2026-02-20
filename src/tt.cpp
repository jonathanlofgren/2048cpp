#include "tt.h"

#include <cstdlib>
#include <cstring>

// Single shared flat hash table, allocated once at first use.
//
// Lockless concurrent access: async workers read/write without locks.
// To detect torn reads (key from one write, value from another), the stored
// key is XORed with the value bits — same technique as Stockfish. A torn
// read produces a key mismatch with overwhelming probability.
//
// Depth and generation are folded into the key via XOR so that:
// (1) entries from previous moves don't match (O(1) clearing), and
// (2) the same board at different depths doesn't alias.

static constexpr int TT_BITS = 20;
static constexpr int TT_SIZE = 1 << TT_BITS;

struct TTEntry {
    uint64_t key;   // tt_key ^ double_bits(value)
    uint64_t data;  // double_bits(value)
};

static TTEntry* g_table = nullptr;
static uint64_t g_gen_salt = 0;
static bool g_enabled = true;

static inline uint64_t double_to_bits(double v) {
    uint64_t b; std::memcpy(&b, &v, 8); return b;
}

static inline double bits_to_double(uint64_t b) {
    double v; std::memcpy(&v, &b, 8); return v;
}

static inline uint64_t make_key(Bitboard board, int depth) {
    return board ^ g_gen_salt ^ (depth * 0x517CC1B727220A95ULL);
}

static inline uint32_t index(uint64_t key) {
    return static_cast<uint32_t>((key * 0x9E3779B97F4A7C15ULL) >> (64 - TT_BITS));
}

void TT::clear() {
    if (!g_table)
        g_table = static_cast<TTEntry*>(std::calloc(TT_SIZE, sizeof(TTEntry)));
    g_gen_salt += 0x6C62272E07BB0142ULL;
}

bool TT::probe(Bitboard board, int depth, double& value) {
    if (!g_enabled) return false;
    uint64_t key = make_key(board, depth);
    TTEntry e = g_table[index(key)];
    if ((e.key ^ e.data) == key) {
        value = bits_to_double(e.data);
        return true;
    }
    return false;
}

void TT::store(Bitboard board, int depth, double value) {
    if (!g_enabled) return;
    uint64_t key = make_key(board, depth);
    uint64_t val_bits = double_to_bits(value);
    g_table[index(key)] = {key ^ val_bits, val_bits};
}

void TT::set_enabled(bool enabled) {
    g_enabled = enabled;
}
