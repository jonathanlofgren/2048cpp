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
./build/2048cpp                        # single game, random seed
./build/2048cpp --seed 42              # single game, deterministic
./build/2048cpp --games 50 --seed 42   # 50 games, seeds 42..91
./build/2048cpp -v                     # verbose move-by-move output
./build/2048cpp --no-tt                # disable transposition table
```

## Benchmark

```bash
# Quick comparison (50 games, deterministic)
./build/2048cpp --games 50 --seed 42

# Compare two branches:
git stash && ./build/2048cpp --games 50 --seed 42 > baseline.txt
git stash pop && cmake --build build --parallel
./build/2048cpp --games 50 --seed 42 > new.txt
diff baseline.txt new.txt
```

## Architecture

This is a 2048 AI solver using bitboard representation and parallelized expectimax search.

- **types.h** — Core types: `Bitboard` (uint64_t), `Move`, `Square`, `Row`, `Col` enums
- **bitboard.h/cpp** — Board representation, O(1) move lookup tables (pre-computed at init), random tile placement
- **tt.h/cpp** — Transposition table: lockless shared flat hash table with Fibonacci hashing, torn-read safe (see comment in tt.cpp)
- **search.h/cpp** — Expectimax search (adaptive depth 3-7, probability cutoff 0.001), parallel via `std::async`
- **main.cpp** — Game loop entry point
- **tests/test_main.cpp** — Test suite (10 tests, run individually by name via CTest)

### Key design decisions

- The entire 4×4 board fits in a single `uint64_t` (4 bits per tile, 16 tiles = 64 bits). Tile values are stored as exponents: bit value `n` = game tile `2^n`.
- All move operations are O(1) lookups into ~5 MB of pre-computed tables (initialized in `Bitboards::init()`).
- The vector representation in `bitboard_to_vector()` reverses column order — "left" in the game shifts toward the high nibble side of the bitboard.
- `Search::init()` pre-computes `RowEval`/`ColEval` tables for O(1) board evaluation using a snake gradient with monotonicity, smoothness, merge, and empty-square components.
- The transposition table is a single global flat hash table (not thread-local) to avoid per-thread allocation costs with `std::async`. Concurrent access is lockless; torn reads are detected via XOR of key and value bits (see detailed comment in tt.cpp).
