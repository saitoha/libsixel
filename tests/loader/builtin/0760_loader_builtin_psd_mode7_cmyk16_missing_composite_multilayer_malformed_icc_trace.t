#!/bin/sh
# Verify mode7(4ch->CMYK16) multi-layer fallback logs malformed ICC resource
# and still decodes.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_cmyk16_missing_composite_multilayer_normal_malformed_resource.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "mode7 CMYK16 multi-layer malformed resource decode failed: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"malformed ICC resource section; skipping ICC conversion"*)
        ;;
    *)
        echo "not ok" 1 - "mode7 CMYK16 multi-layer malformed ICC trace is missing"
        exit 0
        ;;
esac

echo "ok" 1 - "mode7 CMYK16 multi-layer malformed ICC is traced and decode succeeds"
exit 0
