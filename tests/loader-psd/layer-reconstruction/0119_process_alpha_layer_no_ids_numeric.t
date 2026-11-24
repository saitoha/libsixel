#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha layer no ids numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0137_process_alpha_layer_no_ids_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - process alpha layer no ids numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - process alpha layer no ids numeric"
exit 0
