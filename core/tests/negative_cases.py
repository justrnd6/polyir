#!/usr/bin/env python3
"""Negative programs and their recorded answers: every tests/data/negative/*.poly is run with the flags below, and polyir-check must print
the answer recorded in its .expect (stdout, exit status) -- warnings, error blocks of every headline (compile / check / lint / kernel check), `ok`.
  tests/negative_cases.py <build dir>            compare polyir-check with the recorded answers (tests/data/negative/*.expect)
The answers were first recorded from the Nina CLI (each `$` line names its flag); a changed answer is written into its .expect after it
is read. The Metal of a kernel program that passes is compared too (C++ writes it, src/KernelMsl.cpp)."""
import glob, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, 'data', 'negative')
MODES = [(['--analyze'], ['--analyze']), (['--analyze', '--strict'], ['--analyze', '--strict']), (['--emit-hir-json'], ['--json']), (['--emit-msl'], ['--emit-msl'])]
BUILD_MODE = (['--check'], ['--check'])  # only for the programs named native_*: what a native build refuses


def warnings_of(out):
    lines = out.splitlines(True)
    if not lines or lines[0] != 'warning:\n': return ''
    block = [lines[0]] + [l for l in lines[1:] if l.startswith('  ')]
    return ''.join(block[:len(block)]) if len(block) > 1 else ''


def answer(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, errors='replace', cwd=DATA, timeout=120)
    out = r.stdout
    return 'exit %d\n%s' % (r.returncode, out)


def record(name, exe, index):
    modes = MODES + ([BUILD_MODE] if name.startswith('native_') else [])
    return ''.join('$ %s\n%s' % (' '.join(m[0]), answer([exe, name] + m[index])) for m in modes)


def main():
    build = sys.argv[1]
    names = sorted(os.path.basename(p) for p in glob.glob(os.path.join(DATA, '*.poly')))
    bad = 0
    for name in names:
        expect = os.path.join(DATA, name[:-5] + '.expect')
        got = record(name, os.path.join(build, 'polyir-check'), 1)
        want = open(expect).read() if os.path.exists(expect) else ''
        if got != want:
            bad += 1
            g, w = got.splitlines(), want.splitlines()
            first = next(((x, y) for x, y in zip(g + [''], w + ['']) if x != y), ('', ''))
            print('FAIL %s\n   want: %s\n   got:  %s' % (name, first[1][:160], first[0][:160]))
    print('%d programs, %d differ' % (len(names), bad))
    sys.exit(1 if bad else 0)


main()
