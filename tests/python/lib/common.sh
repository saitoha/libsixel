#!/bin/sh
# Common helpers for Python TAP-style tests. The routines here consolidate
# Python discovery, shared-library lookup, and TAP utilities so individual
# test scripts can focus on scenario-specific assertions while sharing the
# same skip and setup logic.

# Enable verbose shell tracing by default so CI artifacts show the exact
# commands leading up to a failure. Set TRACE_SHELL=0 to opt out locally.
if [ "${TRACE_SHELL:-1}" -eq 1 ]; then
    set -xv
fi

# TAP bookkeeping and shared state. The variables are initialized to safe
# defaults so callers running with `set -u` do not trip on unbound names.
tap_status=0
tap_log_file=""
python_bin=""
run_python=""
lib_dir=""
shared_lib=""
python_bits=""
lib_bits=""
wheel_path=""
use_wheel=0
python_in_tree_pythonpath=""
python_in_tree_ld_library_path=""
python_wheel_ld_library_path=""
python_trace_dir=""
python_trace_pythonpath=""
python_trace_env=""
python_in_tree_trace_pythonpath=""

# Emit a TAP plan line.
tap_plan() {
    printf '1..%s\n' "$1"
}

# Mark a test case as passed.
tap_pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

# Mark a test case as failed and include the captured Python log when present.
tap_fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    if [ -n "${tap_log_file:-}" ] && [ -f "${tap_log_file}" ]; then
        printf '# python log follows\n'
        sed 's/^/# /' "${tap_log_file}"
    fi
    tap_status=1
}

# Skip the entire TAP file with a reason.
tap_skip_all() {
    printf '1..0 # SKIP %s\n' "$1"
    exit 0
}

# Locate the build output directory that should contain the libsixel library.
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

# Find a shared library matching libsixel on Unix-like and Windows/MSYS names.
find_shared_library() {
    search_root=$1

    # Prefer real shared objects/dylibs/DLLs and avoid import libraries such as
    # libsixel.dll.a that would crash when loaded via ctypes. Versioned sonames
    # (e.g. libsixel.so.1) are matched explicitly per suffix.
    for suffix in '.so' '.dylib' '.dll'; do
        for prefix in 'lib' ''; do
            case "${suffix}" in
                '.so')
                    patterns="${prefix}sixel*${suffix} ${prefix}sixel*${suffix}.*"
                    ;;
                '.dylib')
                    patterns="${prefix}sixel*${suffix}"
                    ;;
                '.dll')
                    patterns="${prefix}sixel*${suffix}"
                    ;;
            esac

            for pattern in ${patterns}; do
                candidate=$(find "${search_root}" -maxdepth 1 -type f \
                    -name "${pattern}" | head -n 1 || true)

                case "${candidate}" in
                    *.dll.a|*.dll.def)
                        candidate=""
                        ;;
                esac

                if [ -n "${candidate}" ]; then
                    printf '%s' "${candidate}"
                    return 0
                fi
            done
        done
    done

    return 1
}

# Determine the Python interpreter bit width.
detect_python_bits() {
    bin=$1

    ${bin} - <<'PY' 2>/dev/null || true
import struct
print(struct.calcsize("P") * 8)
PY
}

# Determine the shared library bit width for ELF/Mach-O/PE binaries.
detect_library_bits() {
    bin=$1
    path=$2

    ${bin} - <<PY 2>>"${tap_log_file}" || true
import pathlib
import struct


def _detect_elf_class(data):
    """Return '32' or '64' for ELF binaries, otherwise ''."""
    if not data.startswith(b"\x7fELF"):
        return ""
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


def _detect_pe_class(bin_path, data):
    """Return '32' or '64' for PE/COFF binaries, otherwise ''."""
    if not data.startswith(b"MZ") or len(data) < 0x40:
        return ""

    pe_offset = int.from_bytes(data[0x3C:0x40], byteorder="little")
    try:
        with bin_path.open("rb") as handle:
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


def detect(bin_path):
    try:
        with bin_path.open("rb") as handle:
            head = handle.read(512)
    except OSError:
        return ""

    for detector in (_detect_elf_class, _detect_macho_class):
        width = detector(head)
        if width:
            return width

    return _detect_pe_class(bin_path, head)


if __name__ == "__main__":
    lib_path = pathlib.Path(r"${path}")
    print(detect(lib_path))
PY
}

# Return the first wheel path under python-wheel/dist if it exists.
select_wheel() {
    build_root=$1
    wheel_dir="${build_root}/python-wheel/dist"

    if [ ! -d "${wheel_dir}" ]; then
        return 1
    fi

    find "${wheel_dir}" -maxdepth 1 -type f -name 'libsixel-*.whl' \
        | head -n 1 || true
}

# Verify that venv/ensurepip are available.
python_require_venv() {
    if ${python_bin} - <<'PY' >>"${tap_log_file}" 2>&1; then
import importlib.util
missing = [m for m in ("venv", "ensurepip")
           if importlib.util.find_spec(m) is None]
if missing:
    raise SystemExit(f"missing modules: {', '.join(missing)}")
PY
        return 0
    fi

    return 1
}

