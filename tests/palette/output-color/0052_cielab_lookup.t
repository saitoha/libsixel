#!/bin/sh
# Verify standalone cielab lookup.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "53" || {
    printf '\n%s\n' "not ok 1 - standalone cielab lookup"
    exit 0
}
printf '\n%s\n' "ok 1 - standalone cielab lookup"
exit 0
