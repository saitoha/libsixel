#!/bin/sh
# TAP test: k-means init output check.

set -eux


echo "1..1"
set -v

output=$(
   ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --env SIXEL_PALETTE_KMEANS_INITTYPE= "palette/0001_kmeans_init"
) || output=""

cr=$(printf '\r')
case "${output}" in
    *"${cr}") output=${output%"${cr}"} ;;
esac

test "${output}" = "none" || {
    printf '\n%s\n' "not ok 1 - unexpected kmeans init output: ${output}"
    exit 0
}

printf '\n%s\n' "ok 1 - kmeans init output matched: none"

exit 0
