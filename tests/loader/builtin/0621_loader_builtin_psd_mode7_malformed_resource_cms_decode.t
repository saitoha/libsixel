#!/bin/sh
# Verify malformed mode7 PSD image-resource section is logged and decode succeeds.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_bad_resource_signature.psd"

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=auto! \
    "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"libsixel: builtin PSD: malformed ICC resource section; skipping ICC conversion"*)
        ;;
    *)
        echo "not ok" 1 - "missing malformed-ICC trace for mode7 PSD resource section"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: loader builtin succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader failed on mode7 PSD with malformed resource signature"
        exit 0
        ;;
esac

echo "ok" 1 - "builtin loader logs malformed mode7 PSD ICC resource and still decodes"
exit 0
