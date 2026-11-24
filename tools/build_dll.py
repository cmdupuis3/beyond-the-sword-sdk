#!/usr/bin/env python3
"""Build the serial SDK and account for its stock PE32 imports and exports."""
import argparse
import collections
import datetime
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
import build_support as support
import eh_metadata
import pefile

ROOT = support.ROOT
LOCK = json.loads((ROOT / 'tools/link-lock.json').read_text())
DEFAULT_PROFILE = ROOT / 'pgo/CvGameCoreDLL.profdata'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['bts-dir', 'llvm-bin']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--deps-dir', type=Path, default=ROOT / 'tmp/dependencies',
                        help='dependencies prepared by prepare_dependencies.py')
    for name in ['vc71', 'windows-sdk']:
        parser.add_argument('--' + name, type=Path, help='override the prepared dependency path')
    parser.add_argument('--icu70-dir', type=Path, help='required for the pinned Linux LLVM package')
    parser.add_argument('--output-dir', type=Path,
                        help='default tmp/dll-build, with -no-pgo, -pgo-train or (for --pgo-use) -pgo '
                             'added to the name, then -tier1 or -tier3 with --tier, and then '
                             '-profile with --profile')
    parser.add_argument('--tier', type=int, choices=[1, 2, 3], default=2,
                        help='how far the DLL may depart from the stock game (CvGameCoreDLL/BridgedTiers.h): '
                             '1 reproduces BtS (REPRODUCE_BTS); 2, the default, may play a different game '
                             'but always the same one; 3 may differ from run to run (NON_DETERMINISM_OK)')
    parser.add_argument('--profile', action='store_true',
                        help='time the SDK PROFILE scopes inside the DLL (see FProfiler.h)')
    parser.add_argument('--no-lto', dest='lto', action='store_false',
                        help='compile each SDK unit on its own, without link-time optimisation')
    pgo = parser.add_mutually_exclusive_group()
    pgo.add_argument('--pgo-train', action='store_true',
                     help='build the DLL that counts what runs, for run_bench.py --pgo-train')
    pgo.add_argument('--pgo-use', type=Path, metavar='PROFDATA',
                     help='optimise with this profile, merged by llvm-profdata from training runs, '
                          'instead of the one in the repository (pgo/CvGameCoreDLL.profdata)')
    pgo.add_argument('--no-pgo', action='store_true',
                     help='do not optimise with a profile (a --profile build does not anyway)')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--cmake', default='cmake')
    args = parser.parse_args()
    # The build is optimised with the profile kept in the repository unless told otherwise.
    # A --profile build is not: its timing scopes change the hottest functions, about 420
    # of them, so that profile does not fit them and they would be optimised without it.
    own_profile = args.pgo_use is not None
    if not (own_profile or args.pgo_train or args.no_pgo or args.profile):
        args.pgo_use = DEFAULT_PROFILE
    if args.pgo_use is not None and not args.pgo_use.is_file():
        parser.error(f'not found: {args.pgo_use} (--no-pgo builds without a profile)')
    if args.output_dir is None:
        name = 'tmp/dll-build'
        if args.pgo_train:
            name += '-pgo-train'
        elif own_profile:
            name += '-pgo'
        elif args.no_pgo and not args.profile:
            name += '-no-pgo'
        if args.tier != 2:
            name += f'-tier{args.tier}'
        args.output_dir = ROOT / (name + ('-profile' if args.profile else ''))
    tier_macro = {1: 'REPRODUCE_BTS', 3: 'NON_DETERMINISM_OK'}.get(args.tier)
    for name, path in support.dependency_paths(args.deps_dir).items():
        if getattr(args, name) is None:
            setattr(args, name, path)
    windows_host = sys.platform == 'win32'
    if not windows_host and args.icu70_dir is None:
        parser.error('--icu70-dir is required on Linux')
    if args.jobs < 1:
        parser.error('jobs must be positive')
    try:
        output = support.output_directory(args.output_dir)
    except ValueError as error:
        parser.error(str(error))
    report_path = output / 'report.json'
    attempt = output / 'attempts' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    attempt.mkdir(parents=True)
    if report_path.exists():
        (attempt / 'previous-report.json').write_bytes(report_path.read_bytes())
    report_path.unlink(missing_ok=True)
    commands = []
    report = {'pass': False, 's3_complete': False, 'scope': 'Serial SDK build and stock symbol coverage only',
              'commands': commands, 'runtime_tested': False}
    try:
        for name in support.dependency_paths():
            if not getattr(args, name).exists():
                raise RuntimeError(f'missing {name.replace("_", "-")}: {getattr(args, name)}; '
                                   'run tools/prepare_dependencies.py or pass an explicit path')
        if pefile.__version__ != support.LOCK['pefile_version']:
            raise RuntimeError('pinned pefile version differs')
        tool_pins = json.loads((ROOT / 'tools/windows-toolchain-lock.json').read_text())['tools'] if windows_host else {
            **support.LOCK['tools'], **LOCK['tools']}
        for name, expected in tool_pins.items():
            if support.digest(args.llvm_bin / name) != expected:
                raise RuntimeError(f'pinned LLVM tool differs: {name}')
        if not windows_host:
            for name, expected in support.LOCK['icu70'].items():
                if support.digest(args.icu70_dir / name) != expected:
                    raise RuntimeError(f'pinned linker dependency differs: {name}')
        library_paths = {'source/python24.lib': ROOT / 'CvGameCoreDLL/Python24/libs/python24.lib',
                         'source/boost_python-vc71-mt-1_32.lib': ROOT / 'CvGameCoreDLL/Boost-1.32.0/libs/boost_python-vc71-mt-1_32.lib'}
        for name, expected in LOCK['link_inputs'].items():
            path = library_paths.get(name) if name.startswith('source/') else args.windows_sdk / 'lib' / name.split('/', 1)[1]
            if support.digest(path) != expected:
                raise RuntimeError(f'pinned import library differs: {name}')
        for name, pin in support.LOCK['installed_images'].items():
            if support.digest(args.bts_dir / name) != pin['sha256']:
                raise RuntimeError(f'pinned original differs: {name}')
        env = dict(os.environ)
        if not windows_host:
            env['LD_LIBRARY_PATH'] = str(args.icu70_dir.resolve())
        configure = [args.cmake, '-S', ROOT, '-B', output, '-G', 'Ninja',
                     '-DCMAKE_TOOLCHAIN_FILE=' + (ROOT / 'cmake/windows-x86-clang.cmake').as_posix(),
                     '-DCMAKE_BUILD_TYPE=Release', '-DPython3_EXECUTABLE=' + Path(sys.executable).as_posix(),
                     '-DBRIDGED_PROFILE=' + ('ON' if args.profile else 'OFF'),
                     '-DBRIDGED_TIER=' + str(args.tier),
                     '-DBRIDGED_LTO=' + ('ON' if args.lto else 'OFF'),
                     '-DBRIDGED_PGO=' + ('TRAIN' if args.pgo_train else 'USE' if args.pgo_use else 'OFF'),
                     '-DBRIDGED_PGO_PROFILE=' + (args.pgo_use.resolve().as_posix() if args.pgo_use else ''),
                     '-DBRIDGED_LLVM_BIN=' + args.llvm_bin.resolve().as_posix(),
                     '-DBRIDGED_VC71=' + args.vc71.resolve().as_posix(),
                     '-DBRIDGED_WINDOWS_SDK=' + args.windows_sdk.resolve().as_posix()]
        for command, name in [(configure, 'configure.log'),
                              ([args.cmake, '--build', output, '--parallel', str(args.jobs)], 'build.log')]:
            command = [str(item) for item in command]
            commands.append(command)
            with (attempt / name).open('w') as log:
                subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        binary = output / 'CvGameCoreDLL.dll'
        pe, image = support.pe_summary(binary)
        if (image['machine'], image['magic']) != (0x14c, 0x10b):
            raise RuntimeError('SDK output is not PE32/i386')
        original_exe = pefile.PE(str(args.bts_dir / 'Civ4BeyondSword.exe'))
        required = {item.name.decode() for entry in original_exe.DIRECTORY_ENTRY_DELAY_IMPORT
                    if entry.dll.lower() == b'cvgamecoredll.dll' for item in entry.imports if item.name}
        missing = sorted(required - set(image['exports']))
        if missing:
            raise RuntimeError(f'missing stock exports: {missing}')
        _, stock = support.pe_summary(args.bts_dir / 'Assets/CvGameCoreDLL.dll')
        missing_public = sorted(set(stock['exports']) - set(image['exports']))
        if missing_public:
            raise RuntimeError(f'missing original public exports: {missing_public}')
        providers = {path.name.lower(): path for path in args.bts_dir.iterdir() if path.is_file()}
        system = {'kernel32.dll', 'user32.dll', 'winmm.dll'}
        provider_hashes = {}
        provider_exports = {}
        for entry in image['imports']:
            name = entry['dll'].lower()
            if (name.startswith(('vcruntime', 'ucrtbase', 'api-ms-win-crt-')) or
                    (name.startswith(('msvcr', 'msvcp')) and name not in {'msvcr71.dll', 'msvcp71.dll'})):
                raise RuntimeError(f'supplemental modern CRT import is forbidden: {entry}')
            if name in system:
                continue
            if name not in providers:
                raise RuntimeError(f'unaccounted runtime provider: {name}')
            path = providers[name]
            if name not in provider_exports:
                _, target = support.pe_summary(path)
                provider_hashes[name] = target['sha256']
                provider_exports[name] = set(target['exports'])
            if entry['name'] not in provider_exports[name]:
                raise RuntimeError(f'import missing from pinned provider: {entry}')
        legacy_eh_names = {'__CxxFrameHandler', '_CxxThrowException', '?terminate@@YAXXZ'}
        legacy_eh_imports = [entry for entry in image['imports'] if entry['name'] in legacy_eh_names]
        missing_legacy_eh = legacy_eh_names - {entry['name'] for entry in legacy_eh_imports}
        if missing_legacy_eh:
            raise RuntimeError(f'missing legacy EH imports: {sorted(missing_legacy_eh)}')
        for entry in legacy_eh_imports:
            if entry['dll'].lower() != 'msvcr71.dll':
                raise RuntimeError(f'legacy EH import has unexpected provider: {entry}')
        database = json.loads((output / 'compile_commands.json').read_text())
        expected_sources = {path.resolve() for path in (ROOT / 'CvGameCoreDLL').glob('*.cpp')}
        adapter_sources = {ROOT / 'compat/msvc_eh.cpp', ROOT / 'compat/msvc_eh_x86.s'}
        adapter_sources = {path.resolve() for path in adapter_sources}
        rows_by_source = {Path(row['file']).resolve(): row for row in database}
        if len(rows_by_source) != len(database) or set(rows_by_source) != expected_sources | adapter_sources:
            raise RuntimeError('compile inventory differs from inherited Makefile direct sources')
        # the 99 units of the inherited SDK, KmodPathFinder.cpp, CvInfoTables.cpp and BridgedThreads.cpp
        if len(expected_sources) != 102:
            raise RuntimeError(f'inherited SDK inventory changed: {len(expected_sources)} units')
        exception_metadata = {}
        for source, row in rows_by_source.items():
            flags = row.get('arguments') or shlex.split(row['command'], posix=not windows_host)
            compiler = args.llvm_bin / ('clang++.exe' if windows_host else 'clang++')
            if Path(flags[0].strip('"')).resolve() != compiler.resolve():
                raise RuntimeError(f'compile unit uses an unpinned driver: {source}: {flags[0]}; '
                                   'configure a fresh build directory with --output-dir')
            if source in expected_sources:
                required_flags = {'-std=c++23', '-O2', '-D_DLL', '-D_MT',
                                  '--target=i686-pc-windows-msvc', '-fms-compatibility-version=13.10',
                                  '-fno-sized-deallocation', '-fno-threadsafe-statics', '-fcheck-new',
                                  '-fno-wchar', '-fno-strict-aliasing', '-ffp-contract=off', '-Wall', '-Wextra'}
                if not required_flags.issubset(flags) or any(flag in flags for flag in ['-O3', '-ffast-math']):
                    raise RuntimeError('an SDK unit does not use the required target/language/helper configuration')
                tier_flags = {flag[2:] for flag in flags if flag in ('-DREPRODUCE_BTS', '-DNON_DETERMINISM_OK')}
                if tier_flags != ({tier_macro} if tier_macro else set()):
                    raise RuntimeError(f'an SDK unit was not compiled for tier {args.tier}: {source}; '
                                       'configure a fresh build directory with --output-dir')
                if ('-flto' in flags) != args.lto:
                    raise RuntimeError(f'an SDK unit was not compiled for the requested link-time optimisation: '
                                       f'{source}; configure a fresh build directory with --output-dir')
            elif source.suffix == '.cpp':
                required_flags = {'-std=c++23', '-pedantic-errors', '-Wall', '-Wextra', '-Werror',
                                  '-ffreestanding', '-fno-builtin', '-fno-exceptions', '-fno-rtti',
                                  '-fno-stack-protector', '-fms-extensions', '--target=i686-pc-windows-msvc'}
                if not required_flags.issubset(flags):
                    raise RuntimeError('EH adapter C++ unit lacks strict configuration')
            elif '--target=i686-pc-windows-msvc' not in flags:
                raise RuntimeError('EH adapter assembly lacks the i686 MSVC target')
            obj = Path(row['output'])
            if not obj.is_absolute():
                obj = Path(row['directory']) / obj
            # with link-time optimisation an SDK unit is LLVM bitcode until the link
            lto_unit = args.lto and source in expected_sources
            magic = b'BC\xc0\xde' if lto_unit else b'\x4c\x01'
            if not obj.exists() or obj.read_bytes()[:len(magic)] != magic:
                raise RuntimeError(f'missing/unexpected object: {obj}')
            if source in expected_sources and not lto_unit:
                exception_metadata[str(source.relative_to(ROOT))] = eh_metadata.inspect_object(obj)
        if args.lto:
            # the one object the linker made of the SDK units, kept by /lldsavetemps:prelink
            lto_object = output / 'CvGameCoreDLL.dll.lto.obj'
            # A training build has a counter section for every function, which is more
            # sections than a plain COFF object can hold, so its object is a "big object"
            # (0000 FFFF, then the machine at offset 6). eh_metadata.py does not read that
            # form. The training DLL is only ever run to collect a profile, and the DLL
            # built from the profile is checked as usual.
            magic = lto_object.read_bytes()[:8] if lto_object.exists() else b''
            big_object = args.pgo_train and magic[:4] == b'\x00\x00\xff\xff' and magic[6:8] == b'\x4c\x01'
            if magic[:2] != b'\x4c\x01' and not big_object:
                raise RuntimeError(f'missing/non-i386 link-time object: {lto_object}')
            if lto_object.stat().st_mtime < binary.stat().st_mtime - 600:
                raise RuntimeError(f'link-time object is older than the DLL: {lto_object}')
            if not big_object:
                exception_metadata[lto_object.name] = eh_metadata.inspect_object(lto_object)
        log = (attempt / 'build.log').read_text()
        # Functions whose shape has changed since the profile was collected. The compiler
        # says so as it compiles a unit, so this counts the units compiled this time only.
        mismatched = len(re.findall(r'\(hash mismatch\)', log))
        libraries = [args.vc71 / 'lib' / name for name in ['msvcrt.lib', 'msvcprt.lib', 'oldnames.lib']]
        libraries += [args.windows_sdk / 'lib' / name for name in ['Kernel32.Lib', 'User32.Lib', 'WinMM.Lib']]
        libraries += [ROOT / 'CvGameCoreDLL/Python24/libs/python24.lib',
                      ROOT / 'CvGameCoreDLL/Boost-1.32.0/libs/boost_python-vc71-mt-1_32.lib']
        report.update(host=sys.platform, tool_sha256=tool_pins,
                      binary=image, required_exports=len(required), missing_exports=missing,
                      original_public_exports=len(stock['exports']), missing_public_exports=missing_public,
                      added_public_exports=sorted(set(image['exports']) - set(stock['exports'])),
                      units=len(expected_sources), adapter_units=len(adapter_sources),
                      tier=args.tier, tier_macro=tier_macro,
                      link_time_optimisation=args.lto,
                      profile_guided_optimisation=('train' if args.pgo_train else 'use' if args.pgo_use else 'off'),
                      pgo_profile=(str(args.pgo_use) if args.pgo_use else None),
                      pgo_profile_sha256=(support.digest(args.pgo_use) if args.pgo_use else None),
                      pgo_mismatched_functions=mismatched,
                      exception_route='Static Clang EH3 to VC7.1 frame adapter; no supplemental CRT DLL',
                      legacy_eh_imports=legacy_eh_imports, provider_hashes=provider_hashes,
                      exception_metadata=exception_metadata,
                      exception_funcinfo_count=sum(row['funcinfo_count'] for row in exception_metadata.values()),
                      maximum_catch_clauses=max((row['max_catch_clauses'] for row in exception_metadata.values()),
                                                default=0),
                      exception_metadata_checked=bool(exception_metadata),
                      link_library_sha256={str(path): support.digest(path) for path in libraries},
                      logs={name: {'path': str(attempt / name), 'sha256': support.digest(attempt / name)}
                            for name in ['configure.log', 'build.log']},
                      compiled_units_this_attempt=len(re.findall(r'Building CXX object .*CvGameCoreDLL', log)),
                      compiled_adapter_units_this_attempt=len(re.findall(r'Building (?:CXX|ASM) object .*msvc_eh', log)),
                      warning_scope='This invocation only; retained earlier logs cover earlier compilations',
                      compile_database_sha256=support.digest(output / 'compile_commands.json'),
                      header_inputs_sha256=support.digest(output / 'compat-headers/inputs.json'),
                      generated_header_sha256={str(output / 'compat-headers' / name):
                                              support.digest(output / 'compat-headers' / name)
                                              for name in ['overlay.json', 'sal.h', 'vc71-xstring']},
                      warnings=dict(collections.Counter(re.findall(r'\[(-W[^\]]+)\]', log))),
                      source_sha256={str(path): support.digest(path) for path in
                          [*(ROOT / 'CvGameCoreDLL').glob('*.cpp'), *(ROOT / 'CvGameCoreDLL').glob('*.h'),
                           ROOT / 'CMakeLists.txt', ROOT / 'cmake/windows-x86-clang.cmake',
                           ROOT / 'compat/vc71_prelude.h', ROOT / 'compat/msvc_eh_abi.h',
                           ROOT / 'compat/msvc_eh.cpp', ROOT / 'compat/msvc_eh_x86.s',
                           ROOT / 'tools/prepare_headers.py',
                           ROOT / 'tools/link-lock.json', ROOT / 'tools/dependency-lock.json',
                           ROOT / 'tools/windows-toolchain-lock.json',
                           ROOT / 'tools/build_support.py', ROOT / 'tools/eh_metadata.py', Path(__file__)]})
        report['pass'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        support.write_json(report_path, report)
    print(json.dumps({'build_pass': report['pass'], 's3_complete': False,
                      'tier': report['tier'],
                      'units': report['units'], 'required_exports': report['required_exports'],
                      'profile_guided_optimisation': report['profile_guided_optimisation'],
                      'pgo_mismatched_functions': report['pgo_mismatched_functions'],
                      'report': str(report_path)}, indent=2))
    if report['pgo_mismatched_functions']:
        print(f"{report['pgo_mismatched_functions']} functions have changed since the profile was collected "
              'and were optimised without it. Collect a new profile after larger changes: see '
              '"Profile-guided optimisation" in README.md.', file=sys.stderr)


if __name__ == '__main__':
    main()
