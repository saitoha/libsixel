#!/bin/sh
# TAP test for issue #166 crafted width input handling.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"


issue166="${TOP_SRCDIR}/tests/data/security/issue/data/166/poc"


set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${issue166}" -w128 >"${ARTIFACT_LOCAL_DIR}/issue166-width.sixel"
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    echo "not ok" 1 - "crafted width input failed"
    exit 0
}

echo "ok" 1 - "crafted width input handled"

exit 0
