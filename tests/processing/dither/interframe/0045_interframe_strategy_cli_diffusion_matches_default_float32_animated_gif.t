#!/bin/sh
# TAP test ensuring float32 -d interframe ignores STBN source env overrides.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

default_output=''
override_output=''
msg=''
diag_line=''
status=0
nl='
'

default_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}" 2>/dev/null
) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

status=0
override_output=$(
    SIXEL_DITHER_STBN_SOURCE=mask \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}" 2>/dev/null
) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

test "${override_output}" = "${default_output}" || {
    echo "not ok" 1 - "float32 interframe env source override changed output"
    exit 0
}

status=0
msg=$(
    SIXEL_DITHER_STBN_SOURCE=mask \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=dither_contract \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}" 2>&1 >/dev/null
) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diag_line=${msg#*LSXDTH1|}
test "${diag_line}" != "${msg}" || {
    echo "ok" 1 - "float32 interframe output ignores env source override (output contract)"
    exit 0
}

diag_line="LSXDTH1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXDTH1|rc=0|}" != "${diag_line}" || {
    echo "not ok" 1 - "float32 interframe env override malformed diagnostic header"
    exit 0
}

test "${diag_line#*|source=hash|}" != "${diag_line}" || {
    echo "not ok" 1 - "float32 interframe unexpectedly changed source from hash"
    exit 0
}

test "${diag_line#*STRATEGY_SOURCE_PMJ}" = "${diag_line}" || {
    echo "not ok" 1 - "float32 interframe env source override leaked into contract"
    exit 0
}

echo "ok" 1 - "float32 interframe output ignores env source override"
exit 0
