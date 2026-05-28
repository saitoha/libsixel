#!/bin/sh
# TAP test for issue #167 crafted height input handling.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"


issue167="${TOP_SRCDIR}/tests/data/security/issue/data/167/poc"


set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${issue167}" -h128 >"${ARTIFACT_LOCAL_DIR}/issue167-height.sixel"
command_status=$?
set -e

# Accept success or mapped error exits without crashing.
test "${command_status}" -le "${SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3}" || {
    echo "not ok" 1 - "crafted height input failed"
    exit 0
}

echo "ok" 1 - "crafted height input handled"

exit 0
