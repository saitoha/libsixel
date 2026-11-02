#!/bin/sh
# Match named external commands to configure discovery without running them.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v

# This is the optional tree-sitter-bash audit, not a core shell check.
python_bin=${2:-python3}
command -v "$python_bin" >/dev/null 2>&1 || {
    echo 'ok 1 # SKIP optional tree-sitter-bash interpreter is unavailable'
    exit 0
}
result=0
"$python_bin" "$1/tests/_static/python/command_dependencies.py" "$1" || result=$?
test "$result" -ne 77 || {
    echo 'ok 1 # SKIP tree-sitter and tree-sitter-bash are unavailable'
    exit 0
}
test "$result" -eq 0 || {
    echo 'not ok 1 - external command dependencies have configure probes'
    exit 0
}
echo 'ok 1 - external command dependencies have configure probes'
exit 0
