#!/bin/sh
# Verify builtin PSD decoder logs ICC conversion failure for invalid profile bytes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_bad_icc_profile.psd"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -L builtin:cms_engine=auto! \
    "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"libsixel: trying builtin loader"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader was not attempted"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: builtin PSD: embedded ICC conversion failed"*)
        ;;
    *)
        echo "not ok" 1 - "missing PSD embedded-ICC conversion failure trace"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: loader builtin succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader did not finish successfully"
        exit 0
        ;;
esac

echo "ok" 1 - "builtin PSD logs ICC conversion failure for invalid profile bytes"
exit 0
