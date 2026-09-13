#!/usr/bin/env python3
"""Derive the shared PNG CMS tables from existing TAP tests and references."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from itertools import product
from pathlib import Path
import re
import struct
import sys
import zlib


ROOT = Path(__file__).resolve().parent.parent
DOCUMENT = Path("docs/loader/png-color-metadata.md")
MODELS = ("gray", "idx", "rgb")
BACKENDS = ("builtin", "libpng")
MODEL_NAMES = {"gray": "Grayscale", "idx": "Indexed", "rgb": "RGB"}
CASE_NAME = re.compile(
    r"img_(gray|idx|rgb)_icc([01])_srgb([01])_chrm([01])_gama([01])"
)
CONTROLS = {
    "S": (0, 0, 0, 0),
    "I": (1, 0, 0, 0),
    "G": (0, 0, 0, 1),
    "CG": (0, 0, 1, 1),
}


@dataclass(frozen=True)
class Case:
    test: Path
    image: Path
    reference: Path
    threshold: str


def png_chunks(path: Path) -> dict[bytes, bytes]:
    """Read committed specimens, validating boundaries and stored CRCs."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: invalid PNG signature")
    offset = 8
    chunks: dict[bytes, bytes] = {}
    while offset < len(data):
        if len(data) - offset < 12:
            raise ValueError(f"{path}: truncated chunk")
        length = struct.unpack_from(">I", data, offset)[0]
        end = offset + length + 12
        if end > len(data):
            raise ValueError(f"{path}: truncated payload")
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:end - 4]
        crc = struct.unpack_from(">I", data, end - 4)[0]
        if zlib.crc32(kind + payload) != crc:
            raise ValueError(f"{path}: invalid {kind!r} CRC")
        if kind in chunks and kind != b"IDAT":
            raise ValueError(f"{path}: duplicate {kind!r}")
        chunks[kind] = chunks.get(kind, b"") + payload
        offset = end
    if b"IEND" not in chunks:
        raise ValueError(f"{path}: missing IEND")
    return chunks


def read_cases() -> dict[tuple[str, str, tuple[int, ...]], Case]:
    """Require both decoders to use the complete, shared 3 x 16 matrix."""
    cases = {}
    for backend in BACKENDS:
        directory = ROOT / "tests/loader" / backend
        for test in sorted(directory.glob("*_colormgmt_png_img_*.t")):
            match = re.fullmatch(r"\d+_colormgmt_png_(.*)\.t", test.name)
            name = CASE_NAME.fullmatch(match[1]) if match else None
            if name is None:
                raise ValueError(f"{test}: unexpected matrix test name")
            source = test.read_text(encoding="utf-8")
            paths = []
            for variable in ("input_png", "reference_six"):
                found = re.search(
                    rf'^{variable}="\$\{{TOP_SRCDIR\}}/([^"\n]+)"$',
                    source, re.MULTILINE,
                )
                if found is None or not (ROOT / found[1]).is_file():
                    raise ValueError(f"{test}: missing {variable} file")
                paths.append(Path(found[1]))
            if any(path.stem != name[0] for path in paths):
                raise ValueError(f"{test}: input/reference case was remapped")
            if f"-L{backend}:cms_engine=auto!" not in source:
                raise ValueError(f"{test}: review changed loader/CMS selection")
            threshold = re.search(r'-m MS-SSIM -b "MS-SSIM:([0-9.]+)"', source)
            if threshold is None:
                raise ValueError(f"{test}: missing quality threshold")
            model = name[1]
            flags = tuple(int(name[i]) for i in range(2, 6))
            key = backend, model, flags
            if key in cases:
                raise ValueError(f"{test}: duplicate matrix case")
            cases[key] = Case(test.relative_to(ROOT), *paths, threshold[1])
    expected = set(product(BACKENDS, MODELS, product((0, 1), repeat=4)))
    if set(cases) != expected:
        raise ValueError(f"matrix must contain exactly {len(expected)} cases")
    for model in MODELS:
        baseline = None
        metadata = {}
        for flags in product((0, 1), repeat=4):
            a, b = (cases[backend, model, flags] for backend in BACKENDS)
            if (a.image, a.reference) != (b.image, b.reference):
                raise ValueError(f"{a.test}: decoders no longer share fixtures")
            chunks = png_chunks(ROOT / a.image)
            actual = tuple(int(kind in chunks)
                           for kind in (b"iCCP", b"sRGB", b"cHRM", b"gAMA"))
            if flags != actual:
                raise ValueError(f"{a.image}: chunk presence differs from name")
            for kind in (b"iCCP", b"sRGB", b"cHRM", b"gAMA"):
                if kind in chunks:
                    if kind in metadata and metadata[kind] != chunks[kind]:
                        raise ValueError(f"{a.image}: metadata values vary within model")
                    metadata[kind] = chunks[kind]
            raster = (chunks[b"IHDR"], chunks.get(b"PLTE"),
                      zlib.decompress(chunks[b"IDAT"]))
            if baseline is not None and raster != baseline:
                raise ValueError(f"{a.image}: matrix no longer holds pixels constant")
            baseline = raster
    return cases