# Create a virtual environment and install the provided wheel.
python_install_wheel() {
    venv_path=$1
    target_wheel=$2

    if ! python_require_venv; then
        return 1
    fi

    run_python="${venv_path}/bin/python"

    if ${python_bin} -m venv "${venv_path}" >>"${tap_log_file}" 2>&1 \
       && "${run_python}" -m pip install --no-deps "${target_wheel}" \
            >>"${tap_log_file}" 2>&1; then
        return 0
    fi

    return 1
}

# Populate interpreter/library paths and architecture guards. The function
# sets shared globals so callers can re-use them without recalculating.
python_prepare() {
    tap_log_file=$1
    tmp_dir=$2

    python_bin=$(command -v python3 || command -v python || true)
    if [ -z "${python_bin}" ]; then
        tap_skip_all "python is not available"
    fi

    lib_dir=$(resolve_libdir "${TOP_BUILDDIR}") || true
    if [ -z "${lib_dir}" ]; then
        tap_skip_all "could not locate libsixel build output"
    fi

    shared_lib=$(find_shared_library "${lib_dir}" || true)
    if [ -z "${shared_lib}" ]; then
        tap_skip_all "libsixel shared library is unavailable (static-only build?)"
    fi

    python_bits=$(detect_python_bits "${python_bin}")
    lib_bits=$(detect_library_bits "${python_bin}" "${shared_lib}")

    python_trace_dir="${tmp_dir}/trace"
    python_trace_pythonpath="${python_trace_dir}"
    python_trace_env="SIXEL_PY_TRACE_LOG=${tap_log_file}"
    export SIXEL_PY_TRACE_LOG="${tap_log_file}"

    mkdir -p "${python_trace_dir}"
    cat >"${python_trace_dir}/sitecustomize.py" <<'PY'
"""Install a lightweight tracer for Python TAP tests."""
import os
import sys
import threading


def _build_logger():
    """Open the trace log defined via SIXEL_PY_TRACE_LOG."""
    log_path = os.environ.get("SIXEL_PY_TRACE_LOG")
    if not log_path:
        return None

    try:
        return open(log_path, "a", encoding="utf-8", buffering=1)
    except OSError:
        return None


_TRACE_HANDLE = _build_logger()


def _trace(frame, event, arg):
    """Write compact trace events to the shared log file."""
    if _TRACE_HANDLE is None:
        return None

    if event not in {"call", "line", "return", "exception"}:
        return _trace

    code = frame.f_code
    prefix = f"{event}:{code.co_filename}:{frame.f_lineno}:{code.co_name}"

    if event == "exception":
        exc_type, exc, _ = arg
        _TRACE_HANDLE.write(
            f"{prefix}: {exc_type.__name__}: {exc}\n"
        )
    elif event == "return":
        _TRACE_HANDLE.write(f"{prefix}: return\n")
    else:
        _TRACE_HANDLE.write(f"{prefix}\n")

    return _trace


if _TRACE_HANDLE is not None:
    sys.settrace(_trace)
    threading.settrace(_trace)
PY

    if [ -n "${tap_log_file}" ]; then
        printf 'python_bits=%s\n' "${python_bits}" >>"${tap_log_file}"
        printf 'lib_bits=%s\n' "${lib_bits}" >>"${tap_log_file}"
        printf 'lib_dir=%s\n' "${lib_dir}" >>"${tap_log_file}"
        printf 'shared_lib=%s\n' "${shared_lib}" >>"${tap_log_file}"
    fi

    if [ -n "${python_bits}" ] && [ -n "${lib_bits}" ] \
       && [ "${python_bits}" != "${lib_bits}" ]; then
        tap_skip_all "python is ${python_bits}-bit but libsixel is ${lib_bits}-bit"
    fi

    wheel_path=$(select_wheel "${TOP_BUILDDIR}" || true)
    if [ -n "${wheel_path}" ]; then
        use_wheel=1
    else
        use_wheel=0
    fi

    python_in_tree_pythonpath="${TOP_SRCDIR}/python"
    if [ -n "${PYTHONPATH-}" ]; then
        python_in_tree_pythonpath="${python_in_tree_pythonpath}:${PYTHONPATH}"
    fi

    python_in_tree_trace_pythonpath="${python_trace_pythonpath}"
    if [ -n "${python_in_tree_pythonpath}" ]; then
        python_in_tree_trace_pythonpath="${python_in_tree_trace_pythonpath}:${python_in_tree_pythonpath}"
    fi

    python_in_tree_ld_library_path="${lib_dir}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        python_in_tree_ld_library_path="${python_in_tree_ld_library_path}:${LD_LIBRARY_PATH}"
    fi

    python_wheel_ld_library_path="${lib_dir}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        python_wheel_ld_library_path="${python_wheel_ld_library_path}:${LD_LIBRARY_PATH}"
    fi

    run_python="${python_bin}"
}
