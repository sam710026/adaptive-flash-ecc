# Architecture

This project preserves a C++17 coursework encoder and decoder for a Flash-inspired error channel. Its coding pipeline combines a terminated convolutional code, deterministic puncturing, interleaving, and hard-decision Viterbi decoding. The codec uses the C++ standard library and has no external library dependencies.

The official error channel is an evaluation dependency, not an implementation in this repository. The [assignment](spec/assignment.pdf) describes random, burst, retention, and disturb-like corruption. The portfolio's [synthetic experiments](PERFORMANCE.md) exercise selected patterns separately.

## Program boundary

The original [Makefile](../Makefile) builds `bin/solution` from three source files:

| File | Responsibility |
| --- | --- |
| [main.cpp](../src/main.cpp) | Parse mode and T, select encode/decode, and connect file I/O to the codec. |
| [utils.cpp](../src/utils.cpp) | Read the message length and hexadecimal bytes; write hexadecimal output. |
| [ecc.cpp](../src/ecc.cpp) | Select coding parameters, encode, puncture, interleave, and decode. |

```sh
make -B
./bin/solution encode T input.txt encoded.txt
./bin/solution decode T corrupted.txt decoded.txt
```

Both input modes take a first line containing the original byte length N and a second line containing hexadecimal data. Encoding receives N message bytes; decoding receives the corrupted codeword and the original N. Each output is one hexadecimal line. The decoder also needs the same T as the encoder. There is no metadata header inside the codeword.

The assignment's evaluated domain is N<=512 bytes and T=1..1000. T is an abstract storage-aging index, not elapsed physical time. The CLI accepts nonnegative T, but that acceptance does not extend the documented evaluation domain.

## Encoding and redundancy

Message size selects the default convolutional code:

| Message bytes N | Constraint length K | Register memory | States | Generator masks, octal |
| --- | --- | --- | --- | --- |
| N<=256 | 11 | 10 bits | 1024 | 02335, 03661 |
| N>256 | 12 | 11 bits | 2048 | 04335, 05723 |

Bits are read most-significant bit first within each byte. Starting from state zero, the encoder shifts each input bit into a K-bit register and computes two parity outputs with the generator masks. It then appends K-1 zero input bits, ending in state zero. The resulting rate-1/2 mother code has `F=2(8N+K-1)` parity bits before puncturing.

The default target codeword length B adapts to T:

| Message bytes | T range | Encoded bytes B |
| --- | --- | --- |
| 1<=N<=128 | T<=100 | ceil(1.5N) |
| 1<=N<=128 | 101<=T<=200 | ceil(1.75N) |
| 1<=N<=128 | T>=201 | 2N |
| N>128 | T<=50 | ceil(1.5N) |
| N>128 | 51<=T<=100 | ceil(1.75N) |
| N>128 | T>=101 | 2N |

The zero-length target is zero bytes. Whole-byte rounding means individual ratios can exceed 1.5 or 1.75, particularly for small N. The target is clamped between N and 2N bytes. Thresholds, target multipliers, and interleaver controls are compile-time macros in [ecc.cpp](../src/ecc.cpp); this table describes the preserved defaults.

To retain exactly L=8B output bits, the encoder removes P=F-L observations at zero-based raw parity positions `floor(jF/(P+1))`, for j=1..P. This spreads punctures across the mother-code stream. Even at the 2N-byte target, termination overhead requires puncturing `2(K-1)` observations. Zero-tail termination does not imply that every tail parity observation is retained.

## Deterministic interleaving

Retained logical parity bits are written directly into their physical positions in the output byte vector. The decoder reads those same physical positions as it traverses the logical trellis observations, avoiding a separate deinterleaved buffer.

For retained bit length L:

* When L>=4096 and L is a power of two, the default bit-reversal permutation maps i to `reverse_bits((i+offset)&(L-1))`, using log2(L) index bits.
* Otherwise an affine permutation maps i to `(i*stride+offset) mod L`. The selected stride is coprime to L, making the mapping a permutation.

Offset and affine stride are deterministic functions of L, T, and compile-time salts, mixed with SplitMix64. Default salts are 1 for L<=2048, 127 for 2048<L<=4096, and 7 for L>4096, with 29 used in that last range when T modulo 3 is 2. These bit-length thresholds are separate from the message-size thresholds above. Each encode/decode call computes one interleaver plan and reuses it.

Interleaving aims to distribute physically adjacent corruption among separated trellis observations. Its presence alone does not establish a fixed burst-correction guarantee. SplitMix64 here provides deterministic index mixing; it does not provide encryption or a security property.

## Viterbi decoding

The decoder reconstructs the puncturing and interleaver plans from the received codeword length, N, and T. It caps the interpreted received length at 2N bytes. Truncating a codeword changes these plans; arbitrary truncation is not a supported erasure mechanism.

A static trellis caches expected parity outputs for the two incoming transitions of each state. Initially, state zero has metric zero and all other states have metric 30000. At every step, the decoder compares two predecessor candidates for each destination state. Branch cost is hard-decision Hamming distance, obtained from a 4-by-4 lookup table. Masking excludes punctured lanes from that cost. Equal candidate costs select predecessor zero.

Two rolling `uint16_t` arrays hold current and next path metrics. A full traceback table stores one `uint8_t` predecessor choice per step and state. Traceback begins from known terminal state zero, recovers the input bits, and removes the K-1 termination bits before packing N output bytes. It does not select an unconstrained minimum-metric final state or expose a separate decoding-confidence result.

## Cost and implementation limits

For S=2^(K-1) states and Q=8N+K-1 trellis steps, the dominant decoding time is O(QS), and traceback storage is QS bytes. At the largest official input N=512, K=12, the choice table alone uses 8,411,136 bytes, about 8.02 MiB. At N=256, K=11, it uses 2,107,392 bytes, about 2.01 MiB. The choices occupy one byte each, rather than one packed bit.

Input and encoded data remain byte arrays with direct bit access. The decoder still materializes recovered bits before packing bytes, and stores the full traceback history. The static trellis also contains a predecessor array although the decoding loop derives predecessors directly.

At fixed N and K, changing T or redundancy does not change the dominant number of trellis states or steps. Interleaving, retained-observation reads, and output length still contribute work. The source precomputes parity outputs, reuses interleaver parameters, and uses a distance lookup table, but no measured comparison against an earlier version is available.

The 512-byte assignment limit also bounds path metrics: the maximum raw branch-cost sum is 8214, below the initial unreachable-state metric and 16-bit overflow. The implementation does not normalize metrics for arbitrary input lengths. This is an assignment-sized codec, without a universal correction bound or arbitrary-size decoding guarantee. See [performance and limitations](PERFORMANCE.md) for observed recoveries and failures.
