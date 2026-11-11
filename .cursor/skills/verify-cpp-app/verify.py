#!/usr/bin/env python3
"""Verification runner for cppApp. See SKILL.md."""
import json, os, subprocess, sys, time
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
failures = []

def check(name, ok, detail=''):
    print(('PASS' if ok else 'FAIL'), name, (':: ' + str(detail))[:400] if detail else '')
    if not ok:
        failures.append(name)

def run(cmd, **kw):
    return subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=600, **kw)

def main():
    proc = run(['make', 'verify'])
    tail = proc.stdout + proc.stderr
    check('make verify (offline build + verify_offline.py)', proc.returncode == 0 and 'zero network syscalls' in tail, tail[-300:])
    proc = run(['./run_e2e_tests.sh'])
    tail = proc.stdout + proc.stderr
    check('run_e2e_tests.sh (26 independent scenarios)', proc.returncode == 0 and '26 independent scenarios passed' in tail, tail[-300:])
    print('FAILURES=%d' % len(failures))
    evidence = {'repo': 'cppApp', 'ts': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'failures': failures}
    with open(REPO / '.cursor/skills/verify-cpp-app/evidence.json', 'w') as f:
        json.dump(evidence, f, indent=2)
    return 1 if failures else 0

if __name__ == '__main__':
    sys.exit(main())
