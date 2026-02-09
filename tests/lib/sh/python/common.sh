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

# Resolve a writable TAP log destination that works across POSIX and
# MSYS/MinGW shells. Some Windows runners do not expose /dev/stderr, so
# prefer descriptor aliases when available and fall back to a regular file.
resolve_tap_log_file() {
    candidate=""
    fallback=""

    # cmd.exe-launched Git Bash can report /dev/stderr as present even when
    # later append redirections fail with "No such file or directory".
    # Prefer a regular file on Windows-like shells to keep logging reliable.
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            candidate=""
            ;;
        *)
            for candidate in /dev/stderr /proc/self/fd/2 /dev/fd/2; do
                if [ -e "${candidate}" ] \
                   && (: >>"${candidate}") 2>/dev/null; then
                    printf '%s' "${candidate}"
                    return 0
                fi
            done
            ;;
    esac

    fallback="${TMPDIR:-/tmp}/libsixel-python-tap-$$.log"
    if (: >"${fallback}") 2>/dev/null; then
        printf '%s' "${fallback}"
        return 0
    fi

    printf '%s' "./libsixel-python-tap-$$.log"
}

# TAP bookkeeping and shared state. The variables are initialized to safe
# defaults so callers running with `set -u` do not trip on unbound names.
tap_log_file="$(resolve_tap_log_file)"
python_bin=""
run_python=""
lib_dir=""
shared_lib=""
python_bits=""
python_arch=""
lib_bits=""
lib_arch=""
python_libc=""
lib_libc=""
wheel_path=""
use_wheel=0
python_in_tree_pythonpath=""
python_in_tree_ld_library_path=""
python_wheel_ld_library_path=""
python_in_tree_loader_env=""
python_wheel_loader_env=""
python_trace_dir=""
python_trace_pythonpath=""
python_trace_env=""
python_trace_prefixes=""
python_in_tree_trace_pythonpath=""
python_wheel_trace_pythonpath=""
python_lib_dir=""
python_needs_windows_paths=0

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)

# Load shared TAP helpers once per shell.
python_common_path=${python_common_path:-"$0"}
python_helper_dir=${PYTHON_HELPER_DIR-}
if [ -z "${python_helper_dir}" ]; then
    python_helper_dir=$(CDPATH=; cd "$(dirname "${python_common_path}")" && pwd)
fi

if [ "x${SIXEL_TSAN_BUILD}" = xyes ]; then
    tap_skip_all "python extension does not works with TSan build"
fi

if [ "x${SIXEL_MSAN_BUILD}" = xyes ]; then
    tap_skip_all "python extension does not works with MSan build"
fi


