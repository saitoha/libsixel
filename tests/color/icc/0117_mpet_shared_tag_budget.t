#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0117_mpet_shared_tag_budget || {
    printf '\n%s\n' "not ok 1 - shared MPE tags consume one payload budget"
    exit 0
}

printf '\n%s\n' "ok 1 - shared MPE tags consume one payload budget"
exit 0
