#!/bin/sh
# TAP test comparing img2sixel --help with the manpage options list.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v

sum1=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -H \
    | sed -n \
        -e '/^[[:space:]]*-[A-Za-z0-9],/s/^[[:space:]]*\([^[:space:]]*[[:space:]][^[:space:]]*\).*/\1/p' \
        -e '/^[[:space:]]*-[A-Za-z0-9] /s/^[[:space:]]*\([^[:space:]]*[[:space:]][^[:space:]]*[[:space:]][^[:space:]]*\).*/\1/p' \
    | tr -d \\r \
    | cksum)

sum2=$(awk '
        /^\.B \\-[A-Za-z0-9],/ {
            line = $0
            gsub(/\\/, "", line)
            sub(/^\.B[[:space:]]*/, "", line)
            gsub(/[[:space:]][[:space:]]*/, " ", line)
            split(line, fields, " ")
            print fields[1], fields[2]
            next
        }
        /^\.B \\-[A-Za-z0-9] / {
            line = $0
            gsub(/\\fP/, "", line)
            gsub(/\\fI/, "", line)
            gsub(/\\/, "", line)
            sub(/^\.B[[:space:]]*/, "", line)
            gsub(/[[:space:]][[:space:]]*/, " ", line)
            split(line, fields, " ")
            print fields[1], fields[2], fields[3]
            next
        }' \
    "${TOP_SRCDIR}/converters/img2sixel.1" | cksum)

test "${sum1}" = "${sum2}" || {
    echo "not ok" 1 - "--help diverges from manpage"
    exit 0
}

echo "ok" 1 - "--help matches manpage"
exit 0
