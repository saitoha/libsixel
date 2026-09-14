#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png-source-precision.md
# Release PNG valid_rgba16 ownership after allocation failure

set -eux
set +e
output=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_LOADER_PREFER_8BIT=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0194_png_metadata_ownership" 0 0 2>&1)
status=$?
set -e
printf '%s\n' "$output" >&2
test "$status" -ne 77 || {
    echo "1..0 # SKIP required CMS build path is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$status" -eq 0 || {
    echo "not ok 1 - Release PNG valid_rgba16 ownership after allocation failure"
    exit 0
}
echo "ok 1 - Release PNG valid_rgba16 ownership after allocation failure"
exit 0
