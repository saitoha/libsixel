#!/bin/sh
# Verify artboard PSD keeps diagnostic code contract in header-only mode.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_artboard.psd"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "artboard decode failed"
    exit 0
}

diag_line=${trace_output%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "artboard missing diagnostic header line"
    exit 0
}

case "${diag_line}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - "artboard diagnostic header is malformed"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_CLBL1_PARSE*) ;;
    *)
        echo "not ok" 1 - "artboard missing FX_CLBL1_PARSE code"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_INFX0_PARSE*) ;;
    *)
        echo "not ok" 1 - "artboard missing FX_INFX0_PARSE code"
        exit 0
        ;;
esac

echo "ok" 1 - "artboard keeps diagnostic code contract"
exit 0
