#!/bin/sh
# Verify blend-and-clipping keeps diagnostic header and legacy trace details.
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
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

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
    *FX_ORGL_SEM*) ;;
    *)
        echo "not ok" 1 - "blend-and-clipping diagnostic code FX_ORGL_SEM is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_ORGL_INACTIVE_PARSE*) ;;
    *)
        echo "not ok" 1 - \
            "blend-and-clipping diagnostic code FX_ORGL_INACTIVE_PARSE is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_IRGL_SEM*) ;;
    *)
        echo "not ok" 1 - "blend-and-clipping diagnostic code FX_IRGL_SEM is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_IRGL_INACTIVE_PARSE*) ;;
    *)
        echo "not ok" 1 - \
            "blend-and-clipping diagnostic code FX_IRGL_INACTIVE_PARSE is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_BEVEL_LIGHT_SEM*) ;;
    *)
        echo "not ok" 1 - \
            "blend-and-clipping diagnostic code FX_BEVEL_LIGHT_SEM is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_EBBL_INACTIVE_PARSE*) ;;
    *)
        echo "not ok" 1 - \
            "blend-and-clipping diagnostic code FX_EBBL_INACTIVE_PARSE is missing"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_DEFERRED_SOLID_CLIP_SPLIT*) ;;
    *)
        echo "not ok" 1 - \
            "blend-and-clipping diagnostic code FX_DEFERRED_SOLID_CLIP_SPLIT is missing"
        exit 0
        ;;
esac

test "${trace_output#*builtin PSD: parsed OrGl glow source/choke/range semantics*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "blend-and-clipping missed OrGl source/choke/range semantics trace"
    exit 0
}

test "${trace_output#*builtin PSD: parsed IrGl glow source/choke/range semantics*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "blend-and-clipping missed IrGl source/choke/range semantics trace"
    exit 0
}

test "${trace_output#*builtin PSD: parsed bevel lighting semantics*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "blend-and-clipping missed bevel lighting semantics trace"
    exit 0
}

test "${trace_output#*builtin PSD: separating deferred solid coverage source and clip gate in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "blend-and-clipping missing deferred solid coverage split contract"
    exit 0
}

echo "ok" 1 - \
    "blend-and-clipping keeps psd diagnostic codes and detailed trace contract"
exit 0
