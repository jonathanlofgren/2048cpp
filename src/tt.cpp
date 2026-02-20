#include "tt.h"

#include <cstdlib>
#include <cstring>

// Single shared flat hash table, allocated once at first use.
//
// Multiple async workers read and write the same table without locks.
// A TTEntry has two 8-byte fields (key, data). On x86-64 each 8-byte
// aligned access is atomic, but writing the pair is NOT atomic — another
// thread can read between the two stores. This is a "torn read":
//
//   Thread A writes entry:  key = K_a              (first store)
//   Thread B writes entry:  key = K_b, data = D_b  (overwrites both)
//   Thread A writes entry:  data = D_a             (second store)
//   Result in memory:       {K_b, D_a}             (half from each thread)
//
// A reader now sees K_b with D_a — a valid-looking entry with the wrong
// value. If it only checked `entry.key == lookup_key`, it would return
// D_a for a lookup of K_b, silently corrupting the search.
//
// Fix (from Stockfish): store `key XOR data` instead of `key` alone.
// On probe, recover the key as `entry.key XOR entry.data`. A torn read
// produces `K_b XOR D_a`, which won't equal any valid lookup key. The
// probe misses and we just recompute — no wrong values, only lost cache
// hits.
//
// Depth and generation are folded into the key via XOR so that:
// (1) entries from previous moves don't match (O(1) clearing), and
// (2) the same board at different depths doesn't alias.

static constexpr int TT_BITS = 20;
static constexpr int TT_SIZE = 1 << TT_BITS;

struct TTEntry {
    uint64_t key;   // make_key() ^ double_bits(value)  (XOR'd for torn-read safety)
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
    // Recover original key: (key ^ data) ^ data == key. Torn reads fail here.
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
    g_table[index(key)] = {key ^ val_bits, val_bits};  // XOR key so probe can detect torn reads
}

void TT::set_enabled(bool enabled) {
    g_enabled = enabled;
}
