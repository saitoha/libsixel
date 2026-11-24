#!/bin/sh
# Verify FHEDT instance metric isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0013_instance_metric_isolation || {
    printf '\n%s\n' "not ok 1 - FHEDT instance metric isolation"
    exit 0
}

printf '\n%s\n' "ok 1 - FHEDT instance metric isolation"
exit 0
