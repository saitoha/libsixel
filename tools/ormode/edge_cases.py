"""Check every palette index at plane-count boundaries and short bands."""

import json

import numpy as np
from measure import OUT, ROOT, command, decode, rgba, run
from PIL import Image


def check_indices():
    results = []
    for n in [
        2,
        3,
        4,
        7,
        8,
        15,
        16,
        17,
        31,
        32,
        33,
        63,
        64,
        65,
        127,
        128,
        129,
        255,
        256,
    ]:
        for height in [1, 5, 6, 7, 11, 12, 13]:
            width = n
            pixels = np.fromfunction(
                lambda y, x, colors=n: (x + y) % colors, (height, width), dtype=int
            )
            palette = np.array(
                [
                    [(i * 37 + 17) % 101, (i * 61 + 29) % 101, (i * 43 + 53) % 101]
                    for i in range(n)
                ],
                dtype=int,
            )
            chunks = [
                f'\x1bPq"1;1;{width};{height}',
                *[
                    f"#{i};2;{rgb[0]};{rgb[1]};{rgb[2]}"
                    for i, rgb in enumerate(palette)
                ],
            ]
            for y in range(0, height, 6):
                for color in range(n):
                    chunks.append("#" + str(color))
                    for x in range(width):
                        bits = sum(
                            (1 << dy)
                            for dy in range(min(6, height - y))
                            if pixels[y + dy, x] == color
                        )
                        chunks.append(chr(63 + bits))
                    chunks.append("$")
                if y + 6 < height:
                    chunks.append("-")
            chunks.append("\x1b\\")
            data = "".join(chunks).encode("ascii")
            path = OUT / "edge-input.six"
            path.write_bytes(data)
            run([ROOT / "indexed-bench", path, OUT / "edge", 0])
            normal = rgba(decode((OUT / "edge.normal.six").read_bytes()))
            ormode = rgba(decode((OUT / "edge.or.six").read_bytes()))
            expected_rgb = ((palette[pixels] * 255 + 50) // 100).astype(np.uint8)
            expected = np.dstack(
                [expected_rgb, np.full((height, width), 255, dtype=np.uint8)]
            )
            assert np.array_equal(normal, expected), (n, height, "normal")
            assert np.array_equal(ormode, expected), (n, height, "OR")
            results.append(
                {"colors": n, "width": width, "height": height, "changed_pixels": 0}
            )
    (OUT / "index-boundaries.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Exact expected RGBA:", len(results), "cases", flush=True)


def check_alpha():
    image = np.zeros((450, 600, 4), dtype=np.uint8)
    image[:] = [230, 90, 40, 255]
    image[70:350, 90:500] = [30, 160, 220, 255]
    image[100:300, 150:450] = [0, 0, 0, 0]
    path = ROOT / "inputs" / "alpha-hole.png"
    Image.fromarray(image).save(path)
    rows = []
    for policy in ["keep", "composite"]:
        arrays = []
        for mode in [0, 1]:
            args = command(path, 256, 1, mode)
            args[-1:-1] = ["--alpha-policy=" + policy]
            if policy == "composite":
                args[-1:-1] = ["-B", "#ffffff"]
            data = run(args).stdout
            png = decode(data)
            arrays.append(rgba(png))
            (OUT / f"alpha-{policy.split(':')[0]}-{mode}.png").write_bytes(png)
        diff = np.abs(arrays[0].astype(np.int16) - arrays[1].astype(np.int16))
        rows.append(
            {
                "policy": policy,
                "changed_pixels": int(np.count_nonzero(np.any(diff, axis=2))),
                "alpha_changed_pixels": int(np.count_nonzero(diff[:, :, 3])),
                "normal_alpha": list(map(int, np.unique(arrays[0][:, :, 3]))),
                "or_alpha": list(map(int, np.unique(arrays[1][:, :, 3]))),
            }
        )
    (OUT / "alpha.json").write_text(json.dumps(rows, indent=2) + "\n")
    print(rows, flush=True)


if __name__ == "__main__":
    check_indices()
    check_alpha()
