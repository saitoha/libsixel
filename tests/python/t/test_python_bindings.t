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

# Skip when the built shared library is linked against musl but the
# interpreter runs on glibc. A musl-built .so cannot be loaded by a glibc
# process and results in opaque 'invalid ELF header' errors during import.
# Detect libc types in Python so we do not rely on external tools and log
# the results for debugging.
python_libc=$(${python_bin} - <<'PY' 2>>"${log_file}" || true
import platform

name, version = platform.libc_ver()
print(name)
PY
)

lib_libc=$(${python_bin} - <<PY 2>>"${log_file}" || true
import pathlib


def detect_libc(path: pathlib.Path) -> str:
    """Return a best-effort libc name for the given shared object."""

    try:
        with path.open("rb") as handle:
            head = handle.read(65536)
    except OSError:
        return ""

    lowered = head.lower()
    if b"musl" in lowered:
        return "musl"
    if b"glibc" in lowered or b"gnu" in lowered:
        return "glibc"
    return ""


if __name__ == "__main__":
    lib_path = pathlib.Path(r"${shared_lib}")
    print(detect_libc(lib_path))
PY
)

printf 'python_libc=%s\n' "${python_libc}" >>"${log_file}"
printf 'lib_libc=%s\n' "${lib_libc}" >>"${log_file}"

if [ "${lib_libc}" = "musl" ] && [ "${python_libc}" != "musl" ]; then
    skip_all "libsixel is linked with ${lib_libc} but python uses ${python_libc}"
fi

if [ -n "${lib_libc}" ] && [ -n "${python_libc}" ] \
   && [ "${lib_libc}" != "${python_libc}" ]; then
    skip_all "libsixel is linked with ${lib_libc} but python uses ${python_libc}"
fi

# Abort early if the Python interpreter and the built shared library have
# different word sizes (for example, 64-bit Python vs. 32-bit libsixel),
# because such a mismatch always fails at import time with a confusing
# ELFCLASS error. The detection is implemented in Python to avoid depending
# on external tools such as `file`.
python_bits=$(${python_bin} - <<'PY' 2>/dev/null || true
import struct
print(struct.calcsize("P") * 8)
PY
)

lib_bits=$(${python_bin} - <<PY 2>>"${log_file}" || true
import pathlib
import struct
import sys


def _detect_elf_class(data):
    """Return '32' or '64' for ELF binaries, otherwise ''."""

    if not data.startswith(b"\x7fELF"):
        return ""

    # EI_CLASS sits at byte 4 and discriminates 32/64-bit objects.
    elf_class = data[4]
    if elf_class == 1:
        return "32"
    if elf_class == 2:
        return "64"
    return ""


def _detect_macho_class(data):
    """Return '32' or '64' for Mach-O binaries, otherwise ''."""

    magic = data[:4]
    macho_32 = (0xfeedface, 0xcefaedfe)
    macho_64 = (0xfeedfacf, 0xcffaedfe)

    value = int.from_bytes(magic, byteorder="big", signed=False)
    if value in macho_32:
        return "32"
    if value in macho_64:
        return "64"
    return ""


def _detect_pe_class(path, data):
    """Return '32' or '64' for PE/COFF binaries, otherwise ''."""

    if not data.startswith(b"MZ") or len(data) < 0x40:
        return ""

    pe_offset = int.from_bytes(data[0x3C:0x40], byteorder="little")
    try:
        with path.open("rb") as handle:
            handle.seek(pe_offset)
            signature = handle.read(6)
    except OSError:
        return ""

    if not signature.startswith(b"PE\0\0") or len(signature) < 6:
        return ""

    machine = struct.unpack("<H", signature[4:6])[0]
    if machine in (0x014c,):
        return "32"
    if machine in (0x8664,):
        return "64"
    return ""


def detect(path):
    try:
        with path.open("rb") as handle:
            head = handle.read(512)
    except OSError:
        return ""

    for detector in (_detect_elf_class, _detect_macho_class):
        width = detector(head)
        if width:
            return width

    return _detect_pe_class(path, head)


if __name__ == "__main__":
    lib_path = pathlib.Path(r"${shared_lib}")
    print(detect(lib_path))
PY
)

printf 'python_bits=%s\n' "${python_bits}" >>"${log_file}"
printf 'lib_bits=%s\n' "${lib_bits}" >>"${log_file}"

if [ -n "${python_bits}" ] && [ -n "${lib_bits}" ] \
   && [ "${python_bits}" != "${lib_bits}" ]; then
    skip_all "python is ${python_bits}-bit but libsixel is ${lib_bits}-bit"
fi

# Abort early if the Python interpreter and the built shared library have
# different word sizes (for example, 64-bit Python vs. 32-bit libsixel),
# because such a mismatch always fails at import time with a confusing
# ELFCLASS error. We rely on the `file` utility when available to extract
# the bitness of the shared library and compare it with Python's pointer
# size.
python_bits=$(${python_bin} - <<'PY' 2>/dev/null || true
import struct
print(struct.calcsize("P") * 8)
PY
)
lib_bits=""
if command -v file >/dev/null 2>&1; then
    lib_desc=$(file -b "${shared_lib}" 2>/dev/null || true)
    case "${lib_desc}" in
        *64-bit*) lib_bits="64" ;;
        *32-bit*) lib_bits="32" ;;
    esac
fi

if [ -n "${python_bits}" ] && [ -n "${lib_bits}" ] \
   && [ "${python_bits}" != "${lib_bits}" ]; then
    skip_all "python is ${python_bits}-bit but libsixel is ${lib_bits}-bit"
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
import glob
import os
import pathlib


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


ctypes.util.find_library = (
    lambda name, _orig=ctypes.util.find_library:
    _prefer_build_library(name, _orig)
)

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
