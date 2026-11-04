#!/bin/sh
# Policy: docs/threading/encoder.md
# Policy: docs/loader/libpng.md
# A queued worker must not overwrite a later loader-owned loop number.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/planning/0184_shared_timeline_ownership || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP thread support disabled'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - shared timeline ownership'
    exit 0
}
echo 'ok 1 - shared timeline ownership'
exit 0
