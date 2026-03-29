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
        -e '/^-[A-Za-z0-9],/s/^\([^[:space:]]*[[:space:]][^[:space:]]*\).*/\1/p' \
        -e '/^-[A-Za-z0-9] /s/^\([^[:space:]]*[[:space:]][^[:space:]]*[[:space:]][^[:space:]]*\).*/\1/p' \
    | tr -d \\r \
    | cksum)

sum2=$(sed -n \
    -e '/^\.B \\-[A-Za-z0-9],/{
            s/\\//g
            s/^\.B[[:space:]]*//
            s/[[:space:]][[:space:]]*/ /g
            s/^\([^ ]* [^ ]*\).*/\1/p
        }' \
    -e '/^\.B \\-[A-Za-z0-9] /{
            s/\\fP//g
            s/\\fI//g
            s/\\//g
            s/^\.B[[:space:]]*//
            s/[[:space:]][[:space:]]*/ /g
            s/^\([^ ]* [^ ]* [^ ]*\).*/\1/p
        }' \
    "${TOP_SRCDIR}/converters/img2sixel.1" | cksum)

test "${sum1}" = "${sum2}" || {
    echo "not ok" 1 - "--help diverges from manpage"
    exit 0
}

echo "ok" 1 - "--help matches manpage"
exit 0
