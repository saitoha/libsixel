#!/bin/sh
# Emit TAP for generated ruby/perl constant module parity check.

set -eu

src_root=${1:-}
build_root=${2:-${TOP_BUILDDIR:-$src_root}}
ruby_bin=${3:-}
perl_bin=${4:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - binding constants stay synchronized"
    echo "# src_root argument is required"
    exit 1
fi

src_root=$(cd "$src_root" && pwd)
build_root=$(cd "$build_root" && pwd)

# The generators read the configured header, not sixel.h.in: the template
# still carries @PACKAGE_VERSION@ and @LS_LTVERSION@ placeholders.
header_root=$build_root
if test ! -f "$header_root/include/sixel.h"; then
    header_root=$src_root
fi

if test ! -f "$header_root/include/sixel.h"; then
    echo "ok 1 # SKIP generated include/sixel.h not found"
    exit 0
fi

resolve_tool() {
    candidate=$1
    fallback=$2

    if test -n "$candidate" && test -x "$candidate"; then
        printf '%s\n' "$candidate"
        return 0
    fi
    if test -n "$candidate" && command -v "$candidate" >/dev/null 2>&1; then
        printf '%s\n' "$candidate"
        return 0
    fi
    if command -v "$fallback" >/dev/null 2>&1; then
        printf '%s\n' "$fallback"
        return 0
    fi
    return 1
}

ruby_bin=$(resolve_tool "$ruby_bin" ruby || :)
perl_bin=$(resolve_tool "$perl_bin" perl || :)

if test -z "$ruby_bin" && test -z "$perl_bin"; then
    echo "ok 1 # SKIP neither ruby nor perl is available"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-binding-constants-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

report=$tmpdir/report
: > "$report"
status=0

check_one() {
    tool=$1
    generator=$2
    committed=$3
    label=$4
    generated=$tmpdir/$label.generated
    delta=$tmpdir/$label.diff

    if test ! -f "$generator"; then
        printf 'missing generator: %s\n' "$generator" >> "$report"
        return 1
    fi
    if test ! -f "$committed"; then
        printf 'missing generated module: %s\n' "$committed" >> "$report"
        return 1
    fi
    # configure runs the generators from the build tree with a relative
    # header path, and gen_perl_constants.pl copies that path into the file
    # banner, so reproduce the same working directory and argument here.
    if ! (cd "$header_root" && \
            "$tool" "$generator" include/sixel.h "$generated") \
            >/dev/null 2>&1; then
        printf 'could not regenerate %s constants from %s\n' \
            "$label" "$header_root/include/sixel.h" >> "$report"
        return 1
    fi
    if ! diff -u "$committed" "$generated" > "$delta" 2>&1; then
        printf '%s constants are stale; regenerate with %s\n' \
            "$label" "$generator" >> "$report"
        head -n 40 "$delta" >> "$report"
        return 1
    fi

    return 0
}

if test -n "$ruby_bin"; then
    check_one "$ruby_bin" \
        "$src_root/tools/gen_ruby_constants.rb" \
        "$src_root/ruby/lib/libsixel/constants.rb" \
        ruby || status=1
fi

if test -n "$perl_bin"; then
    check_one "$perl_bin" \
        "$src_root/tools/gen_perl_constants.pl" \
        "$src_root/perl/lib/Image/LibSIXEL/Constants.pm" \
        perl || status=1
fi

if test "$status" -ne 0; then
    echo "not ok 1 - binding constants stay synchronized"
    sed 's/^/# /' "$report"
    exit 1
fi

echo "ok 1 - binding constants stay synchronized"
