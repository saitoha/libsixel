"""Record current snake output under both article and size-policy baselines."""

import json

import numpy as np
from measure import OUT, SRC, command, decode, rgba, run

rows = []
for colors in [16, 256]:
    for policy in ["auto", "fast", "size"]:
        arrays = []
        sizes = []
        for mode in [0, 1]:
            args = command(SRC / "images/snake.png", colors, 1, mode)
            args[args.index("-Esize")] = "-E" + policy
            data = run(args).stdout
            sizes.append(len(data))
            arrays.append(rgba(decode(data)))
        rows.append(
            {
                "colors": colors,
                "policy": policy,
                "normal_bytes": sizes[0],
                "or_bytes": sizes[1],
                "size_ratio": sizes[1] / sizes[0],
                "changed_pixels": int(
                    np.count_nonzero(np.any(arrays[0] != arrays[1], axis=2))
                ),
            }
        )
(OUT / "qiita-cases.json").write_text(json.dumps(rows, indent=2) + "\n")
print(rows)
