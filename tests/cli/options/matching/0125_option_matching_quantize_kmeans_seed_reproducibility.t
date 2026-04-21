#!/bin/sh
# TAP test verifying kmeans seed is reproducible and contract-visible.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

seed_123_a=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=123:restarts=2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" | cksum)
seed_123_b=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=123:restarts=2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" | cksum)
msg=''
diag_line=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -Qkmeans:seed=124:restarts=2 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || status=$?

test "${seed_123_a}" = "${seed_123_b}" || {
    echo "not ok" 1 - "same kmeans seed was not reproducible"
    exit 0
}

test "${status}" -eq 0 || {
    echo "not ok" 1 - "kmeans seed contract run failed"
    exit 0
}

diag_line=${msg%%"${nl}"*}
test "${diag_line#LSXPAL1*rc=0*}" != "${diag_line}" || {
    echo "not ok" 1 - "kmeans seed diagnostic header is malformed"
    exit 0
}
test "${diag_line#*kseed_set=1*}" != "${diag_line}" || {
    echo "not ok" 1 - "kmeans seed diagnostic header missing seed_set"
    exit 0
}
test "${diag_line#*kseed=124*}" != "${diag_line}" || {
    echo "not ok" 1 - "kmeans seed diagnostic header missing seed value"
    exit 0
}
test "${diag_line#*KMEANS_SEED_SET*}" != "${diag_line}" || {
    echo "not ok" 1 - "kmeans seed contract code missing"
    exit 0
}

echo "ok" 1 - "kmeans seed is reproducible and contract-visible"
exit 0
