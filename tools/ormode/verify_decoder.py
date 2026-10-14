"""Check a decoder build against the retained OR-mode compatibility manifest.

Every stream in docs/encoder/output/or-mode/sweeps/verification.json records
the RGBA, byte-index, and 16-bit-index outputs of the baseline decoder.  This
runner decodes each distinct stream with the given build at every worker
budget, requires identical dimensions, color counts, and output digests, and
writes an updated manifest for that build.

usage: verify_decoder.py SOURCE MANIFEST OUTPUT
  SOURCE    built libsixel checkout (its src/.libs is loaded explicitly)
  MANIFEST  existing verification.json
  OUTPUT    path for the manifest describing SOURCE

OR_MODE_STUDY_DIR locates the retained streams named in the manifest, and
verify-decoder must have been compiled from tools/ormode/verify-decoder.c
into that directory.
"""

import hashlib
import json
import os
import pathlib
import subprocess
import sys

from measure import ROOT


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    source = pathlib.Path(sys.argv[1]).resolve()
    manifest = json.loads(pathlib.Path(sys.argv[2]).read_text())
    output = pathlib.Path(sys.argv[3])
    library = source / "src/.libs/libsixel.1.dylib"
    tool = ROOT / "verify-decoder"
    budgets = manifest["thread_budgets"]
    env = {k: v for k, v in os.environ.items() if not k.startswith(("SIXEL_", "DYLD_"))}
    # Built dylibs carry an absolute install name, so -rpath alone can
    # silently load an installed libsixel.  Select the build explicitly.
    env["DYLD_LIBRARY_PATH"] = str(source / "src/.libs")
    checked = {}
    calls = 0
    comparisons = 0
    for i, stream in enumerate(manifest["streams"]):
        path = ROOT / stream["path"]
        assert digest(path) == stream["sha256"], stream["path"]
        expected = [list(o) for o in stream["outputs"]]
        if stream["sha256"] not in checked:
            for threads in budgets:
                proc = subprocess.run(
                    [str(tool), str(path)],
                    env={**env, "SIXEL_THREADS": str(threads)},
                    check=True,
                    capture_output=True,
                    text=True,
                )
                calls += 3
                actual = []
                for line in proc.stdout.splitlines():
                    name, w, h, nc, px, pal = line.split()
                    actual.append(
                        [name, int(w), int(h), int(nc), px, None if pal == "-" else pal]
                    )
                assert actual == expected, (stream["path"], threads, actual)
            checked[stream["sha256"]] = True
        comparisons += len(budgets) * len(expected)
        print(f"verified {i + 1}/{len(manifest['streams'])}", flush=True)
    sources = [
        "src/fromsixel.c",
        "src/decoder-parallel.c",
        "src/decoder-image.h",
        "src/decoder-ormode.h",
    ]
    result = dict(manifest)
    result.update(
        {
            "after_library_sha256": digest(library),
            "after_decode_calls": calls,
            "comparisons": comparisons,
            "decoder_source_sha256": {name: digest(source / name) for name in sources},
        }
    )
    result.pop("after_manifest_sha256", None)
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
