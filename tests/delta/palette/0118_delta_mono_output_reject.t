#!/bin/sh
# Final output validation rejects monochrome delta before writing and permits full-output reuse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0205_delta_mono_output_reject || {
    printf '\n%s\n' "not ok 1 - mono output reject"
    exit 0
}
printf '\n%s\n' "ok 1 - mono output reject"
exit 0
