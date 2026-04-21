#!/bin/sh
# TAP test ensuring float32 CLI stbn alias matches stbn-hash output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

msg=''
diag_line=''
status=0
nl='
'
legacy_reference_cli='stbn:source=hash -p 16'
: "${legacy_reference_cli}"
msg=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=dither_contract \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d stbn -p 16 \
        "${input_gif}" 2>&1 >/dev/null
) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diag_line=${msg%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "float32 stbn alias missing diagnostic header"
    exit 0
}

test "${diag_line#LSXDTH1|rc=0|}" != "${diag_line}" || {
    echo "not ok" 1 - "float32 stbn alias malformed diagnostic header"
    exit 0
}

test "${diag_line#*|source=hash|}" != "${diag_line}" || {
    echo "not ok" 1 - "float32 stbn alias did not resolve to hash source"
    exit 0
}

test "${diag_line#*STRATEGY_SOURCE_PMJ}" = "${diag_line}" || {
    echo "not ok" 1 - "float32 stbn alias unexpectedly used PMJ source"
    exit 0
}

echo "ok" 1 - "float32 CLI stbn alias matches stbn-hash output"
exit 0
