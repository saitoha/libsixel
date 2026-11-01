#!/bin/sh
# Policy: docs/loader/color-management/README.md
# A prepared ColorSync source rejects a builtin destination without changing input or requested engine.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0129_colorsync_rgb_backend_mismatch || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - colorsync rgb backend mismatch"
    exit 0
}
echo "ok 1 - colorsync rgb backend mismatch"
exit 0
