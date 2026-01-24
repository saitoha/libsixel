#!/bin/sh
# Exercise encoder options for palette, diffusion, quality, background color,
# and resizing knobs. The script validates that palette limits are respected
# and that explicit/aspect-preserving dimensions are reflected in decoded
# outputs. Each scenario is run via a small helper in Python so the TAP cases
# remain readable.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../lib/sh/python/common.sh"

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/python.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${artifact_dir}" "${tmp_dir}"
rm -f "${log_file}"

tap_log_file="${log_file}"

python_prepare "${log_file}" "${tmp_dir}"

verify_script="${tmp_dir}/verify-options.py"
cat >"${verify_script}" <<'PY'
"""Validate Python encoder options that influence palette and geometry.

The helper exposes three scenarios so the TAP harness can track failures
individually.
"""
import math
import pathlib
import re
import sys
from typing import Iterable, Set, Tuple

try:
    from libsixel import (
        SIXEL_OPTFLAG_BGCOLOR,
        SIXEL_OPTFLAG_COLORS,
        SIXEL_OPTFLAG_DIFFUSION,
        SIXEL_OPTFLAG_HEIGHT,
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_PALETTE_TYPE,
        SIXEL_OPTFLAG_QUALITY,
        SIXEL_OPTFLAG_RESAMPLING,
        SIXEL_OPTFLAG_WIDTH,
    )
    from libsixel.decoder import Decoder
    from libsixel.encoder import Encoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)


def ensure_sixel_signature(data: bytes) -> None:
    """Raise if the DCS/ST framing is missing.

    SIXEL streams must begin with ESC P q and terminate with ESC \\ even
    after trimming trailing newlines that may be emitted by the encoder.
    """

    if not data.startswith(b"\x1bPq"):
        raise SystemExit("missing sixel DCS introducer")

    if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
        raise SystemExit("missing sixel ST terminator")


def extract_palette_indexes(data: bytes) -> Set[int]:
    """Return the set of palette register numbers defined in the stream."""

    matches = re.findall(rb"#(\d+)", data)
    return {int(entry) for entry in matches}


def extract_raster_size(data: bytes) -> Tuple[int, int] | None:
    """Parse the raster attribute if present.

    The encoder emits a raster attribute command of the form
    '"<pan>;<pad>;<ph>;<pv>' where <pan>/<pad> represent logical width/height
    in pixels. Only the first tuple is relevant for these tests.
    """

    match = re.search(rb'"(\d+);(\d+);(\d+);(\d+)', data)
    if match:
        return int(match.group(1)), int(match.group(2))
    return None


def read_png_dimensions(path: pathlib.Path) -> Tuple[int, int]:
    """Return (width, height) from the PNG header without extra deps."""

    header = path.read_bytes()
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("output is not a PNG")

    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")
    return width, height


def encode_with_options(
    source: pathlib.Path, target: pathlib.Path, options: Iterable[tuple[int, str]]
) -> tuple[bytes, Set[int], Tuple[int, int] | None]:
    """Encode an image with the supplied encoder flags and return metadata."""

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))

    for flag, value in options:
        encoder.setopt(flag, value)

    encoder.encode(str(source))

    if not target.exists():
        raise SystemExit("missing sixel output")
    if target.stat().st_size == 0:
        raise SystemExit("empty sixel output")

    data = target.read_bytes()
    ensure_sixel_signature(data)
    palette = extract_palette_indexes(data)
    raster = extract_raster_size(data)

    return data, palette, raster


def decode_to_png(source: pathlib.Path, target: pathlib.Path) -> Tuple[int, int]:
    """Decode a sixel back to PNG and report its dimensions."""

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    decoder.decode(str(source))

    if not target.exists():
        raise SystemExit("decoder did not write output")
    if target.stat().st_size == 0:
        raise SystemExit("decoder wrote empty output")

    return read_png_dimensions(target)


def run_palette_combo(source: pathlib.Path, workdir: pathlib.Path) -> str:
    """Verify palette/diffusion/quality/bg options reduce color count."""

    output = workdir / "palette.six"
    _, palette, raster = encode_with_options(
        source,
        output,
        [
            (SIXEL_OPTFLAG_COLORS, "16"),
            (SIXEL_OPTFLAG_DIFFUSION, "atkinson"),
            (SIXEL_OPTFLAG_PALETTE_TYPE, "hls"),
            (SIXEL_OPTFLAG_QUALITY, "high"),
            (SIXEL_OPTFLAG_BGCOLOR, "#000000"),
        ],
    )

    if not palette:
        raise SystemExit("no palette registers defined")
    if len(palette) > 16:
        raise SystemExit(f"palette expanded to {len(palette)} entries")

    if raster:
        return f"palette ok (<=16 entries), raster={raster[0]}x{raster[1]}"

    return "palette ok (<=16 entries)"


