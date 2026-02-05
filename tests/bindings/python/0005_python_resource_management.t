#!/bin/sh
# Validate that Python bindings release resources after processing a large
# image. The helper enforces ResourceWarning as errors and ensures outputs
# can be cleaned up on platforms like Windows where open handles block
# deletion.

set -eux

PYTHON_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/python"
. "${PYTHON_HELPER_DIR}/common.sh"

python_prepare "${ARTIFACT_LOCAL_DIR}"
set -v

verify_script="${ARTIFACT_LOCAL_DIR}/verify-resources.py"
cat >"${verify_script}" <<'PY'
"""Ensure libsixel frees resources after large image processing.

The script treats ResourceWarning as errors and deletes generated outputs to
confirm no lingering file handles remain.
"""
import gc
import pathlib
import sys
import time
import warnings
from typing import Optional, Tuple

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

    with Encoder() as encoder:
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

    with Decoder() as decoder:
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


def remove_path(path: pathlib.Path, deadline: float = 15.0) -> None:
    """Delete a path with bounded retries for slow Windows handle release."""

    limit = time.monotonic() + deadline
    attempts = 0
    while True:
        attempts += 1
        gc.collect()
        try:
            path.unlink()
            return
        except FileNotFoundError:
            return
        except PermissionError as exc:
            remaining = limit - time.monotonic()
            if remaining <= 0:
                raise SystemExit(
                    f"failed to remove {path.name} after {attempts} attempts: {exc}"
                ) from exc
            print(f"retry {attempts} removing {path.name}: {exc}")
            time.sleep(min(0.5, max(0.05, remaining / 4)))
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

    # Release references and allow handle shutdown before deletion to
    # surface slow cleanup on platforms such as Windows.
    gc.collect()
    time.sleep(0.5)

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
    run_venv="${ARTIFACT_LOCAL_DIR}/venv"
    if ! python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_skip_all "wheel installation failed"
    fi
fi

case_id=1

source_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
work_dir="${ARTIFACT_LOCAL_DIR}/work"

if [ "${use_wheel}" -eq 1 ]; then
    python_output=$(env ${python_wheel_loader_env} \
       PYTHONPATH="${python_wheel_trace_pythonpath}" \
       LIBSIXEL_LIBDIR="${python_lib_dir}" \
       "${run_python}" "${verify_script}" \
       "${source_image}" "${work_dir}" 2>&1)
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "large image roundtrip via wheel frees resources"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "resource test via wheel failed"
    fi
else
    python_output=$(env ${python_in_tree_loader_env} \
       PYTHONPATH="${python_in_tree_trace_pythonpath}" \
       LIBSIXEL_LIBDIR="${python_lib_dir}" \
       "${run_python}" "${verify_script}" \
       "${source_image}" "${work_dir}" 2>&1)
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "large image roundtrip via in-tree modules frees resources"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "resource test via in-tree modules failed"
    fi
fi

tap_plan 1
exit ${tap_status}
