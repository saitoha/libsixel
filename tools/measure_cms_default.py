#!/usr/bin/env python3
"""Measure the CLI CMS migration with matched, interleaved end-to-end runs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--img2sixel', type=Path,
                        default=ROOT / 'converters/img2sixel')
    parser.add_argument('--lsqa', type=Path, default=ROOT / 'assessment/lsqa')
    parser.add_argument('--samples', type=int, default=7)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.samples < 3:
        parser.error('at least three samples are required')
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('SIXEL_', 'LSQA_'))}
    cases = ['images/snake.png', 'images/snake.jpg',
             'tests/data/inputs/formats/snake-64-embedded-esrgb.jpg',
             'tests/data/colormgmt/input/custom/rgb_parametric_012.png']
    records = []
    with tempfile.TemporaryDirectory(prefix='cms-default-') as directory:
        for case in cases:
            source = ROOT / case
            commands = {}
            results = {}
            for engine in ['none', 'auto']:
                command = [str(args.img2sixel), '--threads=1',
                           '-L', 'builtin!', '-d', 'fs:scan=raster',
                           '--cms-engine=' + engine, str(source)]
                commands[engine] = command
                # Warm the process and filesystem path before timing.
                subprocess.run(command, env=env, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, check=True)
                results[engine] = {'seconds': []}
            for sample in range(args.samples):
                order = ['none', 'auto'] if sample % 2 == 0 else ['auto', 'none']
                for engine in order:
                    start = time.perf_counter()
                    output = subprocess.run(commands[engine], env=env,
                                            capture_output=True, check=True)
                    results[engine]['seconds'].append(time.perf_counter() - start)
                    results[engine]['output_bytes'] = len(output.stdout)
                    output_file = Path(directory) / (engine + '.six')
                    output_file.write_bytes(output.stdout)
            for engine, result in results.items():
                result['median_seconds'] = statistics.median(result['seconds'])
                quality = [str(args.lsqa), '-L', 'builtin!',
                           '--cms-engine=auto', '-m', 'MS-SSIM', str(source),
                           str(Path(directory) / (engine + '.six'))]
                result['managed_reference_ms_ssim'] = float(
                    subprocess.check_output(quality, env=env))
                result['command'] = commands[engine]
                result['assessment_command'] = quality
                result['output_sha256'] = digest(Path(directory) / (engine + '.six'))
            records.append({'input': case, 'sha256': digest(source),
                            'results': results})
    encoder_binary = args.img2sixel.parent / '.libs' / args.img2sixel.name
    if not encoder_binary.exists():
        encoder_binary = args.img2sixel
    assessment_binary = args.lsqa.parent / '.libs' / args.lsqa.name
    if not assessment_binary.exists():
        assessment_binary = args.lsqa
    data = {'encoder_binary_sha256': digest(encoder_binary),
            'assessment_binary_sha256': digest(assessment_binary),
            'platform': platform.platform(), 'samples': args.samples,
            'revision': subprocess.check_output(
                ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'source_state': 'CMS CLI defaults changed in working tree',
            'cms_source_sha256': digest(ROOT / 'src/cms.c'),
            'img2sixel_source_sha256': digest(ROOT / 'converters/img2sixel.c'),
            'lsqa_source_sha256': digest(ROOT / 'assessment/lsqa.c'),
            'version': subprocess.check_output(
                [str(args.img2sixel), '--version'], env=env, text=True),
            'timing_scope': 'fresh CLI process, decode through SIXEL output; '
                            'stdout pipe and startup included; quality untimed',
            'environment': 'SIXEL_* and LSQA_* removed; builtin loader, one thread',
            'records': records}
    args.output.write_text(json.dumps(data, indent=2) + '\n')


if __name__ == '__main__':
    main()
