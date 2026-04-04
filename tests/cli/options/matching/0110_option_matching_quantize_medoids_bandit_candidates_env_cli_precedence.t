#!/bin/sh
# TAP test verifying medoids bandit_candidates accepts env/CLI values and keeps CLI priority.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES=8" \
    -Qmedoids \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "env-only bandit_candidates=8 was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:bandit_candidates=4096 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "cli-only bandit_candidates upper bound was rejected"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES=8" \
    -Qmedoids:bandit_candidates=7 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "invalid CLI bandit_candidates unexpectedly ignored in favor of env"
    exit 0
}

test "${msg#*-Q bandit_candidates must be in range 8-4096.*}" != "${msg}" || {
    echo "not ok" 1 - "missing invalid CLI bandit_candidates diagnostic"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES=7" \
    -Qmedoids:bandit_candidates=4096 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "valid CLI bandit_candidates did not override invalid env"
    exit 0
}

echo "ok" 1 - "medoids bandit_candidates follows env/CLI acceptance and CLI priority"
exit 0
