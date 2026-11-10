# Verification scope

Run `python3 scripts/run_tests.py`. It copies the preserved sources and Makefile
into a new `.build/` workspace and rebuilds there. It never executes the
precompiled binary supplied in the archive.

The 353 deterministic trials comprise:

- 144 hashed-message noiseless trials: 16 sizes and nine aging values.
- 84 zero/all-one-message noiseless trials at size and aging boundaries.
- 125 error-injection trials: five sizes, five ages, and five synthetic channels.

Each noiseless trial also checks repeated encoding. Every trial checks the hex
protocol, decoded size and encoded byte limit. Correctness means exact byte
equality with the original synthetic message. Each CLI call has a one-second
timeout; wrapper process groups are terminated on timeout. Source hashes are
captured before execution and checked again afterward.

The helper fails on malformed output, size/cap violations, timeouts,
nondeterminism or noiseless recovery failure. Noisy-channel failures are measured
outcomes, not silently dropped or treated as infrastructure passes. The summary
and full vectors retain all recoveries and failures.

The `reference_intensity_iid` channel chooses independent bit flips with
`p=0.06*sqrt(T/1000)` only as a defined synthetic stress test. The official
specification explicitly says that its severity function need not mean a uniform
independent bit-flip probability. This helper is not the missing official channel.

These tests cover selected ages between 1 and 1000 and messages between 1 and
512 bytes. They do not evaluate every age/message or replay public1–public4.
`python3 scripts/check_project.py` validates preserved input hashes, source
checksums, Python syntax and local documentation links without running the codec.
