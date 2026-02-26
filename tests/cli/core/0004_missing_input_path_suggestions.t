#!/bin/sh
# TAP test ensuring img2sixel reports suggestions for missing input paths.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

missing_input="${TOP_SRCDIR}/tests/data/inputs/xxxxx.xxx"
stderr_capture="${ARTIFACT_LOCAL_DIR}/err.txt"

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              --env SIXEL_TRACE_TOPIC=suggestion:lifecycle \
              "${missing_input}" -o/dev/null 2>"${stderr_capture}" && {
    fail 1 "accepts missing input path"
    exit 0
}

awk '
/Suggestions:/ { m++ }
/Suggestion lookup unavailable on this build\./ { m++ }
/No nearby matches were found in/ { m++ }
END { if (!m) exit 1 }
' "${stderr_capture}" >/dev/null 2>&1 || {
    fail 1 "missing path suggestion diagnostics"
    exit 0
}

pass 1 "missing input path suggestion diagnostics reported"
exit 0
