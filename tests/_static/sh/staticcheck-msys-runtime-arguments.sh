#!/bin/sh
# Runtime paths must not inherit compiler-only MSYS conversion exclusions.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text) {
    return text ~ /test_env\.set\(\047MSYS2_ARG_CONV_EXCL\047, \047\047\)/ &&
        text ~ /env: test_env,/
}
/^foreach t : tests$/ { active = 1 }
active && !/^[[:space:]]*#/ { body = body $0 "\n" }
active && /^endforeach$/ { active = 0 }
END {
    removed = body
    sub(/test_env\.set/, "unrelated.set", removed)
    suppressed = body
    sub(/MSYS2_ARG_CONV_EXCL\047, \047\047/, "MSYS2_ARG_CONV_EXCL\047, \047*\047", suppressed)
    exit !valid(body) || valid(removed) || valid(suppressed) || valid("")
}
' "$1/tests/meson.build" || {
    echo 'not ok 1 - runtime children permit MSYS shell path conversion'
    exit 0
}
echo 'ok 1 - runtime children permit MSYS shell path conversion'
exit 0
