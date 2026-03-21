#!/bin/sh
# Prepare a shared Python test virtualenv and print a shell-safe assignment.
#
# Usage:
#   resolve-python-test-venv.sh <enable_python> <python_bin> <wheel_dir> <venv_dir> [source_dir] [libsixel_lib_dir]

set -eu

if [ "$#" -ne 4 ] && [ "$#" -ne 6 ]; then
    echo "Usage: $0 <enable_python> <python_bin> <wheel_dir> <venv_dir> [source_dir] [libsixel_lib_dir]" >&2
    exit 1
fi

enable_python=$1
python_bin=$2
wheel_dir=$3
shared_python_venv=$4
source_dir=${5-}
libsixel_lib_dir=${6-}
wheel_path=
wheel_marker=
wheel_signature=
source_signature=
combined_signature=

quote_single() {
    if [ -z "$1" ]; then
        printf "''"
        return 0
    fi
    printf "%s" "$1" | sed "s/'/'\\\\''/g;1s/^/'/;\$s/\$/'/"
}

emit_assignment() {
    value=$1
    quoted_value=$(quote_single "$value")
    printf "SIXEL_TEST_PYTHON=%s\n" "$quoted_value"
}

resolve_libsixel_shared_lib() {
    lib_dir=$1
    if [ -z "$lib_dir" ] || [ ! -d "$lib_dir" ]; then
        printf "%s\n" ""
        return 0
    fi
    for candidate in \
        "$lib_dir/libsixel.so.1" \
        "$lib_dir/libsixel.1.so" \
        "$lib_dir/libsixel.so" \
        "$lib_dir/libsixel.1.dylib" \
        "$lib_dir/libsixel.dylib" \
        "$lib_dir/libsixel.dll" \
        "$lib_dir/libsixel-1.dll"; do
        if [ -f "$candidate" ]; then
            printf "%s\n" "$candidate"
            return 0
        fi
    done
    printf "%s\n" ""
}

compute_source_signature() {
    src_dir=$1
    lib_dir=$2
    sig_file="${TMPDIR:-/tmp}/libsixel-python-source-signature-$$.tmp"
    src_lib=

    rm -f "$sig_file"
    : > "$sig_file"

    if [ -n "$src_dir" ] && [ -d "$src_dir" ]; then
        for path in \
            "$src_dir/setup.py" \
            "$src_dir/wheel_builder.py" \
            "$src_dir/libsixel/_wheel_ext.c"; do
            if [ -f "$path" ]; then
                cksum "$path" >> "$sig_file"
            fi
        done
        if [ -d "$src_dir/libsixel" ]; then
            find "$src_dir/libsixel" -maxdepth 1 -type f -name '*.py' \
                | LC_ALL=C sort \
                | while IFS= read -r path; do
                    cksum "$path"
                done >> "$sig_file"
        fi
        if [ -d "$src_dir/libsixel/_libs" ]; then
            find "$src_dir/libsixel/_libs" -maxdepth 1 -type f -name '*.py' \
                | LC_ALL=C sort \
                | while IFS= read -r path; do
                    cksum "$path"
                done >> "$sig_file"
        fi
    fi

    src_lib=$(resolve_libsixel_shared_lib "$lib_dir")
    if [ -n "$src_lib" ]; then
        cksum "$src_lib" >> "$sig_file"
    fi

    if [ ! -s "$sig_file" ]; then
        rm -f "$sig_file"
        printf "%s\n" ""
        return 0
    fi

    cksum "$sig_file" | awk '{print $1}'
    rm -f "$sig_file"
}

emit_assignment ""

if [ "$enable_python" != "1" ]; then
    exit 0
fi

if [ -z "$python_bin" ]; then
    exit 0
fi

wheel_path=$(find "$wheel_dir" -maxdepth 1 -type f -name 'libsixel_wheel-*.whl' \
    2>/dev/null | head -n 1)
if [ -z "$wheel_path" ]; then
    exit 0
fi

# Record which wheel was installed into the shared test virtualenv.
# Recreate the venv when the wheel payload changes so make check does not
# keep executing stale package code after python/ sources were modified.
wheel_marker="$shared_python_venv/.libsixel-wheel-path"
wheel_signature=$(cksum "$wheel_path" | sed 's/ .*//')
source_signature=$(compute_source_signature "$source_dir" "$libsixel_lib_dir")
combined_signature="$wheel_signature"
if [ -n "$source_signature" ]; then
    combined_signature="${wheel_signature}:${source_signature}"
fi

if ! "$python_bin" -c 'import importlib.util,sys; missing=[m for m in ("venv","ensurepip") if importlib.util.find_spec(m) is None]; sys.exit(0 if not missing else 1)' >/dev/null 2>&1; then
    exit 0
fi

if [ ! -x "$shared_python_venv/bin/python" ]; then
    rm -rf "$shared_python_venv"
    "$python_bin" -m venv "$shared_python_venv" >/dev/null 2>&1 || true
fi

if [ ! -x "$shared_python_venv/bin/python" ]; then
    exit 0
fi

if [ -f "$wheel_marker" ]; then
    IFS= read -r installed_wheel_signature < "$wheel_marker" || \
        installed_wheel_signature=
    if [ "$installed_wheel_signature" != "$combined_signature" ]; then
        rm -rf "$shared_python_venv"
    fi
fi

if [ ! -x "$shared_python_venv/bin/python" ]; then
    "$python_bin" -m venv "$shared_python_venv" >/dev/null 2>&1 || true
fi

if ! "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' >/dev/null 2>&1; then
    "$shared_python_venv/bin/python" -m pip install --no-deps "$wheel_path" \
        >/dev/null 2>&1 || true
fi

if ! "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' >/dev/null 2>&1; then
    rm -rf "$shared_python_venv"
    "$python_bin" -m venv "$shared_python_venv" >/dev/null 2>&1 || true
    if [ -x "$shared_python_venv/bin/python" ]; then
        "$shared_python_venv/bin/python" -m pip install --no-deps "$wheel_path" \
            >/dev/null 2>&1 || true
    fi
fi

if "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' >/dev/null 2>&1; then
    printf "%s\n" "$combined_signature" >"$wheel_marker"
    emit_assignment "$shared_python_venv/bin/python"
fi
