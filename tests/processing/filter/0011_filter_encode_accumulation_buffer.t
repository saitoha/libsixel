#!/bin/sh
# Run the accumulation-buffer encode regression via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0011_filter_encode_accumulation_buffer" || {
    echo "not ok 1 - 0011_filter_encode_accumulation_buffer"
    exit 0
}

echo "ok 1 - 0011_filter_encode_accumulation_buffer"
exit 0
