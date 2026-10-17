#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/README.md
# TAP test ensuring 8bit CLI stbn alias matches stbn-hash output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

hash_output=''
alias_output=''
msg=''
diag_line=''
status=0
nl='
'

hash_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn:source=hash -p 16 \
        "${input_gif}" 2>/dev/null
) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

status=0
alias_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn -p 16 \
        "${input_gif}" 2>/dev/null
) || status=$?

test "${status}" -eq 0 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

test "${alias_output}" = "${hash_output}" || {
    echo "not ok" 1 - "8bit stbn alias output diverged from stbn:source=hash"
    exit 0
}

status=0
msg=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=dither_contract \
        --threads=1 \
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

diag_line=${msg#*LSXDTH1|}
case "${msg}" in *LSXDTH1\|*) true ;; *) false ;; esac || {
    echo "ok" 1 - "8bit CLI stbn alias matches stbn-hash output (output contract)"
    exit 0
}

diag_line="LSXDTH1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

case "${diag_line}" in LSXDTH1\|rc=0\|*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "8bit stbn alias malformed diagnostic header"
    exit 0
}

case "${diag_line}" in *\|source=hash\|*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "8bit stbn alias did not resolve to hash source"
    exit 0
}

case "${diag_line}" in *STRATEGY_SOURCE_PMJ*) false ;; *) true ;; esac || {
    echo "not ok" 1 - "8bit stbn alias unexpectedly used PMJ source"
    exit 0
}

echo "ok" 1 - "8bit CLI stbn alias matches stbn-hash output"
exit 0
