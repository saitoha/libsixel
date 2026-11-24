#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify that profile open reports native fallback.

set -eux

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0028_status_native_fallback || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP Little CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - profile open reports native fallback"
    exit 0
}

printf '\n%s\n' "ok 1 - profile open reports native fallback"
exit 0
