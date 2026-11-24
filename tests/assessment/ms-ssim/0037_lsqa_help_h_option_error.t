#!/bin/sh
# Verify -h is rejected because only -H is documented and supported.

set -eux


printf '1..1\n'
set -v

${SIXEL_RUNTIME-} "${LSQA_PATH}" -h >/dev/null && {
    printf '\n%s\n' "not ok 1 - lsqa -h should fail"
    exit 0
}

printf '\n%s\n' "ok 1 - lsqa -h is rejected"
exit 0
