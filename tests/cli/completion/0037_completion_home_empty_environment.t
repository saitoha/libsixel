#!/bin/sh
# Verify an empty completion home falls back to HOME.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env HOME="${ARTIFACT_LOCAL_DIR}" \
    --env IMG2SIXEL_COMPLETION_HOME= \
    -3 zsh) || {
    echo "not ok" 1 - "empty completion home fallback failed"
    exit 0
}
test "${msg#*missing *"${ARTIFACT_LOCAL_DIR##*/}"/.zfunc/_img2sixel*}" != \
    "${msg}" || {
    echo "not ok" 1 - "empty completion home did not fall back to HOME"
    exit 0
}
echo "ok" 1 - "empty completion home falls back to HOME"
exit 0
