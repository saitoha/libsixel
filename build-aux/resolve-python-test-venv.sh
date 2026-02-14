#!/bin/sh
# Prepare a shared Python test virtualenv and print a shell-safe assignment.
#
# Usage:
#   resolve-python-test-venv.sh <enable_python> <python_bin> <wheel_dir> <venv_dir>

set -eu

if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <enable_python> <python_bin> <wheel_dir> <venv_dir>" >&2
    exit 1
fi

enable_python=$1
python_bin=$2
wheel_dir=$3
shared_python_venv=$4
wheel_path=

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
    printf "SIXEL_TEST_PYTHON_VENV=%s\n" "$quoted_value"
}

emit_assignment ""

if [ "$enable_python" != "1" ]; then
    exit 0
fi

if [ -z "$python_bin" ]; then
    python_bin=$(command -v python3 || command -v python || true)
fi

if [ -z "$python_bin" ]; then
    exit 0
fi

wheel_path=$(find "$wheel_dir" -maxdepth 1 -type f -name 'libsixel_wheel-*.whl' \
    2>/dev/null | head -n 1)
if [ -z "$wheel_path" ]; then
    exit 0
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
    emit_assignment "$shared_python_venv"
fi
