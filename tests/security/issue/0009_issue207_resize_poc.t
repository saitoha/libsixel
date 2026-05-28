#!/bin/sh
# TAP test for issue #207 resize handling on crafted input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

issue207="${TOP_SRCDIR}/tests/data/security/issue/data/207/poc"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -h 50% -r lanczos3 -w 300px "${issue207}" \
    >"${ARTIFACT_LOCAL_DIR}/issue207-resize.sixel" || command_status=$?

# Accept success or mapped error exits without crashing.
test "${command_status-0}" -le "${SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3}" || {
    echo "not ok" 1 - "resize path failed"
    exit 0
}

echo "ok" 1 - "resize path handled"
exit 0
