#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0015_encoder_isolation || {
    echo "not ok 1 - encoder isolation"
    exit 0
}
echo "ok 1 - encoder isolation"
exit 0
