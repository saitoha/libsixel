#!/bin/sh
# TAP test for issue #207 resize handling on crafted input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

issue207="${TOP_SRCDIR}/tests/data/security/issue/data/207/poc"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -h 50% -r lanczos3 -w 300px "${issue207}" \
    >"${ARTIFACT_LOCAL_DIR}/issue207-resize.sixel" || command_status=$?

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status-0}" -le 3 || {
    echo "not ok" 1 - "resize path failed"
    exit 0
}

echo "ok" 1 - "resize path handled"
exit 0
