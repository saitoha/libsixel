#!/bin/sh
# Verify application empty palette.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0059_application_empty_palette" || {
    printf '\n%s\n' "not ok 1 - application empty palette"
    exit 0
}
printf '\n%s\n' "ok 1 - application empty palette"
exit 0
