#!/bin/sh
# Verify the shell-specific Bash completion path override.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env IMG2SIXEL_COMPLETION_BASH="${TOP_SRCDIR}/tests/data/completion/sources/bash/img2sixel" \
    -1 bash) || {
    echo "not ok" 1 - "Bash completion path override failed"
    exit 0
}
test "${msg#*completion bash path fixture*}" != "${msg}" || {
    echo "not ok" 1 - "Bash completion path override was ignored"
    exit 0
}
echo "ok" 1 - "Bash completion path override is used"
exit 0
