# Offline/build safety verification

Base: 757dac662b57b16af69d2ac9b71f3d5a6b54d905.
Verified locally on November 5, 2026. This is a bounded offline repair, not live SMS acceptance.

## Actual checks

- Make C++11 build with `-Wall -Wextra -Wpedantic`: passes without warnings.
- CMake default build: passes.
- Both built executables: 26 independent scenario checks pass.
- `./run_e2e_tests.sh`: same 26 checks pass.
- Before any scenario: undefined-symbol/dependency inspection finds no curl or
  network function and no production Twilio endpoint in the offline binary.
- Every scenario runs under a network syscall trace with a five-second process
  timeout. No network syscalls were observed in any checked invocation.
- Secret-output sentinels absent; owner-only new file creation checked; existing
  config and symlink target remain unchanged when save is refused.

## Scope changes

The collided fmt skeleton is removed from the SMS source. Make and CMake default
to an offline target without libcurl. The optional production target is separate.
Unknown/missing/REAL scenario modes fail closed. The legacy committed executable is preserved byte-for-byte and excluded from
all verification. The old fixture harness is replaced by independent synthetic
cases. Explicit production builds force recompilation; clean preserves the old
tracked artifact. Existing old scenario files remain as history and are not presented as
passing tests. Failure exit codes, current-working-directory configuration,
redacted token/API output and safe new configuration-file creation are covered.

PR #3's receive/webhook branch is not changed, merged, deleted or exercised.
JapaneseVillage is not changed. No SMS, live Twilio API request, server exposure,
account-setting change, deployment or payment was performed.

## Remaining gates

- Production libcurl compile/link was not tested: development headers are absent
  in this local environment. No fake header was used to claim success.
- GitHub Actions workflow is updated but has not run. Local results are not CI.
- No main-branch integration or public release is implied by these results.
- Optional saved credentials remain plaintext, now owner-only and non-overwriting.
  Further credential-store policy and safe overwrite UX can be separate work.
- Current file operations are POSIX/Linux; Windows support is not verified.
- Empty message behavior retains a warning-and-continue policy, not new rejection.
- No external service, message delivery, receipt, billing or webhook verification.
