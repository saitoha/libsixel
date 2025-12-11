#!/bin/sh
# TAP smoke test for Python bindings. The test prefers the prebuilt wheel
# under python-wheel/dist when available and falls back to the in-tree Python
# sources otherwise.

set -euxv

. "$(dirname "$0")/../lib/common.sh"

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/python.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${artifact_dir}" "${tmp_dir}"
: >"${log_file}"

tap_log_file="${log_file}"

python_prepare "${log_file}" "${tmp_dir}"

tap_plan 2
case_id=1
skip_code=200
skip_reason=""

if [ "${use_wheel}" -eq 1 ]; then
    run_venv="${tmp_dir}/venv"
    if python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_pass ${case_id} "installs wheel from python-wheel/dist"
    else
        tap_fail ${case_id} "wheel installation failed"
    fi
else
    if PYTHONPATH="${python_in_tree_pythonpath}" \
       LD_LIBRARY_PATH="${python_in_tree_ld_library_path}" \
       LIBSIXEL_LIBDIR="${lib_dir}" \
       "${run_python}" - <<'PY' >>"${log_file}" 2>&1; then
try:
    import libsixel
    from libsixel import encoder, decoder

    encoder.Encoder
    decoder.Decoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY
        tap_pass ${case_id} "imports in-tree python modules"
    else
        python_skip_on_load_error $? "${log_file}"
        tap_fail ${case_id} "failed to import in-tree python modules"
    fi
fi

case_id=$((case_id + 1))
verify_script="${tmp_dir}/verify-bindings.py"
cat >"${verify_script}" <<'PY'
import ctypes.util
import glob
import os
import pathlib
import sys
import traceback

SKIP_CODE = int(os.environ.get("SKIP_CODE", "200"))


def _prefer_build_library(name, original_find):
    libdir = os.environ.get("LIBSIXEL_LIBDIR")
    if libdir:
        prefixes = ["lib", ""]
        suffixes = [".so", ".dylib", ".dll"]

        for prefix in prefixes:
            for suffix in suffixes:
                pattern = os.path.join(libdir,
                                       f"{prefix}{name}*{suffix}")
                matches = sorted(glob.glob(pattern))
                if matches:
                    return matches[0]

    return original_find(name)


def main():
    ctypes.util.find_library = (
        lambda name, _orig=ctypes.util.find_library:
        _prefer_build_library(name, _orig)
    )

try:
    from libsixel import SIXEL_PIXELFORMAT_RGB888
    from libsixel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

    root = pathlib.Path(__file__).parent
    output = root / "sample.six"

    pixels = bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ])

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)

    if not output.exists():
        raise SystemExit("missing sixel output")
    if output.stat().st_size == 0:
        raise SystemExit("empty sixel output")

    print("encode succeeded")
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY

python_env="${run_python}"
libdir="${lib_dir}"

if [ -z "${libdir}" ]; then
    tap_fail ${case_id} "could not locate libsixel build output"
    exit ${tap_status}
fi

export LIBSIXEL_LIBDIR="${libdir}"

if [ -n "${skip_reason}" ]; then
    printf 'ok %s - %s # SKIP %s\n' \
        "${case_id}" \
        "encodes image via in-tree modules" \
        "${skip_reason}"
    exit ${status}
fi

if [ "${use_wheel}" -eq 1 ]; then
    ld_library_path_env="${libdir}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        ld_library_path_env="${ld_library_path_env}:${LD_LIBRARY_PATH}"
    fi

    if PYTHONPATH="${python_trace_pythonpath}" \
       LD_LIBRARY_PATH="${ld_library_path_env}" \
       SKIP_CODE=${skip_code} \
       "${python_env}" "${verify_script}" >>"${log_file}" 2>&1; then
        tap_pass ${case_id} "encodes image via wheel"
    else
        python_skip_on_load_error $? "${log_file}"
        tap_fail ${case_id} "python wheel round-trip failed"
    fi
else
    if PYTHONPATH="${python_in_tree_trace_pythonpath}" \
       LD_LIBRARY_PATH="${python_in_tree_ld_library_path}" \
       "${python_env}" "${verify_script}" >>"${log_file}" 2>&1; then
        tap_pass ${case_id} "encodes image via in-tree modules"
    else
        python_skip_on_load_error $? "${log_file}"
        tap_fail ${case_id} "python in-tree round-trip failed"
    fi
fi

exit ${tap_status}
