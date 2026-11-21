#!/usr/bin/env python3
"""Compare two benchmark logs written by run_bench.py.

  python compare_bench.py results/stock.log results/clang.log

Reports whether both runs played the same game (state hash equal on every turn) and how
their turn times differ, overall and by stretch of the game.
"""
import sys
from pathlib import Path

from run_bench import read_turns


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    names = [Path(arg).stem for arg in sys.argv[1:]]
    runs = [{t[0]: t for t in read_turns(Path(arg))} for arg in sys.argv[1:]]
    common = sorted(set(runs[0]) & set(runs[1]))
    if not common:
        sys.exit('the logs share no game turns')
    print(f'{names[0]}: {len(runs[0])} turns logged; {names[1]}: {len(runs[1])} turns logged; {len(common)} in common')

    diverged = next((turn for turn in common if runs[0][turn][2] != runs[1][turn][2]), None)
    if diverged is None:
        print(f'SAME GAME: state hash identical on all {len(common)} common turns')
        comparable = common
    else:
        print(f'DIVERGED at game turn {diverged}: hashes {runs[0][diverged][2]} vs {runs[1][diverged][2]}')
        comparable = [turn for turn in common if turn < diverged]
        print(f'timing compared only over the {len(comparable)} turns before that')

    timed = [turn for turn in comparable if runs[0][turn][1] >= 0 and runs[1][turn][1] >= 0]
    if not timed:
        return
    print(f'\n{"game turns":>12} {names[0] + " s":>12} {names[1] + " s":>12} {"ratio":>7} {"units":>7}')
    step = max(1, len(timed) // 5)
    chunks = [timed[i:i + step] for i in range(0, len(timed), step)]
    if len(chunks) > 5:
        chunks[-2].extend(chunks.pop())
    for chunk in chunks + [timed]:
        a = sum(runs[0][turn][1] for turn in chunk) / 1000.0
        b = sum(runs[1][turn][1] for turn in chunk) / 1000.0
        label = 'all' if chunk is timed else f'{chunk[0]}-{chunk[-1]}'
        print(f'{label:>12} {a:12.1f} {b:12.1f} {b / a:7.3f} {runs[0][chunk[-1]][3]:7d}')
    print(f'\nratio = {names[1]} time / {names[0]} time; below 1 means {names[1]} is faster')


if __name__ == '__main__':
    main()
