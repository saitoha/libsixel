"""Fetch fixed-ID study photos and verify recorded hashes when supplied."""

import argparse
import concurrent.futures
import hashlib
import json
import os
import pathlib
import time
import urllib.request

root = pathlib.Path(
    os.environ.get("OR_MODE_STUDY_DIR", pathlib.Path(__file__).parent)
).resolve()
out = root / "inputs"
out.mkdir(exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--manifest", type=pathlib.Path)
args = parser.parse_args()
items = (
    json.loads(args.manifest.read_text())
    if args.manifest
    else json.load(
        urllib.request.urlopen("https://picsum.photos/v2/list?page=1&limit=100")
    )
)


def get(item):
    url = f"https://picsum.photos/id/{item['id']}/800/600.jpg"
    path = out / f"picsum-{item['id']}.jpg"
    for attempt in range(4):
        try:
            if not path.exists():
                path.write_bytes(urllib.request.urlopen(url, timeout=40).read())
            sha = hashlib.sha256(path.read_bytes()).hexdigest()
            if item.get("sha256") and item["sha256"] != sha:
                raise RuntimeError("Input hash changed: " + url)
            item.update(path=str(path), request_url=url, sha256=sha)
            return item
        except Exception:
            if attempt == 3:
                raise
            time.sleep(1)


with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results = []
    for row in pool.map(get, items):
        results.append(row)
        if len(results) % 10 == 0:
            print("downloaded", len(results), flush=True)
(root / "inputs.json").write_text(json.dumps(results, indent=2) + "\n")
