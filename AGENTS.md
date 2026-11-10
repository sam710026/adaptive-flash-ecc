# Project rules

- The main agent supervises scope, verification and publication; delegate independent tasks when useful.
- Preserve submitted src/, include/ and Makefile unless implementation changes are explicitly requested.
- Keep portfolio test helpers separate from the original codec.
- Separate historical public-evaluator claims from fresh synthetic-channel experiments.
- Do not present synthetic error channels as the unavailable official Flash evaluator or hidden cases.
- Bind results to source/build/test hashes, seeds, input sizes and aging values.
- Use python3 scripts/check_project.py for packaging/docs and python3 scripts/run_tests.py for codec validation.
- Respect the 2N encoded-byte cap and the specification's one-second encode/decode limit; record observed failures without weakening comparisons.
- Never commit prebuilt executables, build outputs, credentials or local environments.
