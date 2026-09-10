#!/bin/sh
# Emit TAP for sixel2png runtime help versus manual option parity.
# Policy: docs/cli/design-policy.md

set -eu

echo "1..1"

src_root=$1
build_root=${2:-${TOP_BUILDDIR:-$src_root}}
sixel2png_path="$build_root/converters/sixel2png${SIXEL_BIN_EXT-}"
man_file="$src_root/converters/sixel2png.1"

test -x "$sixel2png_path" || {
    echo "ok 1 # SKIP sixel2png binary is not built"
    exit 0
}

test -f "$man_file" || {
    echo "ok 1 # SKIP missing sixel2png manual source"
    exit 0
}

help_raw=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-sixel2png-help-XXXXXX")
help_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-sixel2png-help-norm-XXXXXX")
man_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-sixel2png-man-norm-XXXXXX")

cleanup() {
    rm -f "$help_raw" "$help_norm" "$man_norm"
}
trap cleanup EXIT HUP INT TERM

"$sixel2png_path" -H | tr -d '\r' > "$help_raw" || {
    echo "not ok 1 - sixel2png -H output is not available"
    exit 1
}

while IFS= read -r line; do
    case "$line" in
    -?,*|[[:space:]]-?,*)
        read -r token1 token2 _extra <<EOF
$line
EOF
        test -z "$token1" && continue
        test -z "$token2" && continue
        printf '%s %s\n' "$token1" "$token2"
        ;;
    -?[[:space:]]*|[[:space:]]-?[[:space:]]*)
        read -r token1 token2 token3 _extra <<EOF
$line
EOF
        test -z "$token1" && continue
        test -z "$token3" && continue
        printf '%s %s %s\n' "$token1" "$token2" "$token3"
        ;;
    esac
done < "$help_raw" | LC_ALL=C sort -u > "$help_norm"

while IFS= read -r line; do
    case "$line" in
    ".B \\-"*)
        clean=$(printf '%s\n' "$line" \
            | sed -e 's/\\(ti/~/g' \
                  -e 's/\\fP//g' \
                  -e 's/\\fI//g' \
                  -e 's#\\##g' \
                  -e 's/^\.B[[:space:]]*//' \
                  -e 's/[[:space:]]\+/ /g')
        case "$clean" in
        -?,*)
            read -r token1 token2 _extra <<EOF
$clean
EOF
            test -z "$token1" && continue
            test -z "$token2" && continue
            printf '%s %s\n' "$token1" "$token2"
            ;;
        -?[[:space:]]*)
            read -r token1 token2 token3 _extra <<EOF
$clean
EOF
            test -z "$token1" && continue
            test -z "$token3" && continue
            printf '%s %s %s\n' "$token1" "$token2" "$token3"
            ;;
        esac
        ;;
    esac
done < "$man_file" | LC_ALL=C sort -u > "$man_norm"

help_count=$(wc -l < "$help_norm")
man_count=$(wc -l < "$man_norm")
test "$help_count" -ge 15 || {
    echo "not ok 1 - sixel2png help parser returned too few entries"
    echo "# help=$help_count man=$man_count"
    exit 1
}
test "$man_count" -ge 15 || {
    echo "not ok 1 - sixel2png manual parser returned too few entries"
    echo "# help=$help_count man=$man_count"
    exit 1
}

help_sum=$(cksum < "$help_norm")
man_sum=$(cksum < "$man_norm")

test "$help_sum" = "$man_sum" || {
    echo "not ok 1 - sixel2png -H diverges from its manual"
    echo "# help parse:"
    sed 's/^/#   /' "$help_norm"
    echo "# manual parse:"
    sed 's/^/#   /' "$man_norm"
    exit 1
}

echo "ok 1 - sixel2png -H and its manual option declarations match"
