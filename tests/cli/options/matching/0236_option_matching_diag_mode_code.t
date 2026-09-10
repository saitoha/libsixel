#!/bin/sh
# Verify code diagnostics suppress human-oriented candidate lists.
# Policy: docs/cli/correction-suggestions.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    -d st "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?

test "${status}" -eq 2 || {
    echo "not ok" 1 - "code diagnostic exit status mismatch"
    exit 0
}

test "${message#*LSXCLI1|phase=option_parse|*code=AMBIGUOUS_PREFIX*}" \
    != "${message}" || {
    echo "not ok" 1 - "code diagnostic header is missing"
    exit 0
}

test "${message#*ambiguous prefix \"st\".*}" != "${message}" || {
    echo "not ok" 1 - "code diagnostic detail is missing"
    exit 0
}

test "${message#*\(matches:*}" = "${message}" || {
    echo "not ok" 1 - "code diagnostic includes candidate list"
    exit 0
}

echo "ok" 1 - "code diagnostic suppresses candidate list"
exit 0
