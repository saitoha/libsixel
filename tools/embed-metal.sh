#!/bin/sh
# SPDX-License-Identifier: MIT
# Embed one Metal source in a build-tree header. Atomic publication prevents
# a failed generator from leaving a plausible, truncated include fragment.
set -eu
test "$#" -eq 2 || {
    echo "usage: $0 INPUT.metal|- OUTPUT.inc.h|-" >&2
    exit 2
}
source=$1
output=$2
# Stream mode lets callers inspect literal bytes without temporary files.
if test "$output" = -; then
    LC_ALL=C exec "${AWK:-awk}" -f "${0%/*}/embed-metal.awk" "$source"
fi
directory=${output%/*}
test "$directory" != "$output" || directory=.
mkdir -p "$directory"
temporary=$output.tmp.$$
trap 'rm -f "$temporary"' EXIT HUP INT TERM
LC_ALL=C "${AWK:-awk}" -f "${0%/*}/embed-metal.awk" "$source" > "$temporary"
mv -f "$temporary" "$output"
