#!/bin/sh
# Prepare a shared Python test virtualenv and print a shell-safe assignment.
#
# Usage:
#   resolve-python-test-venv.sh <enable_python> <python_bin> <wheel_dir> <venv_dir> [source_dir] [libsixel_lib_dir]

set -eu

script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
common_helpers="$script_dir/resolve-binding-test-common.sh"
if [ ! -r "$common_helpers" ]; then
    echo "error: missing helper script: $common_helpers" >&2
    exit 1
fi
. "$common_helpers"

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
wheel_candidates=
wheel_marker=
source_signature=

emit_assignment() {
    value=$1
    quoted_value=$(sixel_quote_single "$value")
    printf "SIXEL_TEST_PYTHON=%s\n" "$quoted_value"
}

venv_has_libsixel_wheel() {
    "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' \
        >/dev/null 2>&1
}

try_install_wheel_candidate() {
    candidate=$1
    src_signature=$2
    installed_wheel_signature=
    candidate_signature=
    candidate_combined_signature=

    if [ ! -f "$candidate" ]; then
        return 1
    fi

    candidate_signature=$(cksum "$candidate" | sed 's/ .*//')
    candidate_combined_signature="$candidate_signature"
    if [ -n "$src_signature" ]; then
        candidate_combined_signature="${candidate_signature}:${src_signature}"
    fi

    if [ -f "$wheel_marker" ]; then
        IFS= read -r installed_wheel_signature < "$wheel_marker" || \
            installed_wheel_signature=
        if [ "$installed_wheel_signature" = "$candidate_combined_signature" ] &&
                venv_has_libsixel_wheel; then
            emit_assignment "$shared_python_venv/bin/python"
            return 0
        fi
    fi

    "$shared_python_venv/bin/python" -m pip install --no-deps --force-reinstall \
        "$candidate" >/dev/null 2>&1 || return 1
    if ! venv_has_libsixel_wheel; then
        return 1
    fi

    printf "%s\n" "$candidate_combined_signature" >"$wheel_marker"
    emit_assignment "$shared_python_venv/bin/python"
    return 0
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

    src_lib=$(sixel_resolve_libsixel_shared_lib "$lib_dir")
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

wheel_candidates=$(find "$wheel_dir" -maxdepth 1 -type f -name 'libsixel_wheel-*.whl' \
    2>/dev/null | LC_ALL=C sort)
if [ -z "$wheel_candidates" ]; then
    exit 0
fi

# Record which wheel was installed into the shared test virtualenv.
# Recreate the venv when the wheel payload changes so make check does not
# keep executing stale package code after python/ sources were modified.
wheel_marker="$shared_python_venv/.libsixel-wheel-path"
source_signature=$(compute_source_signature "$source_dir" "$libsixel_lib_dir")

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

for wheel_candidate in $wheel_candidates; do
    if try_install_wheel_candidate "$wheel_candidate" "$source_signature"; then
        exit 0
    fi
done

rm -rf "$shared_python_venv"
"$python_bin" -m venv "$shared_python_venv" >/dev/null 2>&1 || true
if [ ! -x "$shared_python_venv/bin/python" ]; then
    exit 0
fi

for wheel_candidate in $wheel_candidates; do
    if try_install_wheel_candidate "$wheel_candidate" "$source_signature"; then
        exit 0
    fi
done
