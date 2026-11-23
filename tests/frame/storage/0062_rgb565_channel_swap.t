#!/bin/sh
# RGB565 and BGR565 exchange red and blue fields.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical rgb565-swap || {
    echo 'not ok 1 - RGB565 and BGR565 exchange red and blue fields'
    exit 0
}
echo 'ok 1 - RGB565 and BGR565 exchange red and blue fields'
exit 0
