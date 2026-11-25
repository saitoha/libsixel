#!/bin/sh
# RGB555 and BGR555 exchange red and blue fields.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical rgb555-swap || {
    printf '\n%s\n' 'not ok 1 - RGB555 and BGR555 exchange red and blue fields'
    exit 0
}
printf '\n%s\n' 'ok 1 - RGB555 and BGR555 exchange red and blue fields'
exit 0
