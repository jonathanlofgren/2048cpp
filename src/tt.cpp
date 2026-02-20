#include "tt.h"

#include <cstdlib>
#include <cstring>

// Single shared flat hash table, allocated once at first use.
//
// Multiple workers read and write the same table without locks.
// Each TTEntry is 8 bytes (two uint32_t fields), which is atomically
// loadable/storable on x86-64 and ARM64. This eliminates torn-read
// concerns entirely.
//
// We still XOR the key fragment with the data as an extra safety
// layer: on probe we recover the key fragment via `entry.key ^ entry.data`
// and compare it against the expected key fragment. Any corruption or
// collision will cause a mismatch, resulting in a harmless cache miss.
//
// Depth and generation are folded into the full 64-bit key via XOR so
// that: (1) entries from previous moves don't match (O(1) clearing), and
// (2) the same board at different depths doesn't alias. The table index
// is derived from the full 64-bit key, while only the upper 32 bits are
// stored for verification.

static int g_tt_bits = 21;
static int g_tt_size = 1 << 21;

struct TTEntry {
    uint32_t key;   // upper 32 bits of make_key() ^ float_bits(value)
    uint32_t data;  // float_bits(value)
};

static TTEntry* g_table = nullptr;
static uint64_t g_gen_salt = 0;
static bool g_enabled = true;

static inline uint32_t float_to_bits(float v) {
    uint32_t b; std::memcpy(&b, &v, 4); return b;
}

static inline float bits_to_float(uint32_t b) {
    float v; std::memcpy(&v, &b, 4); return v;
}

static inline uint64_t make_key(Bitboard board, int depth) {
    return board ^ g_gen_salt ^ (depth * 0x517CC1B727220A95ULL);
}

static inline uint32_t index(uint64_t key) {
    return static_cast<uint32_t>((key * 0x9E3779B97F4A7C15ULL) >> (64 - g_tt_bits));
}

void TT::init(int bits) {
    std::free(g_table);
    g_tt_bits = bits;
    g_tt_size = 1 << bits;
    g_table = static_cast<TTEntry*>(std::calloc(g_tt_size, sizeof(TTEntry)));
    g_gen_salt = 0;
}

void TT::clear() {
    if (!g_table)
        init();
    g_gen_salt += 0x6C62272E07BB0143ULL;
}

int TT::get_bits() {
    return g_tt_bits;
}

bool TT::probe(Bitboard board, int depth, float& value) {
    if (!g_enabled) return false;
    uint64_t key = make_key(board, depth);
    uint32_t key_hi = static_cast<uint32_t>(key >> 32);
    TTEntry e = g_table[index(key)];
    if ((e.key ^ e.data) == key_hi) {
        value = bits_to_float(e.data);
        return true;
    }
    return false;
}

void TT::store(Bitboard board, int depth, float value) {
    if (!g_enabled) return;
    uint64_t key = make_key(board, depth);
    uint32_t key_hi = static_cast<uint32_t>(key >> 32);
    uint32_t val_bits = float_to_bits(value);
    g_table[index(key)] = {key_hi ^ val_bits, val_bits};
}

void TT::set_enabled(bool enabled) {
    g_enabled = enabled;
}
