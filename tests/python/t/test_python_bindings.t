#!/bin/sh
# TAP smoke test for Python bindings. The test prefers the prebuilt wheel
# under python-wheel/dist when available and falls back to the in-tree Python
# sources otherwise.

set -eu

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    if [ -f "${log_file}" ]; then
        printf '# python log follows\n'
        sed 's/^/# /' "${log_file}"
    fi
    status=1
}

skip_all() {
    printf '1..0 # SKIP %s\n' "$1"
    exit 0
}

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/python.log"
tmp_dir="${artifact_dir}/tmp"
run_python=""
use_wheel=0

mkdir -p "${artifact_dir}" "${tmp_dir}"
: >"${log_file}"

resolve_libdir() {
    build_root=$1

    if [ -d "${build_root}/src/.libs" ]; then
        printf '%s' "${build_root}/src/.libs"
        return 0
    fi

    if [ -d "${build_root}/src" ]; then
        printf '%s' "${build_root}/src"
        return 0
    fi

    return 1
}

python_bin=$(command -v python3 || command -v python || true)
if [ -z "${python_bin}" ]; then
    skip_all "python is not available"
fi

# Locate the build output that should contain the shared library. Static-only
# builds do not produce a loadable .so/.dylib/.dll, so we skip to avoid
# spurious failures when the Python bindings cannot be imported.
lib_path=$(resolve_libdir "${TOP_BUILDDIR}") || true
if [ -z "${lib_path}" ]; then
    skip_all "could not locate libsixel build output"
fi

shared_lib=$(find "${lib_path}" -maxdepth 1 -type f \
    \( -name 'lib*sixel*.so*' -o -name 'lib*sixel*.dylib' \
       -o -name '*sixel*.dll' \) \
    | head -n 1 || true)
if [ -z "${shared_lib}" ]; then
    skip_all "libsixel shared library is unavailable (static-only build?)"
fi

wheel_dir="${TOP_BUILDDIR}/python-wheel/dist"
if [ -d "${wheel_dir}" ]; then
    wheel_path=$(find "${wheel_dir}" -maxdepth 1 -type f -name 'libsixel-*.whl' \
        | head -n 1 || true)
    if [ -n "${wheel_path}" ]; then
        use_wheel=1
    fi
fi

echo "1..2"
status=0
case_id=1

if [ "${use_wheel}" -eq 1 ]; then
    # Require venv/ensurepip to isolate the wheel from system packages.
    if "${python_bin}" - <<'PY' >>"${log_file}" 2>&1; then
import importlib.util
missing = [m for m in ("venv", "ensurepip")
           if importlib.util.find_spec(m) is None]
if missing:
    raise SystemExit(f"missing modules: {', '.join(missing)}")
PY
        :
    else
        skip_all "python lacks venv or ensurepip support"
    fi

    run_venv="${tmp_dir}/venv"
    run_python="${run_venv}/bin/python"

    if "${python_bin}" -m venv "${run_venv}" >>"${log_file}" 2>&1 \
       && "${run_python}" -m pip install --no-deps "${wheel_path}" \
            >>"${log_file}" 2>&1; then
        pass ${case_id} "installs wheel from python-wheel/dist"
    else
        fail ${case_id} "wheel installation failed"
    fi
else
    run_python="${python_bin}"
    in_tree_pythonpath="${TOP_SRCDIR}/python"

    pythonpath_env="${in_tree_pythonpath}"
    if [ -n "${PYTHONPATH-}" ]; then
        pythonpath_env="${pythonpath_env}:${PYTHONPATH}"
    fi

    ld_library_path_env="${lib_path}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        ld_library_path_env="${ld_library_path_env}:${LD_LIBRARY_PATH}"
    fi

    if PYTHONPATH="${pythonpath_env}" \
       LD_LIBRARY_PATH="${ld_library_path_env}" \
       LIBSIXEL_LIBDIR="${lib_path}" \
       "${run_python}" - <<'PY' >>"${log_file}" 2>&1; then
import libsixel
from libsixel import encoder, decoder

encoder.Encoder
decoder.Decoder
PY
        pass ${case_id} "imports in-tree python modules"
    else
        fail ${case_id} "failed to import in-tree python modules"
    fi
fi

case_id=$((case_id + 1))
verify_script="${tmp_dir}/verify-bindings.py"
cat >"${verify_script}" <<'PY'
import ctypes.util
import os
import pathlib


def _prefer_build_library(name, original_find):
    libdir = os.environ.get("LIBSIXEL_LIBDIR")
    if libdir:
        candidate = os.path.join(libdir, f"lib{name}.so")
        if os.path.exists(candidate):
            return candidate

    return original_find(name)


ctypes.util.find_library = lambda name, _orig=ctypes.util.find_library: _prefer_build_library(name, _orig)

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
PY

python_env="${run_python}"
libdir="${lib_path}"

if [ -z "${libdir}" ]; then
    fail ${case_id} "could not locate libsixel build output"
    exit ${status}
fi

export LIBSIXEL_LIBDIR="${libdir}"

if [ "${use_wheel}" -eq 1 ]; then
    ld_library_path_env="${libdir}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        ld_library_path_env="${ld_library_path_env}:${LD_LIBRARY_PATH}"
    fi

    if PYTHONPATH="" \
       LD_LIBRARY_PATH="${ld_library_path_env}" \
       "${python_env}" "${verify_script}" >>"${log_file}" 2>&1; then
        pass ${case_id} "encodes image via wheel"
    else
        fail ${case_id} "python wheel round-trip failed"
    fi
else
    pythonpath_env="${TOP_SRCDIR}/python"
    if [ -n "${PYTHONPATH-}" ]; then
        pythonpath_env="${pythonpath_env}:${PYTHONPATH}"
    fi

    ld_library_path_env="${libdir}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        ld_library_path_env="${ld_library_path_env}:${LD_LIBRARY_PATH}"
    fi

    if PYTHONPATH="${pythonpath_env}" \
       LD_LIBRARY_PATH="${ld_library_path_env}" \
       "${python_env}" "${verify_script}" >>"${log_file}" 2>&1; then
        pass ${case_id} "encodes image via in-tree modules"
    else
        fail ${case_id} "python in-tree round-trip failed"
    fi
fi

exit ${status}
