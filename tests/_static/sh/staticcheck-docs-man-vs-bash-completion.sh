#!/bin/sh
# Emit TAP for manpage vs bash completion option parity.
# Policy: docs/cli/design-policy.md

set -eu

echo "1..1"

src_root=$1
man_file="$src_root/converters/img2sixel.1"
completion_file="$src_root/converters/shell-completion/bash/img2sixel"

if test ! -f "$man_file"; then
    echo "ok 1 # SKIP missing manpage source"
    exit 0
fi

if test ! -f "$completion_file"; then
    echo "ok 1 # SKIP missing bash completion script"
    exit 0
fi

man_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-docs-man-norm-XXXXXX")
completion_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-docs-completion-norm-XXXXXX")

cleanup() {
    rm -f "$man_norm" "$completion_norm"
}
trap cleanup EXIT HUP INT TERM

awk '
/^\.B \\-/ {
    line = $0
    gsub(/\\\(ti/, "~", line)
    gsub(/\\f[IP]/, "", line)
    gsub(/\\/, "", line)
    sub(/^\.B[[:space:]]*/, "", line)
    count = split(line, field, /[[:space:]]+/)
    short_name = field[1]
    sub(/,$/, "", short_name)
    for (i = 2; i <= count; ++i) {
        long_name = field[i]
        sub(/,$/, "", long_name)
        if (long_name ~ /^--/) {
            sub(/=.*/, "", long_name)
            print short_name, long_name
            break
        }
    }
}
' "$man_file" | LC_ALL=C sort -u > "$man_norm"

awk '
$1 ~ /^-.$/ && $2 ~ /^--[a-z0-9][a-z0-9-]*$/ && $3 == "\\" {
    print $1, $2
}
' "$completion_file" | LC_ALL=C sort -u > "$completion_norm"

man_count=$(wc -l < "$man_norm")
completion_count=$(wc -l < "$completion_norm")
test "$man_count" -ge 50 || {
    echo "not ok 1 - manpage option parser returned too few entries"
    echo "# man=$man_count completion=$completion_count"
    exit 1
}
test "$completion_count" -ge 50 || {
    echo "not ok 1 - bash completion option parser returned too few entries"
    echo "# man=$man_count completion=$completion_count"
    exit 1
}

man_sum=$(cksum < "$man_norm")
completion_sum=$(cksum < "$completion_norm")

if test "$man_sum" != "$completion_sum"; then
    echo "not ok 1 - manpage diverges from bash completion"
    echo "# manpage parse:"
    sed 's/^/#   /' "$man_norm"
    echo "# completion parse:"
    sed 's/^/#   /' "$completion_norm"
    exit 1
fi

echo "ok 1 - manpage matches bash completion"
