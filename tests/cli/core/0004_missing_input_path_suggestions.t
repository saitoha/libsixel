#!/bin/sh
# TAP test ensuring img2sixel reports suggestions for missing input paths.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

missing_input="${TOP_SRCDIR}/xxxxx.xxx"
stderr_capture="${ARTIFACT_LOCAL_DIR}/err.txt"

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              -=1 \
              "${missing_input}" >"${ARTIFACT_LOCAL_DIR}/output.txt" 2>"${stderr_capture}" && {
    fail 1 "accepts missing input path"
    exit 0
}

has_suggestions=1
grep -q "Suggestions:" "${stderr_capture}" >/dev/null 2>&1 &&
    has_suggestions=0

has_fallback=1
grep -q "Suggestion lookup unavailable on this build." "${stderr_capture}" >/dev/null 2>&1 && has_fallback=0

test "${has_suggestions}" -eq 0 || [ "${has_fallback}" -eq 0 ] || {
    fail 1 "missing path suggestion diagnostics"
    exit 0
}

test "${has_suggestions}" -ne 0 ||
    grep -q "modified" "${stderr_capture}" >/dev/null 2>&1 || {
    fail 1 "suggestion entries were missing timestamps"
    exit 0
}

test "${has_suggestions}" -eq 0 || {
    pass 1 "missing input path reports unsupported suggestion lookup"
    exit 0
}

pass 1 "missing input path includes ranked suggestion diagnostics"
exit 0
