#!/bin/sh
# Emit TAP for help/manpage option list parity.

set -eu

echo "1..1"

src_root=$1

if test -z "${TOP_BUILDDIR-}"; then
    echo "ok 1 # SKIP TOP_BUILDDIR is not set"
    exit 0
fi

img2sixel_path="$TOP_BUILDDIR/converters/img2sixel${SIXEL_BIN_EXT-}"
man_file="$src_root/converters/img2sixel.1"

if test ! -x "$img2sixel_path"; then
    echo "ok 1 # SKIP img2sixel binary is not built"
    exit 0
fi

if test ! -f "$man_file"; then
    echo "ok 1 # SKIP missing manpage source"
    exit 0
fi

help_raw=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-help-raw-XXXXXX")
help_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-help-norm-XXXXXX")
man_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-man-norm-XXXXXX")

cleanup() {
    rm -f "$help_raw" "$help_norm" "$man_norm"
}
trap cleanup EXIT HUP INT TERM

if ! "$img2sixel_path" -H | tr -d '\r' > "$help_raw"; then
    echo "not ok 1 - --help output is not available"
    exit 1
fi

while IFS= read -r line; do
    case "$line" in
    [[:space:]]-[A-Za-z0-9],*)
        read -r token1 token2 _extra <<EOF
$line
EOF
        test -z "$token1" && continue
        test -z "$token2" && continue
        printf '%s %s\n' "$token1" "$token2"
        ;;
    [[:space:]]-[A-Za-z0-9][[:space:]]*)
        read -r token1 token2 token3 _extra <<EOF
$line
EOF
        test -z "$token1" && continue
        test -z "$token3" && continue
        printf '%s %s %s\n' "$token1" "$token2" "$token3"
        ;;
    esac
done < "$help_raw" > "$help_norm"

while IFS= read -r line; do
    case "$line" in
    ".B \\-[A-Za-z0-9],"*)
        clean=$(printf '%s\n' "$line" \
            | sed 's/\\fP//g; s/\\fI//g; s#\\##g; \
                   s/^\\.B[[:space:]]*//; s/[[:space:]]\+/ /g')
        read -r token1 token2 token3 _extra <<EOF
$clean
EOF
        test -z "$token1" && continue
        test -z "$token2" && continue
        printf '%s %s\n' "$token1" "$token2"
        ;;
    ".B \\-[A-Za-z0-9] "*)
        clean=$(printf '%s\n' "$line" \
            | sed 's/\\fP//g; s/\\fI//g; s#\\##g; \
                   s/^\\.B[[:space:]]*//; s/[[:space:]]\+/ /g')
        read -r token1 token2 token3 _extra <<EOF
$clean
EOF
        test -z "$token1" && continue
        test -z "$token3" && continue
        printf '%s %s %s\n' "$token1" "$token2" "$token3"
        ;;
    esac
done < "$man_file" > "$man_norm"

sum_help=$(cksum < "$help_norm")
sum_man=$(cksum < "$man_norm")

if test "$sum_help" != "$sum_man"; then
    echo "not ok 1 - --help diverges from manpage"
    echo "# help parse:"
    sed 's/^/#   /' "$help_norm"
    echo "# man parse:"
    sed 's/^/#   /' "$man_norm"
    exit 1
fi

echo "ok 1 - --help matches manpage"
