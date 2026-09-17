#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png-source-precision.md
# Keep opaque source bytes after a valid PNG profile factory failure

set -eux
set +e
output=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    SIXEL_LOADER_PREFER_8BIT=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0193_png_metadata_failures" 1 2>&1)
status=$?
set -e
printf '%s\n' "$output" >&2
echo "1..1"
set -v
test "$status" -eq 0 || {
    echo "not ok 1 - Keep opaque source bytes after a valid PNG profile factory failure"
    exit 0
}
echo "ok 1 - Keep opaque source bytes after a valid PNG profile factory failure"
exit 0
