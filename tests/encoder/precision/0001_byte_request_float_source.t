#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify byte request float source on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0001_byte_request_float_source 1>&2 || {
    printf '\n%s\n' "not ok 1 - byte request float source"
    exit 0
}
printf '\n%s\n' "ok 1 - byte request float source"
exit 0
