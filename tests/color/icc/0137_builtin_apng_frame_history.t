#!/bin/sh
# TODO: Carry the decoded ICC history through canvas publication and replay.
# Preserve actual converted pixels and never infer history from file tags.
set -eux
echo '1..1'
set -v

status=0
SIXEL_LOADER_PREFER_8BIT=0 SIXEL_LOADER_CMS_BPC=0 \
SIXEL_LOADER_CMS_RENDERING_INTENT=relative! \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/red_alpha builtin:Ebuiltin! \
    "${TOP_SRCDIR}/tests/data/color-numeric/red-alpha-p3.apng" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required backend unavailable'
    exit 0
}
test "$status" -ne 42 || {
    echo 'not ok 1 - builtin drops ICC history from both APNG frames # TODO known result'
    exit 0
}
test "$status" -ne 0 || {
    echo 'not ok 1 - expected behavior now works; promote this test'
    exit 0
}
echo 'not ok 1 - unexpected fixture, execution or pixel result'
exit 0
