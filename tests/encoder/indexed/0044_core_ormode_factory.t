#!/bin/sh
# The ormode factory publishes the indexed body interface.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0044_core_ormode_factory" || {
    echo 'not ok 1 - core_ormode_factory'
    exit 0
}
echo 'ok 1 - core_ormode_factory'
exit 0
