#!/bin/sh
# TAP test comparing img2sixel manpage with bash completion definitions.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

printf '1..1\n'
set -v

sum1=$(awk '
/^\.B \\-\\?[A-Za-z0-9],/ { gsub(/[\\,]/, ""); print $2, $3; }
/^\.B \\-\\?[A-Za-z0-9] / { gsub(/=\\fI[A-Z][A-Z]*\\fP|\\/, ""); print $2, $4; }
' "${TOP_SRCDIR}/converters/img2sixel.1" | cksum)

sum2=$(awk '
/^ +-[0-9A-Za-z] --.*\\$/ { print $1, $2; }
' "${TOP_SRCDIR}/converters/shell-completion/bash/img2sixel" | cksum)

test "${sum1}" = "${sum2}" || {
    fail 1 "manpage diverges from bash completion"
    exit 0
}

pass 1 "manpage matches bash completion"
exit 0
