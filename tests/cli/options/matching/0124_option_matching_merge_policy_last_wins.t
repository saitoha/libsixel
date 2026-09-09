#!/bin/sh
# TAP test verifying repeated -F options follow argument-order last-wins.
# Policy: docs/functionality/merge-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
msg=''
diag_line=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -p 16 \
    -Qkmeans:seed=1:restarts=1:feedback=0 \
    -Fauto -Fward \
    "${input_ppm}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "late ward merge encode failed"
    exit 0
}

diag_line="LSXPAL1${msg#*LSXPAL1}"
diag_line=${diag_line%%"${nl}"*}
case "${diag_line}" in
    LSXPAL1*rc=0*codes=*) ;;
    *)
        echo "not ok" 1 - "late ward merge diagnostic header is malformed"
        exit 0
        ;;
esac

test "${diag_line#*MODEL_KMEANS}" != "${diag_line}" || {
    echo "not ok" 1 - "late ward merge did not use kmeans contract"
    exit 0
}
test "${diag_line#*MERGE_WARD}" != "${diag_line}" || {
    echo "not ok" 1 - "late ward merge did not override early auto merge"
    exit 0
}

status=0
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -p 16 \
    -Qkmeans:seed=1:restarts=1:feedback=0 \
    -Fward -Fauto \
    "${input_ppm}" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "late auto merge encode failed"
    exit 0
}

diag_line="LSXPAL1${msg#*LSXPAL1}"
diag_line=${diag_line%%"${nl}"*}
case "${diag_line}" in
    LSXPAL1*rc=0*codes=*) ;;
    *)
        echo "not ok" 1 - "late auto merge diagnostic header is malformed"
        exit 0
        ;;
esac

test "${diag_line#*MODEL_KMEANS}" != "${diag_line}" || {
    echo "not ok" 1 - "late auto merge did not use kmeans contract"
    exit 0
}
test "${diag_line#*MERGE_AUTO}" != "${diag_line}" || {
    echo "not ok" 1 - "late auto merge did not override early ward merge"
    exit 0
}

echo "ok" 1 - "-F merge policy uses argument-order last-wins"
exit 0
