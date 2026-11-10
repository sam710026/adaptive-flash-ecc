# Fresh synthetic-channel verification

Date: 2026-11-11 (Asia/Taipei).

```bash
python3 scripts/run_tests.py
```

The helper rebuilt a clean copy of the unchanged source and original Makefile.
There were 353 trials: all 228 noiseless round trips succeeded, and 117 of 125
noisy trials recovered exactly. All encoded lengths met the 2N byte cap; all
228 repeated encodes matched. The eight noisy failures are retained.

[Summary](summary.json), [per-trial metrics](trials.csv), and
[full vectors](trials.jsonl) provide the exact stimuli, flipped positions,
codewords and recovered data. [Metadata](run.json) binds source hashes, the
compiled binary identity and the trial-vector hash to this run.

Timing includes sequential process startup, file I/O and Python polling. All
934 observed encode/decode calls completed within their one-second timeout.
These times are not kernel-only benchmarks or the report's case-total runtime.
The metadata records the build compiler, Python version and architecture.

This is not the unavailable official Flash channel. It samples nine ages and
selected message sizes rather than evaluating every T for public1–public4.
The historical public 100% results remain separately attributed to the report.
Full build trees, working files and generated executables are excluded from Git.
