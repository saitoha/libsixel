#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0005_adobe_reference || {
    echo "not ok 1 - adobe reference"
    exit 0
}
echo "ok 1 - adobe reference"
exit 0
