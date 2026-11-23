#!/usr/bin/env python3
"""Say where the exe's time goes between two calls of CvGame::update().

  python exe_time_report.py results/std150.profile.tsv [path\\to\\CvGameCoreDLL.dll]

A --profile DLL samples the game's thread about once a millisecond while the exe has it
(see "Where the exe's time goes" in CvGameCoreDLL.cpp) and writes three files beside the
profile:

  <profile>.frames.tsv  one row for each time the exe had the thread: the game turn, the
                        units in the game, how long the call of CvGame::update() before
                        it took, and how long the exe then kept the thread
  <profile>.exe.tsv     samples by module, and the addresses sampled most often
  <profile>.stacks.bin  for each sample, the words on the stack that point into the code
                        of the exe or of the DLL

The exe has no symbols, so its code shows as offsets. What names its work is the DLL: the
report lists the exe addresses that are on the most stacks, and for each the DLL functions
the exe was inside when it had come that way. Give the DLL the game was played with to have
them named (this needs the pefile module, which the build's Python environment has). A
name is that of the nearest exported function below the address, so a function that is
not exported shows under the name of one before it.
"""
import bisect
import collections
import struct
import sys
from pathlib import Path


def frames_report(path):
    rows = [line.split('\t') for line in path.read_text().splitlines()[1:]]
    rows = [(int(r[0]), int(r[1]), int(r[2]), float(r[3])) for r in rows if len(r) == 4]
    if not rows:
        return
    total = sum(r[3] for r in rows)
    print(f'{len(rows)} times the exe had the thread, {total / 1000.0:.1f} s in all, {total / len(rows):.0f} ms each')
    last_turn = rows[-1][0]
    thirds = [(0, last_turn // 3), (last_turn // 3, 2 * last_turn // 3), (2 * last_turn // 3, last_turn + 1)]
    for low, high in thirds:
        part = [r for r in rows if low <= r[0] < high]
        if part:
            print(f'  game turns {low}-{high - 1}: {len(part)} times, {sum(r[3] for r in part) / len(part):.0f} ms each, '
                  f'{sum(r[1] for r in part) // len(part)} units')


def demangle(name):
    # ?isInfoBarDirty@CvUnit@@QBE_NXZ -> CvUnit::isInfoBarDirty
    if name.startswith('?'):
        return '::'.join(reversed(name[1:].split('@@')[0].split('@')))
    return name


def stacks_report(path, dll_path):
    data = path.read_bytes()
    exe_base, exe_low, exe_high, dll_base, dll_low, dll_high, count, size = struct.unpack_from('<8I', data, 0)
    exports = []
    if dll_path:
        import pefile
        pe = pefile.PE(str(dll_path), fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
        exports = sorted((e.address, (e.name or b'?').decode()) for e in pe.DIRECTORY_ENTRY_EXPORT.symbols)
    export_starts = [e[0] for e in exports]

    def dll_name(address):
        index = bisect.bisect_right(export_starts, address - dll_base) - 1
        return demangle(exports[index][1]) if index >= 0 else f'dll+{address - dll_base:08x}'

    def in_exe(address):
        return exe_low <= address < exe_high

    def in_dll(address):
        return dll_low <= address < dll_high

    samples = []
    for index in range(count):
        offset = 32 + index * size
        address, words = struct.unpack_from('<2I', data, offset)
        samples.append((address, struct.unpack_from(f'<{words}I', data, offset + 8)))
    if not samples:
        return

    on_stacks = collections.Counter()
    for address, words in samples:
        on_stacks.update(set(word for word in words if in_exe(word)))

    # the DLL function the exe called is the outermost one before the first exe address
    called = collections.Counter()
    called_under = collections.defaultdict(collections.Counter)
    for address, words in samples:
        if not in_dll(address):
            continue
        name = dll_name(address)
        for word in words:
            if in_exe(word):
                break
            if in_dll(word):
                name = dll_name(word)
        called[name] += 1
        for word in set(word for word in words if in_exe(word)):
            called_under[word][name] += 1

    print(f'\n{count} samples; {sum(called.values())} with the thread inside the DLL, in:')
    for name, number in called.most_common(25):
        print(f'  {number:5d}  {name}')

    print('\nexe addresses on the most stacks (offset, share of samples, DLL functions called from under them)')
    for address, number in on_stacks.most_common(45):
        names = ', '.join(f'{name} {n}' for name, n in called_under[address].most_common(5))
        print(f'  {address - exe_base:08x}  {100.0 * number / count:5.1f}%  {names}')


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    profile = Path(sys.argv[1])
    dll = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    frames = Path(str(profile) + '.frames.tsv')
    modules = Path(str(profile) + '.exe.tsv')
    stacks = Path(str(profile) + '.stacks.bin')
    if frames.is_file():
        frames_report(frames)
    if modules.is_file():
        print()
        print(modules.read_text().split('\ndll_scope_open')[0].strip())
    if stacks.is_file():
        stacks_report(stacks, dll)


if __name__ == '__main__':
    main()
