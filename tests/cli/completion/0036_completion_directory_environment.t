#!/bin/sh
# Verify the shared completion source directory override.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env IMG2SIXEL_COMPLETION_BASH= \
    --env IMG2SIXEL_COMPLETION_ZSH= \
    --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/tests/data/completion/sources" \
    -1 all) || {
    echo "not ok" 1 - "completion directory override failed"
    exit 0
}
test "${msg#*completion bash path fixture*}" != "${msg}" || {
    echo "not ok" 1 - "completion directory missed its Bash source"
    exit 0
}
test "${msg#*completion zsh path fixture*}" != "${msg}" || {
    echo "not ok" 1 - "completion directory missed its Zsh source"
    exit 0
}
echo "ok" 1 - "completion directory supplies both shell sources"
exit 0
