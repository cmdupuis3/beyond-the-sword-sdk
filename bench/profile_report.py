#!/usr/bin/env python3
"""Summarise an in-DLL profile written by a --profile benchmark run.

  python profile_report.py results/std150-profile.profile.tsv [rows]

Prints the scopes that cost the most by their own time (exclusive) and by total time
including what they call (inclusive). Times are totals over the whole run. Exclusive time
is approximate for functions that recurse through another scope.
"""
import sys
from pathlib import Path


def read_profile(path):
    lines = Path(path).read_text(encoding='latin-1').splitlines()
    header = lines[0].split('\t')
    info = {'game_turn': int(header[1]), 'wall_ms': float(header[3])}
    rows = []
    for line in lines[2:]:
        parts = line.split('\t')
        if len(parts) == 4:
            rows.append((float(parts[0]), float(parts[1]), int(parts[2]), parts[3]))
    return info, rows


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 25
    info, rows = read_profile(sys.argv[1])
    update = next((row for row in rows if row[3] == 'CvGame::update'), None)
    turns = max(1, info['game_turn'])
    print(f'totals up to game turn {info["game_turn"]}; {len(rows)} scopes ran')
    if update:
        print(f'CvGame::update (all turn processing): {update[0] / 1000:.1f} s, '
              f'{update[0] / turns:.0f} ms per game turn')
    base = update[0] if update else sum(row[1] for row in rows)
    for title, key in [('own time (exclusive)', 1), ('total time (inclusive)', 0)]:
        print(f'\ntop {count} by {title}')
        print(f'{"ms":>10} {"% of update":>12} {"calls":>10}  scope')
        for row in sorted(rows, key=lambda r: r[key], reverse=True)[:count]:
            print(f'{row[key]:10.0f} {100.0 * row[key] / base:11.1f}% {row[2]:10d}  {row[3]}')


if __name__ == '__main__':
    main()
