#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0002_p3_extended_identity || {
    echo "not ok 1 - p3 extended identity"
    exit 0
}
echo "ok 1 - p3 extended identity"
exit 0
