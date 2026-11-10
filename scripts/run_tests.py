#!/usr/bin/env python3
"""Measure the preserved codec on deterministic synthetic channels, not official cases."""
import csv
import hashlib
import json
import math
import platform
import random
import re
import shutil
import signal
import statistics
import subprocess
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AGES = [1, 50, 51, 100, 101, 200, 201, 999, 1000]
SIZES = [1, 2, 3, 7, 8, 16, 32, 64, 127, 128, 129, 255, 256, 257, 511, 512]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def limited(command, cwd, stdout, seconds):
    with subprocess.Popen(command, cwd=cwd, stdout=stdout, stderr=subprocess.STDOUT,
                          start_new_session=True) as process:
        try:
            process.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            try:
                __import__('os').killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
            raise
        if process.returncode:
            raise subprocess.CalledProcessError(process.returncode, command)


def message(size, family):
    if family == 'zero':
        return bytes(size)
    if family == 'ones':
        return bytes([255]) * size
    blocks = [hashlib.sha256(f'HS_final/N={size}/block={i}'.encode()).digest() for i in range((size+31)//32)]
    return b''.join(blocks)[:size]


def corrupt(encoded, channel, size, age):
    seed = int.from_bytes(hashlib.sha256(f'HS_final/{channel}/{size}/{age}'.encode()).digest()[:8], 'big')
    rng = random.Random(seed)
    count = len(encoded)*8
    if channel == 'none':
        positions = []
    elif channel == 'single_bit':
        positions = [rng.randrange(count)]
    elif channel == 'random_1pct':
        positions = sorted(rng.sample(range(count), max(1, count//100)))
    elif channel == 'burst_2bytes':
        start = rng.randrange(len(encoded)-1)*8
        positions = list(range(start, start+16))
    elif channel == 'cluster_8of32':
        start = rng.randrange(count-31)
        positions = sorted(rng.sample(range(start, start+32), 8))
    elif channel == 'reference_intensity_iid':
        p = 0.06*math.sqrt(age/1000)
        positions = [i for i in range(count) if rng.random() < p]
    else:
        raise ValueError(channel)
    changed = bytearray(encoded)
    for bit in positions:
        changed[bit//8] ^= 1 << (7-bit%8)
    return bytes(changed), positions, seed


def read_hex(path):
    text = path.read_text()
    if not re.fullmatch(r'[0-9A-F]*\n', text):
        raise RuntimeError(f'Malformed output: {path}')
    return bytes.fromhex(text.strip())


def main():
    build = ROOT / '.build' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    project = build / 'project'
    project.mkdir(parents=True)
    sources = [ROOT/'Makefile'] + sorted((ROOT/'src').glob('*.cpp')) + sorted((ROOT/'include').glob('*.h'))
    sources += [Path(__file__).resolve()]
    hashes = {p.relative_to(ROOT).as_posix(): digest(p) for p in sources}
    for directory in ['src', 'include']:
        shutil.copytree(str(ROOT/directory), str(project/directory))
    shutil.copy2(str(ROOT/'Makefile'), str(project/'Makefile'))
    print(f'Build and evidence: {build}', flush=True)
    started = time.perf_counter()
    with (build/'build.log').open('w') as log:
        limited(['make'], project, log, 300)
    build_seconds = time.perf_counter()-started
    executable = project/'bin/solution'
    cases = [('hashed', n, t, 'none') for n in SIZES for t in AGES]
    cases += [(family, n, t, 'none') for family in ['zero', 'ones']
              for n in [1, 8, 128, 129, 256, 257, 512] for t in [1, 100, 101, 200, 201, 1000]]
    cases += [('hashed', n, t, channel) for n in [8, 64, 128, 256, 512]
              for t in [1, 51, 101, 201, 1000]
              for channel in ['single_bit', 'random_1pct', 'burst_2bytes', 'cluster_8of32', 'reference_intensity_iid']]
    rows = []
    vectors = []
    call_times = {'encode': [], 'decode': []}
    workspace = build/'case'
    workspace.mkdir()

    def invoke(mode, age, data, n, name):
        input_path = workspace/(name+'.in')
        output_path = workspace/(name+'.out')
        input_path.write_text(f'{n}\n{data.hex().upper()}\n')
        start = time.perf_counter()
        with (workspace/(name+'.log')).open('w') as log:
            limited([str(executable), mode, str(age), str(input_path), str(output_path)], workspace, log, 1)
        elapsed = time.perf_counter()-start
        call_times[mode].append(elapsed)
        return read_hex(output_path), elapsed

    for index, (family, n, t, channel) in enumerate(cases):
        plain = message(n, family)
        encoded, encode_seconds = invoke('encode', t, plain, n, 'encode')
        if not n <= len(encoded) <= 2*n:
            raise RuntimeError(f'Encoded byte cap/length violated at N={n},T={t}')
        if channel == 'none':
            repeated, _ = invoke('encode', t, plain, n, 'repeat')
            if repeated != encoded:
                raise RuntimeError(f'Non-deterministic encoding at N={n},T={t}')
        changed, bits, seed = corrupt(encoded, channel, n, t)
        decoded, decode_seconds = invoke('decode', t, changed, n, 'decode')
        if len(decoded) != n:
            raise RuntimeError('Decoder output length mismatch')
        correct = decoded == plain
        row = {'trial': index, 'family': family, 'message_bytes': n, 'T': t, 'channel': channel,
               'encoded_bytes': len(encoded), 'ratio': len(encoded)/n, 'flipped_bits': len(bits),
               'correct': correct, 'encode_seconds': encode_seconds, 'decode_seconds': decode_seconds}
        rows.append(row)
        vectors.append({**row, 'seed': seed, 'flipped_positions': bits, 'message': plain.hex(),
                        'encoded': encoded.hex(), 'corrupted': changed.hex(), 'decoded': decoded.hex()})
    with (build/'trials.csv').open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (build/'trials.jsonl').write_text(''.join(json.dumps(v)+'\n' for v in vectors))
    groups = {}
    for channel in sorted({r['channel'] for r in rows}):
        group = [r for r in rows if r['channel'] == channel]
        groups[channel] = {'trials': len(group), 'recovered': sum(r['correct'] for r in group)}
    summary = {
        'scope': 'Deterministic synthetic channels; official evaluator and public fixtures unavailable',
        'trials': len(rows), 'by_channel': groups,
        'minimum_ratio': min(r['ratio'] for r in rows), 'maximum_ratio': max(r['ratio'] for r in rows),
        'cap_checks_passed': len(rows), 'determinism_checks_passed': groups['none']['trials'],
        'build_seconds': build_seconds,
        'encode_calls': len(call_times['encode']), 'decode_calls': len(call_times['decode']),
        'median_encode_seconds': statistics.median(call_times['encode']),
        'maximum_encode_seconds': max(call_times['encode']),
        'median_decode_seconds': statistics.median(call_times['decode']),
        'maximum_decode_seconds': max(call_times['decode']),
        'age_values': AGES, 'message_sizes': SIZES,
    }
    (build/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    for relative, h in hashes.items():
        if digest(ROOT/relative) != h:
            raise RuntimeError(f'Execution source changed: {relative}')
    metadata = {'date': datetime.now(timezone(timedelta(hours=8))).strftime('%Y-%m-%d'),
                'date_timezone': 'Asia/Taipei', 'command': 'python3 scripts/run_tests.py',
                'compiler': subprocess.check_output(['g++', '--version']).decode().splitlines()[0],
                'python': platform.python_version(), 'machine': platform.machine(),
                'sources': hashes, 'compiled_binary_sha256': digest(executable),
                'vectors_sha256': digest(build/'trials.jsonl'),
                'timing_context': 'Sequential CLI calls including process startup, file I/O and Python wait polling; not kernel-only time'}
    (build/'run.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print(json.dumps(summary, indent=2), flush=True)
    if groups['none']['recovered'] != groups['none']['trials']:
        raise SystemExit('FAIL: noiseless recovery failures retained in trials.jsonl')
    print('PASS: noiseless recovery, deterministic encoding, formatting and 2N byte cap. Corrupted-channel recovery is reported separately.')


if __name__ == '__main__':
    main()