# Skip all tests when a Python process failed because the dynamic loader could
# not import the libsixel shared library. Python snippets emit the
# SKIP_LIBSIXEL_LOAD marker on OSError so shell callers can detect the
# condition and bail out cleanly instead of reporting a false failure during
# cross-builds. The helper always returns success so callers running with
# `set -e` do not abort before emitting TAP results.
python_skip_on_load_error() {
    status=$1
    log_data=$2
    log_text=""

    if [ "${status}" -eq 0 ]; then
        return 0
    fi

    if [ -n "${log_data}" ] && [ -f "${log_data}" ]; then
        log_text=$(cat "${log_data}")
    else
        log_text=${log_data}
    fi

    # macOS TSan builds can emit an interceptor warning when the runtime is
    # loaded too late (via dlopen). The error is printed to stderr, not raised
    # as a Python exception, so detect it in the captured log and skip the
    # Python binding tests to align with the intended policy.
    if printf '%s' "${log_text}" | grep -q "Interceptors are not working"; then
        tap_skip_all "ThreadSanitizer interceptors not available"
    fi

    marker=$(printf '%s' "${log_text}" | grep -m1 '^SKIP_LIBSIXEL_LOAD:' \
        || true)
    if [ -z "${marker}" ]; then
        printf 'python_skip_on_load_error: no load marker for status %s\n' \
            "${status}" >&2
        return 0
    fi

    reason=${marker#SKIP_LIBSIXEL_LOAD:}
    reason=$(printf '%s' "${reason}" | sed 's/^ *//')
    tap_skip_all "libsixel failed to load: ${reason}"
}

# Return success if python bindings are enabled for the current build.
python_bindings_enabled() {
    python_exec=$1
    build_root=$2

    if [ -z "${build_root}" ]; then
        return 1
    fi

    options_path="${build_root}/meson-info/intro-buildoptions.json"
    if [ -f "${options_path}" ]; then
        "${python_exec}" - "${options_path}" <<'PY'
import json
import sys

path = sys.argv[1]

with open(path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

for entry in data:
    if entry.get("name") == "python":
        value = entry.get("value")
        if value in ("enabled", True):
            sys.exit(0)
        sys.exit(1)

sys.exit(1)
PY
        return $?
    fi

    if [ -f "${build_root}/python/Makefile" ]; then
        return 0
    fi

    return 1
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

    # trace dirs
    find . -type d

    return 1
}

# Filter PYTHONPATH-style strings so only existing directories remain. This
# prevents accidental insertion of script files (e.g., verify-errors.py) into
# sys.path, which would trigger zipimport errors on Windows runners.
sanitize_pythonpath() {
    raw_path=$1
    sep=$2

    cleaned=""

    IFS="${sep}"
    for entry in ${raw_path}; do
        unset IFS
        if [ -d "${entry}" ]; then
            if [ -z "${cleaned}" ]; then
                cleaned="${entry}"
            else
                cleaned="${cleaned}${sep}${entry}"
            fi
        else
            if [ -n "${tap_log_file:-}" ]; then
                printf 'skipping non-directory PYTHONPATH entry: %s\n' \
                    "${entry}" >>"${tap_log_file}"
            fi
        fi
        IFS="${sep}"
    done
    unset IFS

    printf '%s' "${cleaned}"
}

# Convert a single path to Windows form when running against a native
# (msvcrt-linked) interpreter. The function falls back to the original path
# when cygpath is unavailable so POSIX callers remain unaffected.
python_to_windows_path() {
    target=$1

    if [ "${python_needs_windows_paths}" -ne 1 ]; then
        printf '%s' "${target}"
        return 0
    fi

    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "${target}" 2>/dev/null || printf '%s' "${target}"
        return 0
    fi

    if [ -n "${tap_log_file:-}" ]; then
        printf 'cygpath missing, leaving POSIX path as-is: %s\n' \
            "${target}" >>"${tap_log_file}"
    fi

    printf '%s' "${target}"
}

# Apply Windows path normalization to every entry in a separator-delimited
# string. Empty inputs are preserved to avoid altering intentional blanks in
# PYTHONPATH-like variables.
python_normalize_pathlist() {
    raw=$1
    sep=$2

    if [ -z "${raw}" ]; then
        printf '%s' "${raw}"
        return 0
    fi

    if [ "${python_needs_windows_paths}" -eq 1 ] && [ "${sep}" = ":" ]; then
        if command -v cygpath >/dev/null 2>&1; then
            cygpath -m -p "${raw}" 2>/dev/null || printf '%s' "${raw}"
            return 0
        fi
    fi

    normalized=""

    IFS="${sep}"
    for entry in ${raw}; do
        unset IFS

        converted=$(python_to_windows_path "${entry}")

        if [ -z "${normalized}" ]; then
            normalized="${converted}"
        else
            normalized="${normalized}${sep}${converted}"
        fi

        IFS="${sep}"
    done
    unset IFS

    printf '%s' "${normalized}"
}

# Find a shared library matching libsixel on Unix-like and Windows/MSYS names.
find_shared_library() {
    search_root=$1
    expected_bits=$2

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
                    if [ -n "${expected_bits}" ] && \
                       [ -n "${python_bin}" ]; then
                        candidate_bits=$(detect_library_bits \
                            "${python_bin}" "${candidate}" || true)
                        if [ -n "${candidate_bits}" ] && \
                           [ "${candidate_bits}" != "${expected_bits}" ]; then
                            if [ -n "${tap_log_file:-}" ]; then
                                printf 'skip %s: %s-bit lib does not match %s-bit python\n' \
                                    "${candidate}" "${candidate_bits}" \
                                    "${expected_bits}" >>"${tap_log_file}"
                            fi
                            continue
                        fi
                    fi

                    printf '%s' "${candidate}"
                    return 0
                fi
            done
        done
    done

    return 1
}

# Detect whether the shared library carries AddressSanitizer instrumentation so
# Python bindings can be skipped when ASan runtimes would otherwise break
# import-time interceptors.
library_built_with_asan() {
    target=$1

    if command -v nm >/dev/null 2>&1; then
        if nm -an "${target}" 2>/dev/null | grep -q "__asan_init"; then
            if [ -n "${tap_log_file:-}" ]; then
                printf 'asan detected via nm in %s\n' "${target}" \
                    >>"${tap_log_file}"
            fi
            return 0
        fi
    fi

    if command -v strings >/dev/null 2>&1; then
        if strings "${target}" 2>/dev/null | \
           grep -qiE 'libclang_rt\.asan|asan\.dll|__asan_init'; then
            if [ -n "${tap_log_file:-}" ]; then
                printf 'asan detected via strings in %s\n' "${target}" \
                    >>"${tap_log_file}"
            fi
            return 0
        fi
    fi

    return 1
}

# Detect whether the shared library carries ThreadSanitizer instrumentation.
# We use this to skip Python extension tests on macOS where the system
# Python runtime is not built with TSan and triggers false positives.
library_built_with_tsan() {
    target=$1
    tsan_runtime_regex='libclang_rt\.tsan(_osx_dynamic)?'

    # Prefer checking runtime dependencies on macOS to avoid missing symbols
    # after stripping or LTO. This keeps TSan detection stable in CI.
    if command -v otool >/dev/null 2>&1; then
        if otool -L "${target}" 2>/dev/null | \
           grep -qiE "${tsan_runtime_regex}"; then
            if [ -n "${tap_log_file:-}" ]; then
                printf 'tsan detected via otool in %s\n' "${target}" \
                    >>"${tap_log_file}"
            fi
            return 0
        fi
    fi

    if command -v nm >/dev/null 2>&1; then
        if nm -an "${target}" 2>/dev/null | grep -q "__tsan_init"; then
            if [ -n "${tap_log_file:-}" ]; then
                printf 'tsan detected via nm in %s\n' "${target}" \
                    >>"${tap_log_file}"
            fi
            return 0
        fi
    fi

    if command -v strings >/dev/null 2>&1; then
        if strings "${target}" 2>/dev/null | \
           grep -qiE "${tsan_runtime_regex}|tsan\.dll|__tsan_init"; then
            if [ -n "${tap_log_file:-}" ]; then
                printf 'tsan detected via strings in %s\n' "${target}" \
                    >>"${tap_log_file}"
            fi
            return 0
        fi
    fi

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

# Determine the Python interpreter architecture string (e.g., x86_64).
detect_python_arch() {
    bin=$1

    ${bin} - <<'PY' 2>/dev/null || true
import platform

print(platform.machine())
PY
}

# Determine the Python interpreter's libc family so glibc/musl mismatches can
# be skipped before the shared object is imported.
detect_python_libc() {
    bin=$1

    ${bin} - <<'PY' 2>/dev/null || true
import os
import pathlib
import platform
import sys


def _detect_windows_libc():
    """Return the Windows CRT dependency of the current interpreter."""

    exe = pathlib.Path(sys.executable)
    try:
        payload = exe.read_bytes().lower()
    except OSError:
        return "msvcrt"

    if b"ucrtbase.dll" in payload or b"vcruntime" in payload:
        return "ucrt"
    if b"msvcrt.dll" in payload:
        return "msvcrt"

    return "msvcrt"


def main():
    """Detect libc family in a platform-specific manner."""

    # platform.libc_ver() returns ('glibc', version) on glibc and ('musl', '')
    # on musl. Normalize to stable strings and fall back to an empty marker
    # when the libc cannot be determined.
    name, _ = platform.libc_ver()
    name = name.lower().strip()

    if name:
        print(name)
        return

    if os.name == "nt":
        print(_detect_windows_libc())
        return

    print("")


if __name__ == "__main__":
    main()
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

# Determine the shared library architecture string so cross-builds can be
# skipped cleanly when the target differs from the host interpreter.
detect_library_arch() {
    bin=$1
    path=$2

    ${bin} - <<PY 2>>"${tap_log_file}" || true
import pathlib
import struct


def _elf_machine(head):
    if not head.startswith(b"\x7fELF") or len(head) < 20:
        return ""

    machine = int.from_bytes(head[18:20], byteorder="little")
    mapping = {
        0x03: "i386",
        0x08: "mips",
        0x14: "ppc",
        0x15: "ppc64",
        0x28: "arm",
        0x3E: "x86_64",
        0xB7: "aarch64",
    }
    return mapping.get(machine, f"elf-{machine}")


def _macho_machine(head):
    macho_32 = (0xfeedface, 0xcefaedfe)
    macho_64 = (0xfeedfacf, 0xcffaedfe)

    if len(head) < 12:
        return ""

    magic = int.from_bytes(head[:4], byteorder="big", signed=False)
    if magic not in macho_32 + macho_64:
        return ""

    byteorder = "big"
    if magic in (0xcefaedfe, 0xcffaedfe):
        byteorder = "little"

    cputype = int.from_bytes(head[4:8], byteorder=byteorder, signed=True)
    mapping = {
        7: "i386",
        0x01000007: "x86_64",
        12: "arm",
        0x0100000C: "arm64",
    }
    return mapping.get(cputype, f"macho-{cputype}")


def _pe_machine(head, path):
    if not head.startswith(b"MZ") or len(head) < 0x40:
        return ""

    pe_offset = int.from_bytes(head[0x3C:0x40], byteorder="little")
    try:
        with path.open("rb") as handle:
            handle.seek(pe_offset)
            signature = handle.read(6)
    except OSError:
        return ""

    if not signature.startswith(b"PE\0\0") or len(signature) < 6:
        return ""

    machine = struct.unpack("<H", signature[4:6])[0]
    mapping = {
        0x014c: "i386",
        0x8664: "x86_64",
        0x01c0: "arm",
        0xAA64: "arm64",
    }
    return mapping.get(machine, f"pe-{machine}")


def detect(path):
    try:
        with path.open("rb") as handle:
            head = handle.read(512)
    except OSError:
        return ""

    for detector in (_elf_machine, _macho_machine):
        arch = detector(head)
        if arch:
            return arch

    return _pe_machine(head, path)


if __name__ == "__main__":
    lib_path = pathlib.Path(r"${path}")
    print(detect(lib_path))
PY
}

# Determine the shared library's libc family. ELF binaries are inspected for
# interpreter/loader hints while PE binaries are scanned for CRT import
# strings. Unknown formats return an empty marker.
detect_library_libc() {
    bin=$1
    path=$2

    ${bin} - <<PY 2>>"${tap_log_file}" || true
import pathlib


def _detect_elf_libc(head):
    """Return glibc/musl markers for ELF binaries."""

    musl_markers = (b"/ld-musl", b"musl")
    glibc_markers = (b"/ld-linux", b"glibc")

    for marker in musl_markers:
        if marker in head:
            return "musl"

    for marker in glibc_markers:
        if marker in head:
            return "glibc"

    return ""


def _detect_pe_libc(payload):
    """Return CRT family for PE binaries based on import table strings."""

    lower = payload.lower()

    if b"ucrtbase.dll" in lower or b"vcruntime" in lower:
        return "ucrt"
    if b"msvcrt.dll" in lower:
        return "msvcrt"

    return ""


def detect(path):
    try:
        payload = path.read_bytes()
    except OSError:
        return ""

    if payload.startswith(b"\x7fELF"):
        return _detect_elf_libc(payload)

    if payload.startswith(b"MZ"):
        return _detect_pe_libc(payload)

    return ""


if __name__ == "__main__":
    lib_path = pathlib.Path(r"${path}")
    print(detect(lib_path))
PY
}

# Return the first wheel path under python/dist if it exists.
select_wheel() {
    build_root=$1
    wheel_dir="${build_root}/python/dist"

    if [ ! -d "${wheel_dir}" ]; then
        return 1
    fi

    find "${wheel_dir}" -maxdepth 1 -type f -name 'libsixel_wheel-*.whl' \
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

    if [ -n "${SIXEL_TEST_PYTHON_VENV:-}" ] \
       && [ -x "${SIXEL_TEST_PYTHON_VENV}/bin/python" ]; then
        run_python="${SIXEL_TEST_PYTHON_VENV}/bin/python"
        return 0
    fi

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
    tmp_dir=$1
    tap_log_file="$(resolve_tap_log_file)"

    python_bin=$(command -v python3 || command -v python || true)
    if [ -z "${python_bin}" ]; then
        tap_skip_all "python is not available"
    fi

    if [ -n "${MESON_BUILD_ROOT:-}" ]; then
        build_root="${MESON_BUILD_ROOT}"
    elif [ -n "${TOP_BUILDDIR:-}" ]; then
        build_root="${TOP_BUILDDIR}"
    else
        build_root=$(CDPATH=; cd "${script_dir}/../../.." && pwd)
    fi

    if ! python_bindings_enabled "${python_bin}" "${build_root}"; then
        tap_skip_all "python bindings disabled in this build"
    fi

    python_bits=$(detect_python_bits "${python_bin}")
    python_arch=$(detect_python_arch "${python_bin}")
    python_arch=$(printf '%s' "${python_arch}" | tr 'A-Z' 'a-z')
    python_libc=$(detect_python_libc "${python_bin}")
    python_libc=$(printf '%s' "${python_libc}" | tr 'A-Z' 'a-z')

    python_needs_windows_paths=0
    if [ "${python_libc}" = "msvcrt" ]; then
        python_needs_windows_paths=1
    fi

    # Derive a stable path separator without invoking the interpreter, as
    # Python's stdout line endings on MSYS environments can inject stray
    # carriage returns. A simple uname check keeps PYTHONPATH predictable.
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            python_pathsep=';'
            ;;
        *)
            python_pathsep=':'
            ;;
    esac

    lib_dir=$(resolve_libdir "${TOP_BUILDDIR}") || true
    if [ -z "${lib_dir}" ]; then
        tap_skip_all "could not locate libsixel build output"
    fi

    shared_lib=$(find_shared_library "${lib_dir}" "${python_bits}" || true)
    if [ -z "${shared_lib}" ]; then
        tap_skip_all "missing libsixel shared library for ${python_bits}-bit python"
    fi

    if library_built_with_asan "${shared_lib}"; then
        tap_skip_all "libsixel built with AddressSanitizer"
    fi
    case "$(uname -s)" in
        Darwin)
            if library_built_with_tsan "${shared_lib}"; then
                tap_skip_all "macOS TSan run (python runtime is not sanitized)"
            fi
            ;;
    esac

    lib_bits=$(detect_library_bits "${python_bin}" "${shared_lib}")
    lib_arch=$(detect_library_arch "${python_bin}" "${shared_lib}")
    lib_arch=$(printf '%s' "${lib_arch}" | tr 'A-Z' 'a-z')
    lib_libc=$(detect_library_libc "${python_bin}" "${shared_lib}")
    lib_libc=$(printf '%s' "${lib_libc}" | tr 'A-Z' 'a-z')

    python_trace_dir="${tmp_dir}"
    python_trace_pythonpath="${python_trace_dir}"
    python_trace_prefixes="${TOP_SRCDIR}${python_pathsep}${tmp_dir}"
    cat >"${python_trace_dir}/sitecustomize.py" <<'PY'
"""Install a scoped tracer for Python TAP tests.

The tracer is intentionally selective to avoid multi-megabyte logs on
platforms with verbose import paths (e.g., MSYS/MinGW). Only frames whose
filename starts with one of the prefixes listed in SIXEL_PY_TRACE_PREFIXES are
recorded.
"""
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


def _load_prefixes():
    """Normalize allowed filename prefixes for tracing.

    The prefixes mirror PYTHONPATH semantics so platform-specific separators
    work on both POSIX and Windows runners.
    """

    raw = os.environ.get("SIXEL_PY_TRACE_PREFIXES", "")
    if not raw:
        return []

    prefixes = []
    for entry in raw.split(os.pathsep):
        if entry:
            prefixes.append(os.path.normcase(entry))
    return prefixes


_TRACE_HANDLE = _build_logger()
_TRACE_PREFIXES = _load_prefixes()


def _should_trace(filename):
    """Return True when the file should be traced."""

    if not _TRACE_PREFIXES:
        return True

    normalized = os.path.normcase(filename)
    for prefix in _TRACE_PREFIXES:
        if normalized.startswith(prefix):
            return True

    return False


def _trace(frame, event, arg):
    """Write compact trace events to the shared log file."""

    if _TRACE_HANDLE is None:
        return None

    if event not in {"call", "line", "return", "exception"}:
        return _trace

    code = frame.f_code
    if not _should_trace(code.co_filename):
        return _trace

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
        printf 'python_arch=%s\n' "${python_arch}" >>"${tap_log_file}"
        printf 'lib_bits=%s\n' "${lib_bits}" >>"${tap_log_file}"
        printf 'lib_arch=%s\n' "${lib_arch}" >>"${tap_log_file}"
        printf 'python_libc=%s\n' "${python_libc}" >>"${tap_log_file}"
        printf 'lib_libc=%s\n' "${lib_libc}" >>"${tap_log_file}"
        printf 'lib_dir=%s\n' "${lib_dir}" >>"${tap_log_file}"
        printf 'shared_lib=%s\n' "${shared_lib}" >>"${tap_log_file}"
    fi

    if [ -n "${python_bits}" ] && [ -n "${lib_bits}" ] \
       && [ "${python_bits}" != "${lib_bits}" ]; then
        tap_skip_all "python is ${python_bits}-bit but libsixel is ${lib_bits}-bit"
    fi

    if [ -n "${python_arch}" ] && [ -n "${lib_arch}" ] \
       && [ "${python_arch}" != "${lib_arch}" ]; then
        tap_skip_all "python architecture ${python_arch} does not match libsixel ${lib_arch}"
    fi

    if [ -n "${python_libc}" ] && [ -n "${lib_libc}" ] \
       && [ "${python_libc}" != "${lib_libc}" ]; then
        tap_skip_all "python libc ${python_libc} does not match libsixel ${lib_libc}"
    fi

    wheel_path=$(select_wheel "${TOP_BUILDDIR}" || true)
    if [ -n "${wheel_path}" ]; then
        use_wheel=1
    else
        use_wheel=0
    fi

    python_in_tree_pythonpath="${TOP_SRCDIR}/python"
    if [ -n "${PYTHONPATH-}" ]; then
        cleaned_pythonpath=$(sanitize_pythonpath "${PYTHONPATH}" \
            "${python_pathsep}")
        if [ -n "${cleaned_pythonpath}" ]; then
            python_in_tree_pythonpath="${python_in_tree_pythonpath}${python_pathsep}${cleaned_pythonpath}"
        fi
    fi

    python_in_tree_trace_pythonpath="${python_trace_pythonpath}"
    if [ -n "${python_in_tree_pythonpath}" ]; then
        python_in_tree_trace_pythonpath="${python_in_tree_trace_pythonpath}${python_pathsep}${python_in_tree_pythonpath}"
    fi

    python_wheel_trace_pythonpath="${python_trace_pythonpath}"
    if [ -n "${python_in_tree_pythonpath}" ]; then
        python_wheel_trace_pythonpath="${python_wheel_trace_pythonpath}${python_pathsep}${python_in_tree_pythonpath}"
    fi

    python_in_tree_ld_library_path="${lib_dir}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        python_in_tree_ld_library_path="${python_in_tree_ld_library_path}:${LD_LIBRARY_PATH}"
    fi

    python_wheel_ld_library_path="${lib_dir}"
    if [ -n "${LD_LIBRARY_PATH-}" ]; then
        python_wheel_ld_library_path="${python_wheel_ld_library_path}:${LD_LIBRARY_PATH}"
    fi

    python_lib_dir="${lib_dir}"

    if [ "${python_needs_windows_paths}" -eq 1 ]; then
        python_trace_pythonpath=$(python_normalize_pathlist \
            "${python_trace_pythonpath}" "${python_pathsep}")
        python_in_tree_pythonpath=$(python_normalize_pathlist \
            "${python_in_tree_pythonpath}" "${python_pathsep}")
        python_in_tree_trace_pythonpath=$(python_normalize_pathlist \
            "${python_in_tree_trace_pythonpath}" "${python_pathsep}")
        python_wheel_trace_pythonpath=$(python_normalize_pathlist \
            "${python_wheel_trace_pythonpath}" "${python_pathsep}")
        python_trace_prefixes=$(python_normalize_pathlist \
            "${python_trace_prefixes}" "${python_pathsep}")
        python_lib_dir=$(python_to_windows_path "${python_lib_dir}")
        python_in_tree_ld_library_path=$(python_normalize_pathlist \
            "${python_in_tree_ld_library_path}" ':')
        python_wheel_ld_library_path=$(python_normalize_pathlist \
            "${python_wheel_ld_library_path}" ':')

        if [ -n "${wheel_path}" ]; then
            wheel_path=$(python_to_windows_path "${wheel_path}")
        fi
    fi

    python_in_tree_loader_env="LD_LIBRARY_PATH=${python_in_tree_ld_library_path}"
    python_wheel_loader_env="LD_LIBRARY_PATH=${python_wheel_ld_library_path}"

    if [ "$(uname -s)" = "Darwin" ]; then
        python_in_tree_loader_env="${python_in_tree_loader_env} DYLD_LIBRARY_PATH=${python_in_tree_ld_library_path}"
        python_wheel_loader_env="${python_wheel_loader_env} DYLD_LIBRARY_PATH=${python_wheel_ld_library_path}"
    fi

    python_trace_env="SIXEL_PY_TRACE_LOG=${tap_log_file}"
    export SIXEL_PY_TRACE_LOG="${tap_log_file}"

    python_trace_env="${python_trace_env} SIXEL_PY_TRACE_PREFIXES=${python_trace_prefixes}"
    export SIXEL_PY_TRACE_PREFIXES="${python_trace_prefixes}"

    if [ -n "${tap_log_file}" ]; then
        printf 'python_lib_dir_normalized=%s\n' "${python_lib_dir}" \
            >>"${tap_log_file}"
        printf 'python_trace_pythonpath=%s\n' "${python_trace_pythonpath}" \
            >>"${tap_log_file}"
    fi

    run_python="${python_bin}"
}
