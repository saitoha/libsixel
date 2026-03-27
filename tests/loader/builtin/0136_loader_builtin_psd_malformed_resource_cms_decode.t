#!/bin/sh
# Verify malformed PSD image-resource section is logged and decode still succeeds.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_bad_resource_signature.psd"

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
    *"libsixel: builtin PSD: malformed ICC resource section; skipping ICC conversion"*)
        ;;
    *)
        echo "not ok" 1 - "missing malformed-ICC trace for PSD resource section"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: loader builtin succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader failed on PSD with malformed resource signature"
        exit 0
        ;;
esac

echo "ok" 1 - "builtin loader logs malformed PSD ICC resource and still decodes"
exit 0
