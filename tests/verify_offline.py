#!/usr/bin/env python3
"""Synthetic, independent offline cases. Never run the tracked legacy binary."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
# Prove transport exclusion BEFORE running any scenario.
symbols = subprocess.check_output(['nm', '-u', str(binary)], text=True)
links = subprocess.check_output(['readelf', '-d', str(binary)], text=True)
for forbidden in ['curl_', 'socket', 'connect', 'getaddrinfo', 'sendto', 'sendmsg']:
    assert forbidden not in symbols, f'Forbidden transport symbol: {forbidden}'
assert 'libcurl' not in links
for library in re.findall(r'Shared library: \[(.*?)\]', links):
    assert library.startswith(('libstdc++.', 'libgcc_s.', 'libc.', 'libm.', 'ld-linux')), ('Unexpected linked library', library)
assert b'api.twilio.com' not in binary.read_bytes(), 'Production endpoint in offline binary'
print('PASS transport exclusion: no curl/network symbols, curl dependency or Twilio endpoint')
strace = shutil.which('strace')
assert strace, 'strace required for zero-network syscall proof'
# Check tracer availability before launching any scenario.
subprocess.run([strace, '-qq', '-e', 'trace=network', '-o', '/dev/null', '/bin/true'], check=True)
SID = 'AC' + '0' * 32
TOKEN = 'synthetic_TOKEN_never_use_123'
FROM = '+12025550101'
TO = '+12025550102'
MSG = 'synthetic message'
passed = []

def lines(mock='SUCCESS', save='N', recipient=TO, message=MSG):
    directives = [] if mock is None else ['MOCK_SEND_SMS=' + mock]
    return [SID, TOKEN, FROM] + directives + [recipient, message, save]

def run(name, body=None, code=0, contains=(), config=None, args=None, verify=None, setup=None):
    with tempfile.TemporaryDirectory(prefix='cppapp-offline-') as td:
        root = Path(td)
        if config is not None:
            (root / 'config.txt').write_text(config)
        if setup:
            setup(root)
        scenario = root / 'case.scenario'
        if body is not None:
            scenario.write_text('\n'.join(body) + '\n')
        cmdargs = args if args is not None else ['--test-mode', str(scenario)]
        trace = root / 'network.trace'
        result = subprocess.run([strace, '-qq', '-f', '-e', 'trace=network', '-o', str(trace), str(binary)] + cmdargs,
                                cwd=root, text=True, capture_output=True, timeout=5,
                                env={**os.environ, 'HOME': td})
        # Trace can contain process-exit/signal bookkeeping, but NO network calls.
        network = trace.read_text()
        assert not re.search(r'\b(socket|connect|accept\d*|bind|listen|send\w*|recv\w*|getsock\w*|setsock\w*|shutdown)\(', network), (name, network)
        output = result.stdout + result.stderr
        for ext in ['out', 'err']:
            p = root / ('case.' + ext)
            if p.exists():
                output += p.read_text()
        assert result.returncode == code, (name, result.returncode, output)
        for text in contains:
            assert text in output, (name, text, output)
        assert TOKEN not in output, (name, 'token leaked')
        assert 'private-response-sentinel' not in output, (name, 'response leaked')
        if verify:
            verify(root, output)
        passed.append(name)
        print('PASS', name, '(zero network syscalls)')

run('mock_success', lines(), contains=['MOCK_SUCCESS: simulated queue only'])
run('auth_failure', lines('AUTH_FAIL'), code=1, contains=['HTTP 401'])
run('recipient_api_failure', lines('INVALID_TO_FAIL'), code=1, contains=['HTTP 400'])
run('encoding_failure', lines('URL_ENCODE_FAIL'), code=1, contains=['HTTP 400'])
run('connection_failure', lines('PERFORM_FAIL'), code=1, contains=['without an HTTP response'])
run('missing_mock_denied', lines(None), code=1, contains=['explicit valid mock required'])
run('real_mock_denied', lines('REAL'), code=1, contains=['REAL is forbidden'])
run('unknown_mock_denied', lines('MOCK_SUCCESS'), code=1, contains=['Unknown mock directive value'])
run('real_then_success_still_denied', lines()[:-3] + ['MOCK_SEND_SMS=REAL', 'MOCK_SEND_SMS=SUCCESS', TO, MSG, 'N'], code=1, contains=['explicit valid mock required'])
run('unknown_directive_denied', lines()[:-3] + ['MOCK_ARBITRARY=bad', TO, MSG, 'N'], code=1, contains=['Unknown mock directive'])
run('invalid_status_denied', lines()[:-3] + ['MOCK_RESPONSE_CODE=201junk', TO, MSG, 'N'], code=1, contains=['Invalid mock response code'])
run('response_redacted', lines()[:-3] + ['MOCK_API_RESPONSE=private-response-sentinel', TO, MSG, 'N'], contains=['content redacted'])
run('negative_status_denied', lines()[:-3] + ['MOCK_RESPONSE_CODE=-1', TO, MSG, 'N'], code=1, contains=['Invalid mock response code'])
run('eof', [SID], code=1, contains=['EOF'])
run('invalid_phone_reprompt', lines()[:-3] + ['not-a-phone', TO, MSG, 'N'], contains=['Invalid recipient'])
run('empty_sid_reprompt', [''] + lines(), contains=['Account SID cannot be empty'])
run('empty_message_current_policy', lines(message=''), contains=['Message body is empty'])

def saved(root, output):
    p = root / 'config.txt'
    assert (p.stat().st_mode & 0o777) == 0o600
    assert 'AUTH_TOKEN=' + TOKEN in p.read_text()
run('save_owner_only', lines(save='Y'), verify=saved)
config = f'ACCOUNT_SID={SID}\nAUTH_TOKEN={TOKEN}\nFROM_NUMBER={FROM}\n'
run('independent_loaded_config', ['Y', '', '', '', 'MOCK_SEND_SMS=SUCCESS', TO, MSG, 'N'], config=config, contains=['Using loaded credentials'])
run('malformed_config_recovery', lines(), config='AUTH_TOKEN_NO_EQUALS\n', contains=['MOCK_SUCCESS:'])
run('incomplete_config_recovery', lines(), config=f'ACCOUNT_SID={SID}\n', contains=['incomplete'])
run('save_existing_file_denied', ['N'] + lines(save='Y'), config=config, code=1, contains=['Cannot create configuration safely'], verify=lambda root, output: (None if (root / 'config.txt').read_text() == config else (_ for _ in ()).throw(AssertionError('config overwritten'))))
def symlink_setup(root):
    (root / 'target.txt').write_text('do-not-overwrite')
    (root / 'config.txt').symlink_to(root / 'target.txt')

def symlink_check(root, output):
    assert (root / 'target.txt').read_text() == 'do-not-overwrite'

run('save_symlink_denied', lines(save='Y'), code=1, contains=['Cannot create configuration safely'], setup=symlink_setup, verify=symlink_check)
run('no_args_denied', args=[], code=1, contains=['offline binary requires'])
run('live_send_denied', args=['--send'], code=1, contains=['offline binary requires'])
run('missing_scenario_denied', args=['--test-mode', '/nonexistent/cppapp-case'], code=1, contains=['Failed to open scenario'])
print(f'{len(passed)} independent scenarios passed; zero network syscalls in every run.')
