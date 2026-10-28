#!/usr/bin/env python3
"""Remeasure the four bundled range-adaptation examples, without overwriting the archive."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'docs/encoder/palette-construction/pipeline/range-measurements'
METRICS = 'MS-SSIM,DELTA_E00,DE00_region_mean,DE00_region_max,S-CIELAB,S-CIELAB_max'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--measure', type=Path, required=True)
    parser.add_argument('--lsqa', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    # Refuse to overwrite evidence. This tool is outside tests and keeps a
    # complete command/environment record for the new measurement boundary.
    args.output.mkdir(parents=True, exist_ok=False)
    measure, lsqa = args.measure.resolve(), args.lsqa.resolve()
    env = {k:v for k,v in os.environ.items() if not k.startswith(('SIXEL_', '_SIXEL_', 'EXPERIMENT_'))}
    commands=[]
    def run(command):
        commands.append([str(x) for x in command])
        return subprocess.check_output(command, env=env, text=True)
    with (DATA/'quality.csv').open() as f:
        archive = {(r['id'],r['model'],r['space'],r['p']):r for r in csv.DictReader(f)}
    results=[]
    for example in json.loads((DATA/'examples.json').read_text()):
        stem='-'.join(str(example[k]) for k in ['id','model','space','p'])
        source=args.output/(stem+'-source.ppm')
        Image.open(DATA/example['files']['source']).convert('RGB').save(source)
        expected=archive[tuple(str(example[k]) for k in ['id','model','space','p'])]
        for round_id in range(3):
            arms=[('baseline',0),('adaptive',100)]
            if round_id%2:arms.reverse()
            for arm,strength in arms:
                ppm=args.output/(stem+'-'+arm+'.ppm')
                values=run([str(measure),example['model'],str(source),str(example['p']),example['space'],str(strength),'5',str(ppm)]).strip().split(',')
                image=Image.open(ppm).convert('RGB')
                digest=hashlib.sha256(image.tobytes()).hexdigest()
                # PNG avoids ambiguity in Netpbm readers when the first raster
                # byte is '#'. Pixel bytes, not PNG container bytes, are hashed.
                png=ppm.with_suffix('.png');image.save(png)
                metrics=json.loads(run([str(lsqa),'-m',METRICS,'-P','float32','-W','gamma','-L','builtin','-v','32','-w','16',str(DATA/example['files']['source']),str(png)])) if round_id==0 else None
                results.append({k:example[k] for k in ['id','model','space','p']}|{'arm':arm,'round':round_id,'median_ms':float(values[0]),'max_ms':float(values[1]),'bytes':int(values[2]),'colors':int(values[3]),'rgb_sha256':digest,'archive_rgb_match':digest==expected[arm+'_rgb_sha256'],'metrics':metrics})
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    meta={'revision':run(['git','-C',str(ROOT),'rev-parse','HEAD']).strip(),'tracked_dirty':bool(run(['git','-C',str(ROOT),'status','--porcelain','--untracked-files=no']).strip()),'platform':platform.platform(),'measure_sha256':hashlib.sha256(measure.read_bytes()).hexdigest(),'lsqa_sha256':hashlib.sha256(lsqa.read_bytes()).hexdigest(),'library_environment':{k:v for k,v in env.items() if 'LIBRARY_PATH' in k},'commands':commands}
    (args.output/'provenance.json').write_text(json.dumps(meta,indent=2)+'\n')
    print(f"Wrote {len(results)} observations; {sum(r['archive_rgb_match'] for r in results)} archived RGB matches.")
    if not all(r['archive_rgb_match'] for r in results):
        raise SystemExit('Output differs from the archive; inspect results before comparing timings.')


if __name__=='__main__':
    main()
