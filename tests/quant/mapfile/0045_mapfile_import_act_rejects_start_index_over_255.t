#!/bin/sh
# TAP test: ACT import rejects trailer start index above 255.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-start-256-count-1.act"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m "${act_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "ACT start index over 255 unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_act: ACT start index out of range.}" != "${msg}" || {
    echo "not ok" 1 - "missing ACT start-index range diagnostic"
    exit 0
}

echo "ok" 1 - "ACT start index over 255 is rejected"

exit 0
