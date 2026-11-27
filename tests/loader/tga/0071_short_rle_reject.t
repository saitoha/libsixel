#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify TGA short rle reject using original codes and analytic values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract \
    SIXEL_LOADER_CMS_TARGET_COLORSPACE=srgb-linear \
    SIXEL_LOADER_PREFER_8BIT=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" tgac/0030_short_rle_reject 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    printf '\n%s\n' "not ok 1 - TGA short_rle_reject values"
    exit 0
}
printf '\n%s\n' "ok 1 - TGA short_rle_reject"
exit 0