def run_fixed_resize(source: pathlib.Path, workdir: pathlib.Path) -> str:
    """Resize to explicit dimensions and confirm the decoded PNG matches."""

    output = workdir / "resize_fixed.six"
    png = workdir / "resize_fixed.png"

    _, _, raster = encode_with_options(
        source,
        output,
        [
            (SIXEL_OPTFLAG_WIDTH, "64"),
            (SIXEL_OPTFLAG_HEIGHT, "32"),
            (SIXEL_OPTFLAG_RESAMPLING, "bilinear"),
            (SIXEL_OPTFLAG_QUALITY, "full"),
        ],
    )

    width, height = decode_to_png(output, png)
    if (width, height) != (64, 32):
        raise SystemExit(f"expected 64x32, got {width}x{height}")

    if raster and (raster[0] > 1 or raster[1] > 1):
        if (raster[0], raster[1]) != (64, 32):
            raise SystemExit(
                f"raster attribute {raster[0]}x{raster[1]} does not match target"
            )

    return "resize to 64x32 preserved in decode and raster"


def run_aspect_resize(source: pathlib.Path, workdir: pathlib.Path) -> str:
    """Resize by width only and check aspect ratio is retained."""

    output = workdir / "resize_aspect.six"
    png = workdir / "resize_aspect.png"

    _, _, raster = encode_with_options(
        source,
        output,
        [
            (SIXEL_OPTFLAG_WIDTH, "48"),
            (SIXEL_OPTFLAG_RESAMPLING, "lanczos3"),
            (SIXEL_OPTFLAG_QUALITY, "auto"),
        ],
    )

    original_size = read_png_dimensions(source)
    width, height = decode_to_png(output, png)

    if width != 48:
        raise SystemExit(f"expected width 48, got {width}")

    src_ratio = original_size[0] / original_size[1]
    out_ratio = width / height
    if not math.isclose(src_ratio, out_ratio, rel_tol=0.05, abs_tol=0.01):
        raise SystemExit(
            f"aspect ratio changed (src={src_ratio:.4f}, out={out_ratio:.4f})"
        )

    if raster and (raster[0] > 1 or raster[1] > 1):
        if raster[0] != 48:
            raise SystemExit(
                f"raster width {raster[0]} did not track resize width"
            )

    return f"aspect preserved at 48px width (decoded {width}x{height})"


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: verify-options.py <scenario> <source> <workdir>")

    scenario = sys.argv[1]
    source = pathlib.Path(sys.argv[2])
    workdir = pathlib.Path(sys.argv[3])
    workdir.mkdir(parents=True, exist_ok=True)

    if scenario == "palette":
        message = run_palette_combo(source, workdir)
    elif scenario == "resize_fixed":
        message = run_fixed_resize(source, workdir)
    elif scenario == "resize_aspect":
        message = run_aspect_resize(source, workdir)
    else:
        raise SystemExit(f"unknown scenario: {scenario}")

    print(message)


if __name__ == "__main__":
    main()
PY

if [ "${use_wheel}" -eq 1 ]; then
    run_venv="${tmp_dir}/venv"
    if ! python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_skip_all "wheel installation failed"
    fi
fi

cases=3
case_id=1

run_case() {
    scenario=$1
    description=$2

    working_dir="${tmp_dir}/${scenario}"

    mkdir -p "${working_dir}"

    if [ "${use_wheel}" -eq 1 ]; then
        if env ${python_wheel_loader_env} \
           PYTHONPATH="${python_wheel_trace_pythonpath}" \
           LIBSIXEL_LIBDIR="${python_lib_dir}" \
           "${run_python}" "${verify_script}" \
           "${scenario}" "${TOP_SRCDIR}/images/autumn.png" \
           "${working_dir}" >>"${log_file}" 2>&1; then
            tap_pass ${case_id} "${description} via wheel"
        else
            python_skip_on_load_error $? "${log_file}"
            tap_fail ${case_id} "${description} via wheel failed"
        fi
    else
        if env ${python_in_tree_loader_env} \
           PYTHONPATH="${python_in_tree_trace_pythonpath}" \
           LIBSIXEL_LIBDIR="${python_lib_dir}" \
           "${run_python}" "${verify_script}" \
           "${scenario}" "${TOP_SRCDIR}/images/autumn.png" "${working_dir}" \
           >>"${log_file}" 2>&1; then
            tap_pass ${case_id} "${description} via in-tree modules"
        else
            python_skip_on_load_error $? "${log_file}"
            tap_fail ${case_id} "${description} via in-tree modules failed"
        fi
    fi

    case_id=$((case_id + 1))
}

run_case palette "palette/diffusion/quality options honor palette limit"
run_case resize_fixed "explicit resize dimensions survive decode"
run_case resize_aspect "width-only resize keeps aspect ratio"

tap_plan ${cases}
exit ${tap_status}
