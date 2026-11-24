#!/usr/bin/env python3
"""Start Civ4 BtS for a person to play, with a given CvGameCoreDLL, and log what it did.

Like run_bench.py the game runs from its own user folder (bench/playtest-root), so the
player's own settings, saves and mods are not touched: the settings are copied from the
player's CivilizationIV.ini, with the mod set to the DLL under test. Nothing is automated;
start or load a game and play. Saves made in it go to bench/playtest-root.

With a --profile DLL the profile log is written each game turn to
results/<label>.profile.tsv, as in a bench run.

  python playtest.py --dll path\\to\\CvGameCoreDLL.dll --label playtest1 --define WORK_AHEAD_TRIPWIRE=1
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

BENCH = Path(__file__).resolve().parent
ROOT = BENCH / 'playtest-root'
MOD_NAME = 'BridgedPlaytest'
MOD = ROOT / 'MODS' / MOD_NAME
RESULTS = BENCH / 'results'
EXE = 'Civ4BeyondSword.exe'
DEFAULT_BTS = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / \
    'Steam' / 'steamapps' / 'common' / "Sid Meier's Civilization IV Beyond the Sword" / 'Beyond the Sword'
DEFAULT_INI = Path.home() / 'Documents' / 'My Games' / 'Beyond the Sword' / 'CivilizationIV.ini'


def write_ini(template, overrides):
    remaining = dict(overrides)
    lines = []
    for line in template.read_text(encoding='latin-1').splitlines():
        match = re.match(r'^([A-Za-z][\w ]*?)\s*=', line)
        if match and match.group(1) in remaining:
            key = match.group(1)
            line = f'{key} = {remaining.pop(key)}'
        lines.append(line)
    lines.extend(f'{key} = {value}' for key, value in remaining.items())
    ROOT.mkdir(parents=True, exist_ok=True)
    (ROOT / 'CivilizationIV.ini').write_text('\r\n'.join(lines) + '\r\n', encoding='latin-1')


def deploy_mod(dll, defines):
    assets = MOD / 'Assets'
    if assets.exists():
        shutil.rmtree(assets)
    (assets / 'XML').mkdir(parents=True)
    shutil.copyfile(dll, assets / 'CvGameCoreDLL.dll')
    entries = ''.join(f'\t<Define>\n\t\t<DefineName>{name}</DefineName>\n'
                      f'\t\t<iDefineIntVal>{value}</iDefineIntVal>\n\t</Define>\n'
                      for name, value in defines.items())
    (assets / 'XML' / 'GlobalDefinesAlt.xml').write_text(
        '<?xml version="1.0"?>\n<Civ4Defines xmlns="x-schema:CIV4GlobalDefinesSchema.xml">\n'
        + entries + '</Civ4Defines>\n', encoding='ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dll', type=Path, required=True, help='CvGameCoreDLL.dll to play with')
    parser.add_argument('--label', required=True, help='name for this playtest; results/<label>.profile.tsv')
    parser.add_argument('--define', action='append', default=[], metavar='NAME=INT',
                        help='integer global define for the mod; repeatable')
    parser.add_argument('--bts-dir', type=Path, default=DEFAULT_BTS)
    parser.add_argument('--template-ini', type=Path, default=DEFAULT_INI)
    parser.add_argument('--deploy-only', action='store_true', help='set up the mod and settings, do not start the game')
    args = parser.parse_args()

    defines = {}
    for item in args.define:
        match = re.fullmatch(r'([A-Z][A-Z0-9_]*)=(-?\d+)', item)
        if not match:
            parser.error(f'--define wants NAME=INT, got {item}')
        defines[match[1]] = int(match[2])

    exe = args.bts_dir / EXE
    if not exe.exists():
        sys.exit(f'{exe} not found; pass --bts-dir')

    deploy_mod(args.dll.resolve(), defines)
    write_ini(args.template_ini, {
        'Mod': f'Mods\\{MOD_NAME}',
        'QuickStart': '0', 'Autorun': '0',
        'HidePythonExceptions': '0', 'LoggingEnabled': '1', 'OverwriteLogs': '1',
    })

    env = dict(os.environ)
    env.pop('BRIDGED_PROFILE_LOG', None)
    RESULTS.mkdir(exist_ok=True)
    profile = RESULTS / f'{args.label}.profile.tsv'
    profile.unlink(missing_ok=True)
    env['BRIDGED_PROFILE_LOG'] = str(profile)

    if args.deploy_only:
        print(f'set up in {ROOT}; not started')
        return

    subprocess.Popen([str(exe), f'/ALTROOT={ROOT}'], cwd=str(args.bts_dir), env=env)
    print(f'started; with a profile DLL the log goes to {profile}')


if __name__ == '__main__':
    main()
