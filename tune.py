#!/usr/bin/env python3
"""
CMA-ES tuner for 2048 AI evaluation weights.

Optimizes the 5 evaluation weights (grad, mono, mono_power, empty, merge)
by running batches of games and maximizing average score.

Usage:
    python3 tune.py                          # defaults: 20 games/eval, 50 generations
    python3 tune.py --games 50 --gens 100    # more thorough
    python3 tune.py --resume log.json        # resume from saved state
    python3 tune.py --binary ./build/2048cpp # custom binary path

Requires: pip install numpy cma
"""

import argparse
import json
import os
import subprocess
import sys

import cma
import numpy as np


# Default weights (current values in the codebase)
DEFAULT_WEIGHTS = {
    "grad":  5.0,
    "mono":  47.0,
    "power": 4.0,
    "empty": 270.0,
    "merge": 700.0,
}

WEIGHT_NAMES = list(DEFAULT_WEIGHTS.keys())


def run_games(binary, weights, num_games, seed, threads):
    """Run a batch of games with given weights, return (mean_score, median_score, scores)."""
    cmd = [
        binary,
        "--games", str(num_games),
        "--seed", str(seed),
        "--threads", str(threads),
        "--grad-weight",  f"{weights[0]:.6f}",
        "--mono-weight",  f"{weights[1]:.6f}",
        "--mono-power",   f"{weights[2]:.6f}",
        "--empty-weight", f"{weights[3]:.6f}",
        "--merge-weight", f"{weights[4]:.6f}",
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    except subprocess.TimeoutExpired:
        return 0.0, 0.0, []

    # Parse per-game scores from tab-separated output lines:
    # seed  moves  max_tile  score  time_s  nodes
    scores = []
    for line in result.stdout.splitlines():
        parts = line.split("\t")
        if len(parts) == 6:
            try:
                scores.append(int(parts[3]))  # score column
            except ValueError:
                continue

    if not scores:
        return 0.0, 0.0, []

    return float(np.mean(scores)), float(np.median(scores)), scores


def weights_to_internal(w):
    """Convert raw weights to internal CMA-ES space (log-space, except mono_power)."""
    return np.array([
        np.log(max(w[0], 0.01)),  # grad
        np.log(max(w[1], 0.01)),  # mono
        w[2],                      # power (linear, typically 1-6)
        np.log(max(w[3], 0.01)),  # empty
        np.log(max(w[4], 0.01)),  # merge
    ])


def internal_to_weights(x):
    """Convert internal CMA-ES space back to raw weights."""
    return np.array([
        np.exp(x[0]),  # grad
        np.exp(x[1]),  # mono
        max(x[2], 0.5),  # power (clamped)
        np.exp(x[3]),  # empty
        np.exp(x[4]),  # merge
    ])


def save_log(path, history, best_weights, best_score, es):
    """Save tuning progress to JSON."""
    data = {
        "best_weights": dict(zip(WEIGHT_NAMES, best_weights.tolist())),
        "best_score": best_score,
        "history": history,
        "cma_mean": list(es.mean),
        "cma_sigma": es.sigma,
        "cma_gen": es.countiter,
    }
    with open(path, "w") as f:
        json.dump(data, f, indent=2)


def main():
    parser = argparse.ArgumentParser(description="CMA-ES tuner for 2048 AI eval weights")
    parser.add_argument("--binary", default="./build/2048cpp", help="Path to 2048cpp binary")
    parser.add_argument("--games", type=int, default=20, help="Games per evaluation (more = less noise)")
    parser.add_argument("--gens", type=int, default=50, help="Number of CMA-ES generations")
    parser.add_argument("--pop", type=int, default=0, help="Population size (0 = auto)")
    parser.add_argument("--sigma", type=float, default=0.3, help="Initial step size in log-space")
    parser.add_argument("--seed", type=int, default=100, help="Base game seed")
    parser.add_argument("--threads", type=int, default=0, help="Threads for game (0 = auto)")
    parser.add_argument("--log", default="tune_log.json", help="Log file path")
    parser.add_argument("--resume", default="", help="Resume from log file")
    args = parser.parse_args()

    if not os.path.isfile(args.binary):
        print(f"Error: binary not found at {args.binary}")
        print("Build first: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel")
        sys.exit(1)

    if args.threads <= 0:
        args.threads = os.cpu_count() or 4

    # Initialize starting point
    x0 = weights_to_internal(list(DEFAULT_WEIGHTS.values()))

    if args.resume and os.path.isfile(args.resume):
        with open(args.resume) as f:
            saved = json.load(f)
        x0 = np.array(saved["cma_mean"])
        print(f"Resumed from {args.resume}, generation {saved['cma_gen']}, "
              f"best score {saved['best_score']:.0f}")

    else:
        # Evaluate baseline
        baseline_w = list(DEFAULT_WEIGHTS.values())
        mean_s, med_s, _ = run_games(args.binary, baseline_w, args.games, args.seed, args.threads)
        print(f"Baseline weights: {dict(zip(WEIGHT_NAMES, baseline_w))}")
        print(f"Baseline score:   mean={mean_s:.0f}  median={med_s:.0f}")
        print()

    # Configure pycma options
    opts = {
        "verbose": -9,         # suppress pycma's own output
        "verb_disp": 0,
        "verb_log": 0,
        "verb_filenameprefix": "",
        "maxiter": args.gens,
        # Boundary handling: mono_power (index 2) should stay in [0.5, 8]
        # Other dimensions are in log-space, loosely bounded
        "bounds": [[-5, -5, 0.5, -5, -5],
                   [10, 10, 8.0, 10, 10]],
    }
    if args.pop > 0:
        opts["popsize"] = args.pop

    es = cma.CMAEvolutionStrategy(x0.tolist(), args.sigma, opts)

    best_score = -1e9
    best_weights = internal_to_weights(x0)
    history = []

    pop_size = es.popsize
    print(f"CMA-ES (pycma): pop={pop_size}, dimensions={len(x0)}")
    print(f"Games per evaluation: {args.games}, base seed: {args.seed}")
    print(f"{'Gen':>4}  {'Best':>8}  {'Mean':>8}  {'Sigma':>7}  Weights")
    print("-" * 90)

    gen = 0
    while not es.stop() and gen < args.gens:
        # Use different seeds each generation to avoid overfitting
        gen_seed = args.seed + gen * args.games

        candidates = es.ask()
        fitnesses = []

        for x in candidates:
            w = internal_to_weights(np.array(x))
            mean_s, med_s, scores = run_games(
                args.binary, w.tolist(), args.games, gen_seed, args.threads
            )
            fitnesses.append(mean_s)

            if mean_s > best_score:
                best_score = mean_s
                best_weights = w.copy()

        # pycma minimizes, so negate scores
        es.tell(candidates, [-f for f in fitnesses])

        gen_best = max(fitnesses)
        gen_mean = np.mean(fitnesses)
        w_display = internal_to_weights(np.array(es.mean))

        entry = {
            "gen": gen,
            "best": gen_best,
            "mean": gen_mean,
            "sigma": es.sigma,
            "weights": dict(zip(WEIGHT_NAMES, w_display.tolist())),
        }
        history.append(entry)

        w_str = "  ".join(f"{n}={v:.1f}" for n, v in zip(WEIGHT_NAMES, w_display))
        print(f"{gen:4d}  {gen_best:8.0f}  {gen_mean:8.0f}  {es.sigma:7.4f}  {w_str}")

        # Save progress each generation
        save_log(args.log, history, best_weights, best_score, es)
        gen += 1

    if es.stop():
        reasons = es.stop()
        print(f"\nCMA-ES stopped early: {reasons}")

    print()
    print("=" * 60)
    print("Tuning complete!")
    print(f"Best mean score: {best_score:.0f}")
    print(f"Best weights:")
    for name, val in zip(WEIGHT_NAMES, best_weights):
        print(f"  {name:>5} = {val:.2f}")
    print()
    print("To use these weights:")
    cmd = (
        f"./build/2048cpp --games 50 --seed 42"
        f" --grad-weight {best_weights[0]:.2f}"
        f" --mono-weight {best_weights[1]:.2f}"
        f" --mono-power {best_weights[2]:.2f}"
        f" --empty-weight {best_weights[3]:.2f}"
        f" --merge-weight {best_weights[4]:.2f}"
    )
    print(cmd)
    print()
    print(f"Log saved to {args.log}")


if __name__ == "__main__":
    main()
