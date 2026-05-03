#!/bin/sh
# Emit TAP for optional tree-sitter-c based C AST static checks.

set -eu

src_root=${1:-}
python_bin=${2:-}
have_tree_sitter_c=${3:-auto}

if test -z "$src_root"; then
    echo "not ok 1 - C AST static checks"
    echo "# src_root argument is required"
    exit 1
fi

if test -z "$python_bin"; then
    if command -v python3 >/dev/null 2>&1; then
        python_bin=python3
    elif command -v python >/dev/null 2>&1; then
        python_bin=python
    else
        echo "1..0 # SKIP python interpreter not found"
        exit 0
    fi
fi

if ! command -v "$python_bin" >/dev/null 2>&1; then
    echo "1..0 # SKIP python executable not found: $python_bin"
    exit 0
fi

case "$have_tree_sitter_c" in
    0)
        echo "1..0 # SKIP tree-sitter-c not found by build probe"
        exit 0
        ;;
    1 | auto)
        ;;
    *)
        have_tree_sitter_c=auto
        ;;
esac

if ! "$python_bin" "$src_root/tests/_static/python/staticcheck_c_ast.py" --probe \
        >/dev/null 2>&1; then
    echo "1..0 # SKIP tree-sitter-c python bindings not found"
    exit 0
fi

echo "1..1"

report=$(mktemp "${TMPDIR:-/tmp}/libsixel-c-ast-staticcheck-XXXXXX")
trap 'rm -f "$report"' EXIT HUP INT TERM

if "$python_bin" "$src_root/tests/_static/python/staticcheck_c_ast.py" \
        "$src_root" >"$report" 2>&1; then
    echo "ok 1 - C AST static checks"
    exit 0
fi

echo "not ok 1 - C AST static checks"
sed 's/^/# /' "$report"
exit 1
