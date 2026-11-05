#!/bin/sh
# Implementation copies with different lookup macros need separate C units.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text) {
    return text ~ /override_options:[[:space:]]*\[[[:space:]]*\047unity=off\047[[:space:]]*\]/
}
/^float_lookup_tests = static_library/ { active = 1 }
active && !/^[[:space:]]*#/ { text = text $0 "\n" }
active && /install: false\)/ { active = 0 }
END {
    changed = text
    sub(/unity=off/, "unity=on", changed)
    removed = text
    sub(/override_options:/, "unused_option:", removed)
    exit !valid(text) || valid(changed) || valid(removed) || valid("")
}
' "$1/tests/meson.build" || {
    echo 'not ok 1 - float lookup implementation copies compile separately'
    exit 0
}
echo 'ok 1 - float lookup implementation copies compile separately'
exit 0
