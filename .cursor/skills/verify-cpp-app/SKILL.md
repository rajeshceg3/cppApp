# Verify cppApp

C++ Twilio SMS command-line project. Build and verification are offline by design - no credentials, no network, no production transport.

## Launch
`make` builds only the offline target `build/sms_app_offline`.

## Doctor
`python3 .cursor/skills/verify-cpp-app/verify.py`

## Drive
Runs `make verify` (offline build plus tests/verify_offline.py) and `./run_e2e_tests.sh` (26 independent synthetic scenarios asserting zero network syscalls via strace).

## Evidence
`.cursor/skills/verify-cpp-app/evidence.json`.

## Cleanup
Build artifacts stay under `build/`; `make clean` removes the offline binary.

## Known boundaries
The real Twilio send path is intentionally unverifiable offline and out of scope.
