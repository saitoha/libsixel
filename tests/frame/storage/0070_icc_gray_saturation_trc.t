#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical gray-saturation-matrix || {
    echo 'not ok 1 - Gray ICC saturation LUT preserves relative kTRC'
    exit 0
}
echo 'ok 1 - Gray ICC saturation LUT preserves relative kTRC'
exit 0
