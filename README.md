# 2048cpp

A highly optimized AI solver for the game [2048](https://play2048.co/), combining bitboard representation with a parallelized expectimax search algorithm.

## How It Works

The AI uses three key techniques to play 2048 at high speed:

- **Bitboard representation** — The entire 4x4 board is packed into a single 64-bit integer (4 bits per tile). All moves are executed via O(1) lookup tables pre-computed at startup (~5 MB).
- **Expectimax search** — The game tree is searched with expectimax (adaptive depth 3-7), treating the random tile placement as a chance node. Branches with probability below 0.1% are pruned.
- **Transposition table** — A lockless shared flat hash table caches evaluated positions across parallel workers, using XOR key verification to safely handle concurrent reads and writes without locks.
- **Parallel search** — Tile-placement children are dispatched to a persistent thread pool (~80 tasks per search step), saturating all available cores with no external dependencies.

## Prerequisites

- A C++17-compatible compiler (GCC, Clang, or MSVC)
- [CMake](https://cmake.org/) 3.14 or later

## Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel
```

## Running

```bash
./build/2048cpp                        # single game, random seed
./build/2048cpp --seed 42              # deterministic seed
./build/2048cpp --games 50 --seed 42   # batch of 50 games (seeds 42..91)
./build/2048cpp -v                     # verbose move-by-move output
./build/2048cpp --no-tt                # disable transposition table
./build/2048cpp --threads 8            # use 8 worker threads (default: all cores)
```

## Running Tests

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run a specific test
./build/2048tests bitboard_conversion
./build/2048tests move_left_right
./build/2048tests move_consistency
./build/2048tests empty_squares
./build/2048tests place_random
./build/2048tests evaluation
./build/2048tests board_score
./build/2048tests node_counter
./build/2048tests monotonicity
./build/2048tests smoothness
```

## Project Structure

```
├── CMakeLists.txt              # Build configuration
├── .github/workflows/ci.yml   # CI pipeline
├── src/
│   ├── types.h                 # Core types (Bitboard, Move, Square enums)
│   ├── bitboard.h / bitboard.cpp   # Board representation and move operations
│   ├── tt.h / tt.cpp               # Transposition table (lockless flat hash)
│   ├── search.h / search.cpp       # Expectimax search algorithm
│   ├── threadpool.h                # Header-only thread pool
│   └── main.cpp                    # Game entry point
└── tests/
    └── test_main.cpp           # Test suite
```

## License

This project is provided as-is for educational purposes.
