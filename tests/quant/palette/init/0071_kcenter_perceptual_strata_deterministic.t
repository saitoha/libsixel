#!/bin/sh
# TAP test: perceptual adaptive strata keeps deterministic output with same seed.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" perceptual-strata-deterministic >/dev/null || {
    echo "not ok" 1 - "kcenter perceptual strata deterministic check failed"
    exit 0
}

echo "ok" 1 - "kcenter perceptual strata deterministic check passed"
exit 0
