#!/bin/sh
# Verify infx parse contract remains visible on blend-and-clipping hardcase.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_blend_and_clipping.psd"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "blend-and-clipping decode failed"
    exit 0
}

diag_line=${trace_output%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "blend-and-clipping missing diagnostic header line"
    exit 0
}

case "${diag_line}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - "blend-and-clipping diagnostic header is malformed"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_INFX0_PARSE*) ;;
    *)
        echo "not ok" 1 - "blend-and-clipping did not parse infx=0"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_INFX1_PARSE*) ;;
    *)
        echo "not ok" 1 - "blend-and-clipping did not parse infx=1"
        exit 0
        ;;
esac

echo "ok" 1 - "blend-and-clipping keeps infx parse trace contract"
exit 0
