#!/bin/sh
# Validate that Python bindings release resources after processing a large
# image. The helper enforces ResourceWarning as errors and ensures outputs
# can be cleaned up on platforms like Windows where open handles block
# deletion.

set -euxv

. "$(dirname "$0")/../lib/common.sh"

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

verify_script="${tmp_dir}/verify-resources.py"
cat >"${verify_script}" <<'PY'
"""Ensure libsixel frees resources after large image processing.

The script treats ResourceWarning as errors and deletes generated outputs to
confirm no lingering file handles remain.
"""
import gc
import pathlib
import sys
import warnings
from typing import Tuple

try:
    from libsixel import (
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_WIDTH,
        SIXEL_OPTFLAG_HEIGHT,
    )
    from libsixel.encoder import Encoder
    from libsixel.decoder import Decoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)


def encode_large(source: pathlib.Path, target: pathlib.Path) -> int:
    """Encode a large image into SIXEL and return the output size."""

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    encoder.setopt(SIXEL_OPTFLAG_WIDTH, "800")
    encoder.setopt(SIXEL_OPTFLAG_HEIGHT, "600")
    encoder.encode(str(source))

    if not target.exists():
        raise SystemExit("encoder did not write output")
    size = target.stat().st_size
    if size == 0:
        raise SystemExit("encoder wrote empty output")

    return size


def decode_large(source: pathlib.Path, target: pathlib.Path) -> Tuple[int, int]:
    """Decode the SIXEL back to PNG and return its dimensions."""

    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    decoder.decode(str(source))

    if not target.exists():
        raise SystemExit("decoder did not write output")
    if target.stat().st_size == 0:
        raise SystemExit("decoder wrote empty output")

    # Read minimal PNG header to avoid extra dependencies.
    with target.open("rb") as handle:
        header = handle.read(24)
    if len(header) < 24:
        raise SystemExit("decoded PNG header too small")

    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")
    return width, height


def remove_path(path: pathlib.Path) -> None:
    """Delete a path and report detailed errors on failure."""

    try:
        path.unlink()
    except FileNotFoundError:
        return
    except Exception as exc:  # noqa: BLE001 - propagate exact failure context
        raise SystemExit(f"failed to remove {path.name}: {exc}") from exc


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: verify-resources.py <source> <workdir>")

    source_image = pathlib.Path(sys.argv[1])
    workdir = pathlib.Path(sys.argv[2])
    workdir.mkdir(parents=True, exist_ok=True)

    sixel_path = workdir / "large.six"
    decoded_png = workdir / "roundtrip.png"

    with warnings.catch_warnings():
        warnings.simplefilter("error", ResourceWarning)
        size = encode_large(source_image, sixel_path)
        width, height = decode_large(sixel_path, decoded_png)

    # Release references before deletion to surface handle leaks.
    gc.collect()

    remove_path(sixel_path)
    remove_path(decoded_png)

    if sixel_path.exists():
        raise SystemExit("sixel output persists after deletion attempt")
    if decoded_png.exists():
        raise SystemExit("decoded PNG persists after deletion attempt")

    print(
        f"encoded {size} bytes, decoded {width}x{height}, resources cleaned"
    )


if __name__ == "__main__":
    main()
PY

if [ "${use_wheel}" -eq 1 ]; then
    run_venv="${tmp_dir}/venv"
    if ! python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_skip_all "wheel installation failed"
    fi
fi

case_id=1

source_image="${TOP_SRCDIR}/images/autumn.png"
work_dir="${tmp_dir}/work"

if [ "${use_wheel}" -eq 1 ]; then
    ld_env="${python_wheel_ld_library_path}"
    if PYTHONPATH="${python_trace_pythonpath}" \
       LD_LIBRARY_PATH="${ld_env}" \
       LIBSIXEL_LIBDIR="${lib_dir}" \
       "${run_python}" "${verify_script}" \
       "${source_image}" "${work_dir}" >>"${log_file}" 2>&1; then
        tap_pass ${case_id} "large image roundtrip via wheel frees resources"
    else
        python_skip_on_load_error $? "${log_file}"
        tap_fail ${case_id} "resource test via wheel failed"
    fi
else
    if PYTHONPATH="${python_in_tree_trace_pythonpath}" \
       LD_LIBRARY_PATH="${python_in_tree_ld_library_path}" \
       LIBSIXEL_LIBDIR="${lib_dir}" \
       "${run_python}" "${verify_script}" \
       "${source_image}" "${work_dir}" >>"${log_file}" 2>&1; then
        tap_pass ${case_id} "large image roundtrip via in-tree modules frees resources"
    else
        python_skip_on_load_error $? "${log_file}"
        tap_fail ${case_id} "resource test via in-tree modules failed"
    fi
fi

tap_plan 1
exit ${tap_status}