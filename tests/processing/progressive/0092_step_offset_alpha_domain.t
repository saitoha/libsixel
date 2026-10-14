#!/bin/sh
# Verify step offset alpha domain.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0092_step_offset_alpha_domain" || {
    echo "not ok 1 - step offset alpha domain"
    exit 0
}
echo "ok 1 - step offset alpha domain"
exit 0
