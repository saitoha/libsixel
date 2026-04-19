#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Compare k-center and heckbert quality/speed over a 5x5 -X/-W matrix.
#

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_COLORSPACES = ["gamma", "linear", "oklab", "cielab", "din99d"]


@dataclass
class CaseMetric:
    xspace: str
    wspace: str
    center_time_ms: float
    heckbert_time_ms: float
    time_ratio: float
    center_msssim: float
    heckbert_msssim: float
    quality_delta: float
    speed_win: int
    quality_win: int
    strict_win: int
    extreme_regression: int


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[2]

    parser = argparse.ArgumentParser(
        description=(
            "Compare -Q center vs -Q heckbert over the 25-case -X/-W matrix."
        )
    )
    parser.add_argument(
        "--build-dir",
        default=str(root / "build-autotools-kcenter-check"),
        help="Build directory that contains converters/ and assessment/ binaries.",
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=3,
        help="Encode repeats per (image, model, -X/-W case).",
    )
    parser.add_argument(
        "--out",
        default=str(root / "runs" / "kcenter_xw_matrix.tsv"),
        help="Output TSV path.",
    )
    parser.add_argument(
        "--images",
        nargs="*",
        default=[
            str(root / "tests" / "data" / "inputs" / "formats" /
                "snake-64-reference-rgb.png"),
            str(root / "tests" / "data" / "inputs" / "formats" /
                "snake-64-reference-rgba.png"),
            str(root / "tests" / "data" / "inputs" / "formats" /
                "snake-png-adam7-rgb.png"),
            str(root / "tests" / "data" / "inputs" / "formats" /
                "snake-jpeg-444.jpg"),
            str(root / "tests" / "data" / "inputs" / "formats" /
                "pal8-256colors.png"),
        ],
        help="Input images. Default is a balanced 5-image set.",
    )
    return parser.parse_args()


def ensure_binary(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"required binary not found: {path}")


