# 2048cpp

A highly optimized AI solver for the game [2048](https://play2048.co/), combining bitboard representation with a parallelized expectimax search algorithm.

## How It Works

The AI uses three key techniques to play 2048 at high speed:

- **Bitboard representation** — The entire 4x4 board is packed into a single 64-bit integer (4 bits per tile). All moves are executed via O(1) lookup tables pre-computed at startup (~5 MB).
- **Expectimax search** — The game tree is searched with expectimax (depth 4), treating the random tile placement as a chance node. Branches with probability below 0.1% are pruned.
- **OpenMP parallelization** — Each candidate move is evaluated in a separate thread for ~2-4x speedup.

## Prerequisites

- A C++17-compatible compiler (GCC, Clang, or MSVC)
- [CMake](https://cmake.org/) 3.14 or later
- OpenMP support

### Installing dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install cmake g++ libomp-dev
```

**macOS (Homebrew):**
```bash
brew install cmake libomp
```

**Arch Linux:**
```bash
sudo pacman -S cmake gcc openmp
```

## Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel
```

### macOS note

Apple Clang does not ship with OpenMP. After installing `libomp` via Homebrew, configure with:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DOpenMP_C_FLAGS="-Xpreprocessor -fopenmp -I$(brew --prefix libomp)/include" \
  -DOpenMP_C_LIB_NAMES="omp" \
  -DOpenMP_CXX_FLAGS="-Xpreprocessor -fopenmp -I$(brew --prefix libomp)/include" \
  -DOpenMP_CXX_LIB_NAMES="omp" \
  -DOpenMP_omp_LIBRARY="$(brew --prefix libomp)/lib/libomp.dylib"
```

## Running

```bash
./build/2048cpp
```

The AI will play a full game of 2048, printing each move and the board state to stdout. At the end it reports the maximum tile reached and average time per move.

Example output:
```
Move 1: Left
Value: 48.6
|-------+-------+-------+-------|
|     0 |     0 |     0 |     0 |
|-------+-------+-------+-------|
|     0 |     0 |     0 |     0 |
|-------+-------+-------+-------|
|     0 |     0 |     0 |     2 |
|-------+-------+-------+-------|
|     0 |     0 |     2 |     4 |
|-------+-------+-------+-------|
...
Game Over.
Max: 2048
Time taken parallel: 12.3 ms/move
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
```

## Project Structure

```
├── CMakeLists.txt              # Build configuration
├── .github/workflows/ci.yml   # CI pipeline
├── types.h                     # Core types (Bitboard, Move, Square enums)
├── bitboard.h / bitboard.cpp   # Board representation and move operations
├── search.h / search.cpp       # Expectimax search algorithm
├── main.cpp                    # Game entry point
└── tests/
    └── test_main.cpp           # Test suite
```

## License

This project is provided as-is for educational purposes.
