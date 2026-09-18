#!/bin/sh
# Validate heckbert profile=speed parsing.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
msg=''
diag_line=''
status=0
nl='
'

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=palette_contract \
    -Q "heckbert:profile=speed" \
    -p 16 \
    "${input_image}" 2>&1 >/dev/null) || status=$?
test "${status}" -eq 0 || {
    echo "not ok" 1 - "heckbert profile=speed encode failed"
    exit 0
}
diag_line="LSXPAL1${msg#*LSXPAL1}"
diag_line=${diag_line%%"${nl}"*}
case "${diag_line}" in LSXPAL1*rc=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "heckbert profile=speed diagnostic header is malformed"
    exit 0
}
case "${diag_line}" in *codes=*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "heckbert profile=speed diagnostic header is malformed"
    exit 0
}
case "${diag_line}" in *MODEL_HECKBERT*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "heckbert profile=speed contract code mismatch"
    exit 0
}
case "${diag_line}" in *MERGE_NONE*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "heckbert profile=speed contract code mismatch"
    exit 0
}
echo "ok" 1 - "heckbert profile=speed accepted"

exit 0
