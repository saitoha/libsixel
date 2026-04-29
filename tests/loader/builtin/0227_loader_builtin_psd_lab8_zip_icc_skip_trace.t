#!/bin/sh
# Verify Lab ZIP path does not emit false ICC failure traces.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_lab8_zip_icc.psd"

trace_log=''
command_status=0

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "Lab ZIP decode failed unexpectedly: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"builtin PSD: embedded ICC conversion failed"*|\
    *"builtin PSD: malformed embedded ICC resource"*|\
    *"builtin PSD: failed to apply embedded ICC profile"*|\
    *"builtin PSD: skipping embedded ICC conversion for Lab custom decode path"*|\
    *"builtin PSD: skipping embedded ICC conversion for CIELAB path (cms backend unsupported)"*)
        echo "not ok" 1 - "Lab ZIP produced unexpected ICC trace"
        exit 0
        ;;
    *)
        echo "ok" 1 - "Lab ZIP decode keeps ICC trace contract without false failure"
        ;;
esac

exit 0
