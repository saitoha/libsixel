#!/bin/sh
# TAP test comparing img2sixel manpage with bash completion definitions.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v

sum1=$(sed -n \
    -e '/^\.B \\-[A-Za-z0-9],/{
            s/[\\,]//g
            s/^\.B[[:space:]]*//
            s/[[:space:]][[:space:]]*/ /g
            s/^\([^ ]* [^ ]*\).*/\1/p
        }' \
    -e '/^\.B \\-[A-Za-z0-9] /{
            s/\\//g
            s/=/ /g
            s/^\.B[[:space:]]*//
            s/[[:space:]][[:space:]]*/ /g
            s/^\([^ ]*\) [^ ]* \([^ ]*\).*/\1 \2/p
        }' \
    "${TOP_SRCDIR}/converters/img2sixel.1" | cksum)

sum2=$(sed -n \
    's/^[[:space:]]*\(-[0-9A-Za-z]\)[[:space:]][[:space:]]*\(--[^[:space:]]*\)[[:space:]][[:space:]]*\\$/\1 \2/p' \
    "${TOP_SRCDIR}/converters/shell-completion/bash/img2sixel" | cksum)

test "${sum1}" = "${sum2}" || {
    echo "not ok" 1 - "manpage diverges from bash completion"
    exit 0
}

echo "ok" 1 - "manpage matches bash completion"
exit 0