def link(label: str, path: Path) -> str:
    return f"[{label}](../../{path.as_posix()})"


def reference_class(cases: dict, model: str, flags: tuple[int, ...]) -> str:
    """Classify reference bytes, never derive an expected result from code."""
    reference = (ROOT / cases["builtin", model, flags].reference).read_bytes()
    for label, control in CONTROLS.items():
        candidate = ROOT / cases["builtin", model, control].reference
        if reference == candidate.read_bytes():
            return label
    raise ValueError(f"{model}/{flags}: new reference class requires review")


def render_matrix(cases: dict) -> str:
    lines = [
        "| iCCP | sRGB | cHRM | gAMA | Grayscale | Indexed | RGB |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for flags in product((0, 1), repeat=4):
        cells = [str(flag) for flag in flags]
        for model in MODELS:
            case = cases["builtin", model, flags]
            label = reference_class(cases, model, flags)
            cells.append(link(label, case.reference))
        lines.append("| " + " | ".join(cells) + " |")
    lines += [
        "", "### Reference controls", "",
        "The cell labels are determined by byte equality with these committed SIXEL controls. The SHA-256 prefixes identify the input and reference bytes; they are not measured decoder output or a guarantee of platform-wide pixel equality.",
        "",
        "| Model | Class | Reference control | Input SHA-256 prefix | Reference SHA-256 prefix |",
        "| --- | --- | --- | --- | --- |",
    ]
    for model in MODELS:
        for label, flags in CONTROLS.items():
            case = cases["builtin", model, flags]
            digest = hashlib.sha256((ROOT / case.reference).read_bytes()).hexdigest()[:16]
            input_digest = hashlib.sha256((ROOT / case.image).read_bytes()).hexdigest()[:16]
            actual = reference_class(cases, model, flags)
            equivalent = f" (same bytes as {actual})" if actual != label else ""
            lines.append(
                f"| {MODEL_NAMES[model]} | {label}{equivalent} | "
                f"{link(case.reference.name, case.reference)} | `{input_digest}` | `{digest}` |"
            )
    return "\n".join(lines)


def render_coverage(cases: dict) -> str:
    lines = []
    number = 0
    for model in MODELS:
        lines += [
            f"#### {MODEL_NAMES[model]}", "",
            "| ID | Input → reference class | builtin owner / MS-SSIM floor | libpng owner / MS-SSIM floor |",
            "| --- | --- | --- | --- |",
        ]
        for flags in product((0, 1), repeat=4):
            number += 1
            a, b = (cases[backend, model, flags] for backend in BACKENDS)
            label = reference_class(cases, model, flags)
            compact = "/".join(str(flag) for flag in flags)
            lines.append(
                f"| PCMQ-{number:02d} | {link(compact, a.image)} → "
                f"{link(label, a.reference)} | "
                f"{link(a.test.as_posix(), a.test)} / {a.threshold} | "
                f"{link(b.test.as_posix(), b.test)} / {b.threshold} |"
            )
        lines.append("")
    return "\n".join(lines).rstrip()


def replace_block(source: str, name: str, contents: str) -> str:
    start = f"<!-- png-cms-{name}: begin -->"
    end = f"<!-- png-cms-{name}: end -->"
    if source.count(start) != 1 or source.count(end) != 1:
        raise ValueError(f"{DOCUMENT}: missing or duplicate {name} markers")
    before, remainder = source.split(start)
    _, after = remainder.split(end)
    return before + start + "\n\n" + contents + "\n\n" + end + after


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()
    try:
        cases = read_cases()
        path = ROOT / DOCUMENT
        current = path.read_text(encoding="utf-8")
        expected = replace_block(current, "matrix", render_matrix(cases))
        expected = replace_block(expected, "coverage", render_coverage(cases))
        if args.write:
            path.write_text(expected, encoding="utf-8")
        elif current != expected:
            raise ValueError(f"{DOCUMENT}: stale tables; run {Path(__file__).name} --write")
    except (OSError, ValueError, KeyError, zlib.error) as error:
        print(error, file=sys.stderr)
        return 1
    print("PNG CMS matrix: 48 inputs, 48 references, 96 quality tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
