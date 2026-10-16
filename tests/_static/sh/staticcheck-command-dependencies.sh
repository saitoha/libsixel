#!/bin/sh
# Match named external commands to configure discovery without running them.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v

result=0
"${2:-python3}" "$1/tests/_static/python/command_dependencies.py" "$1" || result=$?
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
