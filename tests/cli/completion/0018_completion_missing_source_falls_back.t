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
    printf '\n%s\n' "not ok 1 - missing completion source fallback failed"
    exit 0
}
case "${msg}" in *#\ bash\ completion\ for\ img2sixel**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing completion source did not fall back"
    exit 0
}
printf '\n%s\n' "ok 1 - missing completion source falls back to packaged data"
exit 0
