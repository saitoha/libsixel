#!/bin/sh
# Emit TAP for help/manpage option list and alpha-policy parity.

set -eu

echo "1..1"

src_root=$1

if test -z "${TOP_BUILDDIR-}"; then
    echo "ok 1 # SKIP TOP_BUILDDIR is not set"
    exit 0
fi

img2sixel_path="$TOP_BUILDDIR/converters/img2sixel${SIXEL_BIN_EXT-}"
man_file="$src_root/converters/img2sixel.1"
policy_doc_file="$src_root/docs/loader/alpha-policy.md"

if test ! -x "$img2sixel_path"; then
    echo "ok 1 # SKIP img2sixel binary is not built"
    exit 0
fi

if test ! -f "$man_file"; then
    echo "ok 1 # SKIP missing manpage source"
    exit 0
fi

if test ! -f "$policy_doc_file"; then
    echo "not ok 1 - missing alpha policy documentation"
    exit 1
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

require_fragment() {
    check_file=$1
    check_fragment=$2
    check_label=$3

    awk -v fragment="$check_fragment" '
        index($0, fragment) != 0 { found = 1 }
        END { exit found ? 0 : 1 }
    ' "$check_file" || {
        echo "not ok 1 - $check_label"
        exit 1
    }
}

require_fragment "$help_raw" \
    "choose source alpha and omitted SIXEL pixel policy" \
    "--help has stale alpha-policy terminology"
require_fragment "$help_raw" \
    "composite -> composite source alpha over the resolved" \
    "--help has stale composite policy semantics"
require_fragment "$help_raw" \
    "fall back to keep" \
    "--help omits composite fallback"
require_fragment "$help_raw" \
    "clear     -> preserve alpha-zero and emit DCS P2=0" \
    "--help has stale clear policy semantics"
require_fragment "$help_raw" \
    "keep      -> preserve alpha-zero and emit DCS P2=1" \
    "--help has stale keep policy semantics"
require_fragment "$help_raw" \
    "semi-transparent pixels are composited when a background is" \
    "--help omits partial-alpha semantics"
require_fragment "$help_raw" \
    "P2 rendering details are terminal-dependent." \
    "--help omits terminal-dependent P2 semantics"

require_fragment "$man_file" \
    "choose source alpha and omitted SIXEL pixel policy" \
    "manpage has stale alpha-policy terminology"
require_fragment "$man_file" \
    "composite -> composite source alpha over the resolved background" \
    "manpage has stale composite policy semantics"
require_fragment "$man_file" \
    "fall back to keep." \
    "manpage omits composite fallback"
require_fragment "$man_file" \
    "clear -> preserve alpha-zero and emit DCS P2=0" \
    "manpage has stale clear policy semantics"
require_fragment "$man_file" \
    "keep -> preserve alpha-zero and emit DCS P2=1" \
    "manpage has stale keep policy semantics"
require_fragment "$man_file" \
    "Semi-transparent pixels are composited when a background is available" \
    "manpage omits partial-alpha semantics"
require_fragment "$man_file" \
    "terminal-dependent." \
    "manpage omits terminal-dependent P2 semantics"

require_fragment "$policy_doc_file" \
    "Composite source alpha over the resolved background." \
    "loader documentation has stale background policy semantics"
require_fragment "$policy_doc_file" \
    "If no background can be resolved, fall back to \`keep\`." \
    "loader documentation omits the composite fallback"
require_fragment "$policy_doc_file" \
    "Omit alpha-zero pixels and emit DCS \`P2=0\`" \
    "loader documentation has stale clear policy semantics"
require_fragment "$policy_doc_file" \
    "Omit alpha-zero pixels and emit DCS \`P2=1\`" \
    "loader documentation has stale keep policy semantics"
require_fragment "$policy_doc_file" \
    "Semi-transparent pixels are composited" \
    "loader documentation omits partial-alpha semantics"
require_fragment "$policy_doc_file" \
    "terminal-dependent" \
    "loader documentation omits terminal-dependent P2 semantics"

echo "ok 1 - --help, manpage, and alpha-policy docs match"
