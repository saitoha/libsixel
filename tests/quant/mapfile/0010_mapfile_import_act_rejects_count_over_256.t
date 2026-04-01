#!/bin/sh
# TAP test: ACT import rejects trailer color counts above 256.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/invalid-count-over-256.act"

{
    i=0
    while test "${i}" -lt 768; do
        printf '\000'
        i=$((i + 1))
    done
    printf '\001\001\000\000'
} >"${act_palette}"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m "${act_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "ACT trailer count over 256 unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_act: invalid ACT color count.}" != "${msg}" || {
    echo "not ok" 1 - "missing ACT over-256 diagnostic"
    exit 0
}

echo "ok" 1 - "ACT trailer count over 256 is rejected"

exit 0
