# Performance and validation

The original report records 100% correctness on four public coursework cases. A fresh deterministic synthetic suite also recovers every tested noiseless message, while retaining failures on some corrupted inputs. These are separate sets of evidence: the official evaluator and public fixtures are unavailable in this repository.

## Historical public results

The [submitted report](report/report.pdf), section 5, records:

| Case | Reported correctness | Reported runtime | Reported average encoded/original ratio |
| --- | --- | --- | --- |
| public1 | 100.00% | 612.8 ms | 1.5000 |
| public2 | 100.00% | 3267.8 ms | 1.6250 |
| public3 | 100.00% | 17528.7 ms | 1.9500 |
| public4 | 100.00% | 50897.7 ms | 1.9625 |

Under the [assignment's definitions](spec/assignment.pdf), correctness is the fraction of ages T=1..Tmax that recover the message exactly. Runtime is the total of encoder and decoder calls over that age range. Redundancy is the average encoded/original byte ratio over those ages. The table therefore gives case totals, not individual-call latency or throughput. The assignment separately limits each encode/decode call to one second.

These values are preserved historical claims, not a fresh reproduction. The provided archive does not include the evaluator, public input/meta files, raw grading logs, benchmark machine details, compiler version, evaluator version, or an exact benchmarked source hash. Archival file hashes establish copy identity; they do not prove that those exact source bytes produced the historical table. No hidden-case score or real NAND-device result is established here.

## Fresh synthetic suite

The release evidence was generated on 2026-11-11 in the Asia/Taipei timezone with `python3 scripts/run_tests.py`. It covers 353 deterministic trials against the preserved default codec. Exact inputs, corrupted codewords, outputs, seeds, flip positions, source hashes, and build context are retained so that failures can be inspected.

| Channel | Recovered / trials | Exact recovery |
| --- | --- | --- |
| No corruption | 228 / 228 | 100% |
| One flipped bit | 25 / 25 | 100% |
| Distinct bit flips at approximately 1% | 25 / 25 | 100% |
| Two consecutive fully flipped bytes | 20 / 25 | 80% |
| Eight flipped bits within a 32-bit window | 23 / 25 | 92% |
| Illustrative aging-intensity IID flips | 24 / 25 | 96% |

Every trial met the N<=encoded bytes<=2N length check and emitted valid hexadecimal output with the required decoded length. All 228 noiseless cases were re-encoded to check determinism. The observed encoded/original ratio ranged from 1.5 to 2.0.

The noiseless set comprises:

* 144 hashed-message cases: N in {1, 2, 3, 7, 8, 16, 32, 64, 127, 128, 129, 255, 256, 257, 511, 512}, crossed with T in {1, 50, 51, 100, 101, 200, 201, 999, 1000}.
* 84 all-zero/all-one cases: both message families at N in {1, 8, 128, 129, 256, 257, 512}, crossed with T in {1, 100, 101, 200, 201, 1000}.

Each corrupted channel uses the same 25 size/age pairs: N in {8, 64, 128, 256, 512}, crossed with T in {1, 51, 101, 201, 1000}. Hashed messages are derived deterministically from N; a deterministic seed derived from channel, N, and T selects corruption positions.

Channel definitions in [run_tests.py](../scripts/run_tests.py) are intentionally explicit:

| Channel | Local corruption rule |
| --- | --- |
| `single_bit` | Flip one uniformly selected encoded bit. |
| `random_1pct` | Select max(1, floor(encoded bits/100)) distinct positions and flip them. |
| `burst_2bytes` | Select two consecutive encoded bytes and invert all 16 bits. |
| `cluster_8of32` | Select a 32-bit window, then flip eight distinct positions within it. |
| `reference_intensity_iid` | Independently flip each encoded bit with probability 0.06 sqrt(T/1000). |

The last rule borrows the assignment's reference intensity as an illustrative IID probability. The official specification explicitly says its severity measure need not be a uniform independent bit-flip probability and permits bounded variation and mixed structured errors. None of these local channels reproduces the unavailable official Flash channel.

## Recorded failures and coverage limits

The preserved codec did not recover these synthetic trials:

| Channel | Failed N, T pairs |
| --- | --- |
| `burst_2bytes` | (8, 1), (8, 51), (8, 101), (8, 201), (64, 201) |
| `cluster_8of32` | (8, 1), (8, 51) |
| `reference_intensity_iid` | (128, 1000) |

These failures are retained in the evidence; the codec and comparisons were not changed to remove them. Small codewords can be particularly exposed to a fixed-size burst, but this suite does not establish a general failure boundary or a guaranteed correction count. The 228 noiseless successes sample selected messages, sizes, and ages rather than exhaustively proving invertibility for every message.

## Fresh timing observations

The release run used GCC 8.5.0, Python 3.14.7, and an x86_64 machine. The original Makefile supplied `-O2 -std=c++17 -Wall -Wextra -Iinclude`. Measurements cover sequential command-line calls, including process startup, file I/O, and Python subprocess wait polling; they are not isolated codec-kernel timings.

| Mode | Calls | Median observed wall time | Maximum observed wall time |
| --- | --- | --- | --- |
| Encode, including noiseless repeats | 581 | 4.444 ms | 18.668 ms |
| Decode | 353 | 16.442 ms | 115.387 ms |

Each call was subject to a one-second timeout. All completed calls in this run remained below it. These machine-specific observations do not establish a portable worst-case bound, and their scope differs from the historical totals across T.

The [architecture](ARCHITECTURE.md) explains decoding cost: the dominant trellis work depends on N and K. At fixed N, raising redundancy does not increase the number of trellis states or steps. Without the original public-case metadata and environment, the historical differences in total runtime cannot be attributed to redundancy alone.

## Evidence and reproduction

The committed release evidence contains:

* [summary.json](evidence/release/summary.json): recovery counts, length/determinism checks, and timing aggregates.
* [trials.csv](evidence/release/trials.csv): one row per trial.
* [trials.jsonl](evidence/release/trials.jsonl): full vectors, outputs, flip positions, and seeds, including failures.
* [run.json](evidence/release/run.json): environment, source/helper hashes, binary hash, and vector-file hash.
* [build.log](evidence/release/build.log): compiler commands from the clean isolated build.

With a C++17 compiler, Make, and Python 3.9 or newer, run from the repository root:

```sh
python3 scripts/check_project.py
python3 scripts/run_tests.py
```

The project checker verifies original-asset identity, packaged source hashes, local documentation links, and helper syntax. The codec runner copies the preserved source and Makefile into a new ignored `.build/<timestamp>/project` directory, builds there, and writes fresh evidence alongside it. It checks source hashes again after execution. This keeps compilation artifacts separate from the tracked source.

The codec runner treats malformed output, timeout, process failure, cap violations, and non-determinism as errors. It also exits unsuccessfully if a noiseless case fails. Corrupted-channel recovery is measured and reported separately; an overall successful runner exit does not mean every corrupted message recovered. Timings and environment details will vary across reruns.
