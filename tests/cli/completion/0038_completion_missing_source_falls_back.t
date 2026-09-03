#!/bin/sh
# Verify an unreadable completion source falls back to packaged data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env IMG2SIXEL_COMPLETION_BASH=/missing/completion/bash \
    --env IMG2SIXEL_COMPLETION_DIR=/missing/completion/directory \
    -1 bash) || {
    echo "not ok" 1 - "missing completion source fallback failed"
    exit 0
}
test "${msg#*# bash completion for img2sixel*}" != "${msg}" || {
    echo "not ok" 1 - "missing completion source did not fall back"
    exit 0
}
echo "ok" 1 - "missing completion source falls back to packaged data"
exit 0
