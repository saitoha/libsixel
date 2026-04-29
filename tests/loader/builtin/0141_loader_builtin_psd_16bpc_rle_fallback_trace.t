#!/bin/sh
# Verify builtin PSD decoder keeps native 16bpc path for RLE input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_16bpc_rle.psd"

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
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
    *"libsixel: builtin PSD: 16-bpc source decoded as 8-bpc fallback path"*)
        echo "not ok" 1 - "unexpected PSD 16bpc fallback trace"
        exit 0
        ;;
    *)
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

echo "ok" 1 - "builtin PSD RLE 16bpc path avoids 8bpc fallback trace"
exit 0
