#!/usr/bin/env python3
"""Run one automated Civ4 BtS game with a given CvGameCoreDLL and report per-turn timings.

The game runs windowed from its own user folder (bench/altroot), so the player's own
settings, saves and mods are not touched. It uses the game's built-in automation:
QuickStart (start a game from ini settings), Autorun (every player is an AI) and fixed
random seeds, so two runs with the same arguments play the same game.

  python run_bench.py --dll path\\to\\CvGameCoreDLL.dll --label clang --turns 50

Each run writes results/<label>.log: one TURN line per game turn with the wall time since
the previous turn and a hash of the game state. Compare two logs with compare_bench.py.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

BENCH = Path(__file__).resolve().parent
ALTROOT = BENCH / 'altroot'
MOD_NAME = 'BridgedBench'
MOD = ALTROOT / 'MODS' / MOD_NAME
RESULTS = BENCH / 'results'
EXE = 'Civ4BeyondSword.exe'
DEFAULT_BTS = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / \
    r"Steam\steamapps\common\Sid Meier's Civilization IV Beyond the Sword\Beyond the Sword"
DEFAULT_INI = Path.home() / 'Documents' / 'My Games' / 'Beyond the Sword' / 'CivilizationIV.ini'


def write_ini(template, overrides):
    """Copy the player's ini, replacing only the keys the benchmark controls."""
    remaining = dict(overrides)
    lines = []
    for line in template.read_text(encoding='latin-1').splitlines():
        match = re.match(r'^([A-Za-z][\w ]*?)\s*=', line)
        if match and match.group(1) in overrides:
            key = match.group(1)
            line = f'{key} = {overrides[key]}'
            remaining.pop(key, None)
        lines.append(line)
    if remaining:
        lines.append('')
        lines.extend(f'{key} = {value}' for key, value in remaining.items())
    ALTROOT.mkdir(parents=True, exist_ok=True)
    (ALTROOT / 'CivilizationIV.ini').write_text('\r\n'.join(lines) + '\r\n', encoding='latin-1')


def game_running():
    out = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {EXE}', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    return EXE.lower() in out.lower()


def kill_game():
    subprocess.run(['taskkill', '/F', '/IM', EXE], capture_output=True, text=True)


def deploy_mod(dll, label, turns, log):
    """The mod is the harness scripts plus the DLL under test, nothing else."""
    assets = MOD / 'Assets'
    if assets.exists():
        shutil.rmtree(assets)
    shutil.copytree(BENCH / 'mod' / 'Assets', assets)
    shutil.copyfile(dll, assets / 'CvGameCoreDLL.dll')
    (assets / 'Python' / 'BenchConfig.py').write_text(
        '# Written by run_bench.py for this run.\n'
        f'LOG_PATH = r"{log}"\n'
        f'LABEL = "{label}"\n'
        f'MAX_TURNS = {turns}\n', encoding='ascii')


def read_turns(log):
    """TURN lines up to the DONE marker: (game turn, ms, hash, units, cities)."""
    turns = []
    for line in log.read_text(encoding='latin-1').splitlines():
        if line.startswith('DONE'):
            break
        match = re.match(r'TURN (\d+) ms=([-\d.]+) hash=(\w+) units=(\d+) cities=(\d+)', line)
        if match:
            turns.append((int(match[1]), float(match[2]), match[3], int(match[4]), int(match[5])))
    return turns


