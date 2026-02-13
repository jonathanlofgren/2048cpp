# CLAUDE.md

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/2048cpp
```

## Architecture

This is a 2048 AI solver using bitboard representation and parallelized expectimax search.

- **types.h** — Core types: `Bitboard` (uint64_t), `Move`, `Square`, `Row`, `Col` enums
- **bitboard.h/cpp** — Board representation, O(1) move lookup tables (pre-computed at init), random tile placement
- **search.h/cpp** — Expectimax search (depth 4, probability cutoff 0.001), parallel via `std::async`
- **main.cpp** — Game loop entry point
- **tests/test_main.cpp** — Test suite (6 tests, run individually by name via CTest)

### Key design decisions

- The entire 4×4 board fits in a single `uint64_t` (4 bits per tile, 16 tiles = 64 bits). Tile values are stored as exponents: bit value `n` = game tile `2^n`.
- All move operations are O(1) lookups into ~5 MB of pre-computed tables (initialized in `Bitboards::init()`).
- The vector representation in `bitboard_to_vector()` reverses column order — "left" in the game shifts toward the high nibble side of the bitboard.
- `Search::init()` pre-computes a `RowValue` table for O(1) board evaluation using a diagonal-linear gradient (favors keeping high tiles in corners).
