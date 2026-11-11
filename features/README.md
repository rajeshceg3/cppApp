# Feature map - cppApp

| Feature | Entry point | Proven by |
| --- | --- | --- |
| Offline-safe SMS CLI (config parse, validation, denial paths) | `src/`, `build/sms_app_offline` | `make verify` + `run_e2e_tests.sh` (26 scenarios, zero network syscalls) |

The automated proof lives in `.cursor/skills/verify-cpp-app/`.
