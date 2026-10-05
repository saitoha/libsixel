#!/bin/sh
# P3 PNG rejects outer transports and OR before body output.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0009_transport_rejection || {
    echo "not ok 1 - PNG transport rejection"
    exit 0
}
echo "ok 1 - PNG transport rejection"
exit 0