def summarize(log):
    turns = read_turns(log)
    timed = [t for t in turns if t[1] >= 0]
    if not timed:
        return 'no timed turns logged'
    total = sum(t[1] for t in timed) / 1000.0
    last = turns[-1]
    return (f'{len(timed)} timed turns (game turns {timed[0][0]}-{timed[-1][0]}), '
            f'{total:.1f} s total, {1000.0 * total / len(timed):.0f} ms/turn mean, '
            f'slowest {max(t[1] for t in timed):.0f} ms; '
            f'final turn {last[0]}: hash {last[2]}, {last[3]} units, {last[4]} cities')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dll', type=Path, required=True, help='CvGameCoreDLL.dll to test')
    parser.add_argument('--label', required=True, help='name for this run; results/<label>.log')
    parser.add_argument('--turns', type=int, default=50, help='game turns to play')
    parser.add_argument('--save', type=Path, help='load this save instead of starting a new game')
    parser.add_argument('--map', default='Continents', help='map script for a new game')
    parser.add_argument('--world-size', default='WORLDSIZE_STANDARD')
    parser.add_argument('--game-speed', default='GAMESPEED_NORMAL')
    parser.add_argument('--seed', type=int, default=20261120, help='map and game random seed')
    parser.add_argument('--autosave-interval', type=int, default=0, help='0 disables autosaves')
    parser.add_argument('--timeout', type=int, default=900, help='seconds before the game is killed')
    parser.add_argument('--bts-dir', type=Path, default=DEFAULT_BTS)
    parser.add_argument('--template-ini', type=Path, default=DEFAULT_INI)
    parser.add_argument('--fullscreen', action='store_true')
    parser.add_argument('--profile', action='store_true',
                        help='for a DLL built with build_dll.py --profile: write results/<label>.profile.tsv')
    args = parser.parse_args()

    exe = args.bts_dir / EXE
    for path in [exe, args.dll, args.template_ini] + ([args.save] if args.save else []):
        if not path.is_file():
            parser.error(f'not found: {path}')
    if not re.fullmatch(r'[\w.-]+', args.label):
        parser.error('label may contain only letters, digits, dot, dash and underscore')
    if game_running():
        parser.error(f'{EXE} is already running; close it first')

    RESULTS.mkdir(exist_ok=True)
    log = RESULTS / f'{args.label}.log'
    log.unlink(missing_ok=True)
    deploy_mod(args.dll.resolve(), args.label, args.turns, log)

    overrides = {
        'FullScreen': '1' if args.fullscreen else '0',
        'ScreenWidth': '1280', 'ScreenHeight': '800',
        'NoIntroMovie': '1', 'PlayMusic': '0', 'AudioEnable': '0',
        'HidePythonExceptions': '1', 'LoggingEnabled': '1', 'OverwriteLogs': '1',
        'Mod': f'Mods\\{MOD_NAME}',
        'QuickStart': '1',
        'GameType': 'spLoad' if args.save else 'singlePlayer',
        'FileName': str(args.save.resolve()) if args.save else '0',
        'Map': args.map, 'WorldSize': args.world_size, 'GameSpeed': args.game_speed,
        'Era': 'ERA_ANCIENT', 'SeaLevel': 'SEALEVEL_MEDIUM', 'Climate': 'CLIMATE_TEMPERATE',
        'QuickHandicap': 'HANDICAP_NOBLE',
        'GameOptions': '0' * 24, 'VictoryConditions': '0' * 7, 'MaxTurns': '0',
        'MapRandSeed': str(args.seed), 'SyncRandSeed': str(args.seed),
        'Autorun': '1', 'AutorunTurnLimit': '0',
        'AutoSaveInterval': str(args.autosave_interval), 'MaxAutoSaves': '50',
        'GameName': 'Bench', 'CheatCode': '0',
    }
    write_ini(args.template_ini, overrides)

    env = dict(os.environ)
    env.pop('BRIDGED_PROFILE_LOG', None)
    if args.profile:
        profile = RESULTS / f'{args.label}.profile.tsv'
        profile.unlink(missing_ok=True)
        env['BRIDGED_PROFILE_LOG'] = str(profile)

    started = time.monotonic()
    process = subprocess.Popen([str(exe), f'/ALTROOT={ALTROOT}'], cwd=str(args.bts_dir), env=env)
    outcome = 'timeout'
    seen_init = False
    while time.monotonic() - started < args.timeout:
        time.sleep(2)
        text = log.read_text(encoding='latin-1') if log.exists() else ''
        if not seen_init and 'INIT' in text:
            seen_init = True
            print(f'harness loaded after {time.monotonic() - started:.0f} s', flush=True)
        if 'HARNESS_ERROR' in text:
            outcome = 'harness error'
            break
        if 'DONE' in text:
            outcome = 'done'
            break
        # The Steam build may hand over to a second process, so trust the process list.
        if process.poll() is not None and not game_running():
            outcome = 'game exited'
            break
    # The game keeps playing past the turn count, so it is stopped here.
    if game_running():
        kill_game()
    elapsed = time.monotonic() - started
    print(f'{args.label}: {outcome} after {elapsed:.0f} s; harness {"loaded" if seen_init else "never loaded"}')
    if log.exists():
        print(summarize(log))
        print(log)
    return 0 if outcome == 'done' else 1


if __name__ == '__main__':
    sys.exit(main())
