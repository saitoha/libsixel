#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify the native BPC intent boundary with the requested engine.
set -eux
result=0
SIXEL_LOADER_CMS_BPC=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bpc/0015_colorsync_absolute || result=$?
test "$result" -ne 77 || {
    echo '1..0 # SKIP native CMS unavailable'
    set -v
    exit 0
}
echo '1..1'
set -v
test "$result" -eq 0 || {
    echo 'not ok 1 - colorsync absolute BPC delivery'
    exit 0
}
echo 'ok 1 - colorsync absolute BPC delivery'
exit 0
