#include "tt.h"

#include <cstdlib>
#include <cstring>

// Single shared flat hash table, allocated once at first use.
//
// Multiple workers read and write the same table without locks.
// Each TTEntry is 8 bytes (two uint32_t fields), which is atomically
// loadable/storable on x86-64 and ARM64, so torn reads cannot occur.
//
// The XOR scheme (`entry.key ^ entry.data`) exists for hash collision
// detection: with only 21 index bits, many different boards map to the
// same slot. On probe we recover a 28-bit key fragment and compare it
// against the expected one. A mismatch (whether from a hash collision
// or a concurrent overwrite) results in a harmless cache miss.
//
// Generation salt is folded into the 64-bit key so entries from previous
// moves don't match (O(1) clearing). Depth is NOT part of the key — the
// same board at any search depth maps to the same slot. Instead, depth
// is packed into the upper 4 bits of the key field (outside the XOR
// verification region, which uses the lower 28 bits).
//
// Probe returns a hit when stored_depth <= query_depth, meaning the
// cached result was computed with at least as much remaining lookahead.
// Store uses depth-preferred replacement: an entry is only overwritten
// if it belongs to a different board (or is stale) or the new result
// has at least as much lookahead (new_depth <= stored_depth).

static int g_tt_bits = 21;
static int g_tt_size = 1 << 21;

struct TTEntry {
    uint32_t key;   // bits 31-28: depth, bits 27-0: (key_hi ^ val_bits) & 0x0FFFFFFF
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

static inline uint64_t make_key(Bitboard board) {
    return board ^ g_gen_salt;
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
    uint64_t key = make_key(board);
    uint32_t key_hi = static_cast<uint32_t>(key >> 32);
    TTEntry e = g_table[index(key)];
    uint32_t stored_depth = (e.key >> 28) & 0xF;
    uint32_t key_check = (e.key ^ e.data) & 0x0FFFFFFFU;
    if (key_check == (key_hi & 0x0FFFFFFFU) && stored_depth <= static_cast<uint32_t>(depth)) {
        value = bits_to_float(e.data);
        return true;
    }
    return false;
}

void TT::store(Bitboard board, int depth, float value) {
    if (!g_enabled) return;
    uint64_t key = make_key(board);
    uint32_t key_hi = static_cast<uint32_t>(key >> 32);
    uint32_t val_bits = float_to_bits(value);
    uint32_t idx = index(key);

    // Depth-preferred replacement: keep the entry with more lookahead
    // (lower depth = closer to root = more remaining search).
    // Always replace if different board / stale generation.
    TTEntry existing = g_table[idx];
    uint32_t existing_key_frag = (existing.key ^ existing.data) & 0x0FFFFFFFU;
    uint32_t my_key_frag = key_hi & 0x0FFFFFFFU;

    if (existing_key_frag != my_key_frag || static_cast<uint32_t>(depth) <= ((existing.key >> 28) & 0xF)) {
        g_table[idx] = {
            ((key_hi ^ val_bits) & 0x0FFFFFFFU) | (static_cast<uint32_t>(depth) << 28),
            val_bits
        };
    }
}

void TT::set_enabled(bool enabled) {
    g_enabled = enabled;
}