def run_encode(
    img2sixel: Path,
    image: Path,
    model: str,
    xspace: str,
    wspace: str,
    repeat: int,
) -> tuple[bytes, float]:
    total_ns = 0
    latest_sixel = b""
    index = 0

    for index in range(repeat):
        command = [
            str(img2sixel),
            str(image),
            "-Q",
            model,
            "-X",
            xspace,
            "-W",
            wspace,
        ]

        started = time.perf_counter_ns()
        result = subprocess.run(
            command,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        elapsed_ns = time.perf_counter_ns() - started
        total_ns += elapsed_ns
        if index == repeat - 1:
            latest_sixel = result.stdout

    return latest_sixel, (total_ns / float(repeat)) / 1_000_000.0


def sixel_to_png(sixel2png: Path, payload: bytes, png_path: Path) -> None:
    with png_path.open("wb") as output:
        subprocess.run(
            [str(sixel2png)],
            check=True,
            input=payload,
            stdout=output,
            stderr=subprocess.PIPE,
        )


def run_lsqa(lsqa: Path, reference: Path, candidate: Path) -> float:
    result = subprocess.run(
        [str(lsqa), str(reference), str(candidate)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    report = json.loads(result.stdout)
    return float(report["quality"]["MS-SSIM"])


def iter_matrix() -> Iterable[tuple[str, str]]:
    xspace = ""
    wspace = ""

    for xspace in DEFAULT_COLORSPACES:
        for wspace in DEFAULT_COLORSPACES:
            yield xspace, wspace


def evaluate_case(
    img2sixel: Path,
    sixel2png: Path,
    lsqa: Path,
    images: list[Path],
    repeat: int,
    xspace: str,
    wspace: str,
) -> CaseMetric:
    center_time_sum = 0.0
    heckbert_time_sum = 0.0
    center_q_sum = 0.0
    heckbert_q_sum = 0.0

    image = Path()
    center_sixel = b""
    heckbert_sixel = b""

    for image in images:
        center_sixel, center_time = run_encode(
            img2sixel,
            image,
            "center",
            xspace,
            wspace,
            repeat,
        )
        heckbert_sixel, heckbert_time = run_encode(
            img2sixel,
            image,
            "heckbert",
            xspace,
            wspace,
            repeat,
        )

        with tempfile.TemporaryDirectory(prefix="kcenter-matrix-") as tmpdir:
            center_png = Path(tmpdir) / "center.png"
            heckbert_png = Path(tmpdir) / "heckbert.png"
            sixel_to_png(sixel2png, center_sixel, center_png)
            sixel_to_png(sixel2png, heckbert_sixel, heckbert_png)
            center_q = run_lsqa(lsqa, image, center_png)
            heckbert_q = run_lsqa(lsqa, image, heckbert_png)

        center_time_sum += center_time
        heckbert_time_sum += heckbert_time
        center_q_sum += center_q
        heckbert_q_sum += heckbert_q

    count = float(len(images))
    center_time_avg = center_time_sum / count
    heckbert_time_avg = heckbert_time_sum / count
    center_q_avg = center_q_sum / count
    heckbert_q_avg = heckbert_q_sum / count

    time_ratio = center_time_avg / heckbert_time_avg
    quality_delta = center_q_avg - heckbert_q_avg

    speed_win = 1 if time_ratio < 1.0 else 0
    quality_win = 1 if quality_delta > 0.0 else 0
    strict_win = 1 if speed_win == 1 and quality_win == 1 else 0
    extreme_regression = 1 if time_ratio > 1.10 and quality_delta < -0.003 else 0

    return CaseMetric(
        xspace=xspace,
        wspace=wspace,
        center_time_ms=center_time_avg,
        heckbert_time_ms=heckbert_time_avg,
        time_ratio=time_ratio,
        center_msssim=center_q_avg,
        heckbert_msssim=heckbert_q_avg,
        quality_delta=quality_delta,
        speed_win=speed_win,
        quality_win=quality_win,
        strict_win=strict_win,
        extreme_regression=extreme_regression,
    )


def write_tsv(path: Path, rows: list[CaseMetric]) -> None:
    header = [
        "xspace",
        "wspace",
        "center_time_ms",
        "heckbert_time_ms",
        "time_ratio",
        "center_msssim",
        "heckbert_msssim",
        "quality_delta",
        "speed_win",
        "quality_win",
        "strict_win",
        "extreme_regression",
    ]

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        stream.write("\t".join(header) + "\n")
        for row in rows:
            stream.write(
                "\t".join(
                    [
                        row.xspace,
                        row.wspace,
                        f"{row.center_time_ms:.6f}",
                        f"{row.heckbert_time_ms:.6f}",
                        f"{row.time_ratio:.6f}",
                        f"{row.center_msssim:.6f}",
                        f"{row.heckbert_msssim:.6f}",
                        f"{row.quality_delta:.6f}",
                        str(row.speed_win),
                        str(row.quality_win),
                        str(row.strict_win),
                        str(row.extreme_regression),
                    ]
                )
                + "\n"
            )


def main() -> int:
    args = parse_args()

    build_dir = Path(args.build_dir).resolve()
    img2sixel = build_dir / "converters" / "img2sixel"
    sixel2png = build_dir / "converters" / "sixel2png"
    lsqa = build_dir / "assessment" / "lsqa"

    ensure_binary(img2sixel)
    ensure_binary(sixel2png)
    ensure_binary(lsqa)

    if args.repeat < 1:
        raise ValueError("--repeat must be >= 1")

    images = [Path(item).resolve() for item in args.images]
    for image in images:
        if not image.exists():
            raise FileNotFoundError(f"input image not found: {image}")

    rows = []
    xspace = ""
    wspace = ""

    print("Running k-center vs heckbert matrix benchmark...")
    print(f"build_dir={build_dir}")
    print(f"repeat={args.repeat}")
    print(f"image_count={len(images)}")

    for xspace, wspace in iter_matrix():
        row = evaluate_case(
            img2sixel,
            sixel2png,
            lsqa,
            images,
            args.repeat,
            xspace,
            wspace,
        )
        rows.append(row)
        print(
            f"-X {xspace:7s} -W {wspace:7s} "
            f"tr={row.time_ratio:.3f} dq={row.quality_delta:+.6f} "
            f"strict={row.strict_win}"
        )

    out_path = Path(args.out).resolve()
    write_tsv(out_path, rows)

    strict_win = sum(item.strict_win for item in rows)
    speed_win = sum(item.speed_win for item in rows)
    quality_win = sum(item.quality_win for item in rows)
    extreme = sum(item.extreme_regression for item in rows)

    print("")
    print(f"wrote: {out_path}")
    print(f"strict_win: {strict_win}/25")
    print(f"speed_win:  {speed_win}/25")
    print(f"quality_win:{quality_win}/25")
    print(f"extreme_regression(tr>1.10 && dq<-0.003): {extreme}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
