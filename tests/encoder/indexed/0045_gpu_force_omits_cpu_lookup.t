#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
# gpu force omits cpu lookup.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/indexed/gpu_lookup_preparation 0 || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - gpu force omits cpu lookup"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu force omits cpu lookup"
exit 0
