#!/bin/sh
# Verify the compact Vfs alias matches k_undither output.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dk_ \
        <"${TOP_SRCDIR}/images/map8.six" >/dev/null || {
    echo "not ok" 1 - "legacy k_undither prefix rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dl:Vf \
        <"${TOP_SRCDIR}/images/map8.six" >/dev/null || {
    echo "not ok" 1 - "compact Vfs alias rejected"
    exit 0
}

vfs_reference=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dk_ \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
vfs_alias=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dl:Vf \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
test "${vfs_reference}" = "${vfs_alias}" || {
    echo "not ok" 1 - "compact Vfs alias changed k_undither output"
    exit 0
}

echo "ok" 1 - "compact Vfs alias matches k_undither output"
exit 0
