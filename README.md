# C++ SMS sender

This is a C++ Twilio SMS command-line project. Its default build and verification
are offline: they cannot send a message and do not link the production transport.

## Safe build and verification

Linux tools: C++11 compiler, Make or CMake, Python 3, binutils (`nm`, `readelf`)
and `strace`. No Twilio credentials, account, Conan download or network is needed.

```sh
make verify
# Or:
cmake -S . -B build-cmake
cmake --build build-cmake
python3 tests/verify_offline.py build-cmake/sms_app_offline
```

`make` builds only `build/sms_app_offline`. `./run_e2e_tests.sh` rebuilds that same
safe target and runs the independent synthetic cases. The verifier checks the
binary's symbols, dependencies and strings before scenarios run, then traces
every invocation to assert zero network syscalls. Every process has a five-second
timeout and an isolated temporary working directory. Missing tracer support is a
failed verification, not permission to skip the safety check.

Test mode requires `--test-mode <scenario-file>` and an explicit supported mock:
`SUCCESS`, `AUTH_FAIL`, `INVALID_TO_FAIL`, `URL_ENCODE_FAIL`, or `PERFORM_FAIL`.
`REAL`, missing mock selection, malformed status values and unknown directives
fail closed. Mock output says that it is a simulation, not a delivered SMS.
The old scenarios in `e2e_tests/` are historical fixtures, not a passing suite;
their `MOCK_SUCCESS`-style directive values are intentionally rejected.

The new tests cover independent configuration load/save and malformed input,
EOF, phone validation, mock API/transport failures, redaction, return codes and
explicit refusals of live commands. They do not test Twilio or prove delivery.
Empty message text retains its historical warning-and-continue behavior.

## Production build is separate

Only if live integration is separately approved, with libcurl development files:

```sh
make production
# Or cmake -S . -B build-production -DSMS_BUILD_PRODUCTION=ON
```

Do not run the production binary as a verification step. It requires explicit
`--send` and can spend money/send a real SMS. Approval of repository repair does
not approve a recipient, message, charge, Twilio setup or webhook exposure.
No real SMS was sent to verify this repair. Production compilation/linking and
Twilio integration remain separate acceptance gates.

Configuration is relative to the process working directory (`./config.txt`),
not the executable's folder. New configuration files use owner-only permissions
and refuse to overwrite an existing file or follow a symlink. Tokens are still
plaintext on disk if saving is selected, so use synthetic data during tests.
Saved configuration is optional. Token displays are fully masked and raw API
responses are not logged. Non-success send outcomes return a failure exit code.
The current owner-only file implementation targets POSIX/Linux.

## Preserved work

The separate receive/webhook proposal in PR #3 remains independent and unchanged.
This repair does not merge it, start a server, expose port 8080, configure Twilio,
or discard that work. Reconcile it against this safety boundary before merging.

## Historical executable preserved

The pre-existing tracked `build/sms_app` is preserved byte-for-byte for provenance.
It predates this repair, is not a verified output, and MUST NOT be run as a test.
The safe harness only uses a freshly built `sms_app_offline`; `make clean` also
preserves the historical file. Explicit `make production` forces recompilation
instead of treating that old executable as up to date. No old binary was run.
