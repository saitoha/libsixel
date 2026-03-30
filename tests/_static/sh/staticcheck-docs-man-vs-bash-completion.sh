#!/bin/sh
# Emit TAP for manpage vs bash completion option parity.

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

while IFS= read -r line; do
    case "$line" in
    ".B \\-[A-Za-z0-9],"*)
        clean=$(printf '%s\n' "$line" \
            | sed 's/\\fP//g; s/\\fI//g; s#\\##g; \
                   s/^\\.B[[:space:]]*//; s/[[:space:]]\+/ /g')
        read -r token1 token2 _extra <<EOF
$clean
EOF
        test -z "$token1" && continue
        test -z "$token2" && continue
        printf '%s %s\n' "$token1" "$token2"
        ;;
    ".B \\-[A-Za-z0-9] "*)
        clean=$(printf '%s\n' "$line" \
            | sed 's/\\fP//g; s/\\fI//g; s#\\##g; \
                   s/^\\.B[[:space:]]*//; s/=/ /g; s/[[:space:]]\+/ /g')
        read -r token1 token2 token3 _extra <<EOF
$clean
EOF
        test -z "$token1" && continue
        test -z "$token3" && continue
        printf '%s %s\n' "$token1" "$token3"
        ;;
    esac
done < "$man_file" > "$man_norm"

while IFS= read -r line; do
    case "$line" in
    [[:space:]]-[0-9A-Za-z][[:space:]]*--*)
        clean=$(printf '%s\n' "$line" \
            | sed 's/[[:space:]]\+/ /g; s/^ //; s/[[:space:]]$//')
        read -r token1 token2 _extra <<EOF
$clean
EOF
        test -z "$token1" && continue
        test -z "$token2" && continue
        case "$token1" in
        -[0-9A-Za-z]|-[0-9A-Za-z],)
            ;;
        *)
            continue
            ;;
        esac
        case "$token2" in
        --*)
            printf '%s %s\n' "$token1" "$token2"
            ;;
        esac
    esac
done < "$completion_file" > "$completion_norm"

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
