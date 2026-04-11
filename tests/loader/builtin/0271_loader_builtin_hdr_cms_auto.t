#!/bin/sh
# TAP test confirming builtin HDR decode succeeds with cms_engine=auto.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..2"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal.hdr"
output_auto="${ARTIFACT_LOCAL_DIR}/builtin-hdr-cms-auto.six"
output_none="${ARTIFACT_LOCAL_DIR}/builtin-hdr-cms-none-reference.six"
black_reference="${ARTIFACT_LOCAL_DIR}/builtin-hdr-black-reference.ppm"
lsqa_status=0
trace_status=0
trace_log=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! "${input_hdr}" >"${output_auto}" || {
    echo "not ok" 1 - "builtin loader failed to decode HDR with cms=auto"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! "${input_hdr}" >"${output_none}" || {
    echo "not ok" 1 - "builtin loader failed to decode HDR reference with cms=none"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_none}" "${output_auto}" 2>&1) || {
    echo "not ok" 1 - "builtin HDR cms=auto diverged from cms=none reference: ${lsqa_msg}"
    exit 0
}

printf 'P6\n1 1\n255\n\000\000\000' >"${black_reference}"
lsqa_status=0
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.99" \
    "${black_reference}" "${output_auto}" 2>&1) || lsqa_status=$?
lsqa_status=${lsqa_status-0}

test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "builtin HDR cms=auto output collapsed to black-like image: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin loader decodes HDR with cms=auto, matches cms=none, and avoids black output collapse"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms_engine=auto! \
    "${input_hdr}" -o /dev/null 2>&1) || trace_status=$?

test "${trace_status}" -eq 0 || {
    echo "not ok" 2 - "builtin HDR cms=auto trace run failed"
    exit 0
}
test "${trace_log#*header-derived source profile is unavailable on this CMS backend*}" = "${trace_log}" || {
    echo "not ok" 2 - "builtin HDR cms=auto emitted unexpected header-profile unavailable trace"
    exit 0
}

echo "ok" 2 - "builtin HDR cms=auto does not emit header-profile unavailable trace"
exit 0
