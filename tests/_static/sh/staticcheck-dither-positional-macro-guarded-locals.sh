#!/bin/sh
# Emit TAP for positional bluenoise locals guarded by policy macros.

set -eu

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - positional bluenoise locals are macro-guarded"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-positional-guard-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bad=$tmpdir/bad.txt
: > "$bad"

check_file() {
    path=$1
    macro=$2
    decl=$3
    init_prefix=$4

    awk -v macro="$macro" -v decl="$decl" -v init_prefix="$init_prefix" '
    function in_guarded_scope(    i) {
        for (i = 1; i <= depth; ++i) {
            if (guard[i]) {
                return 1
            }
        }
        return 0
    }
    /^[[:space:]]*#if/ {
        depth += 1
        guard[depth] = ($0 ~ macro) ? 1 : 0
        next
    }
    /^[[:space:]]*#elif/ {
        if (depth > 0) {
            guard[depth] = ($0 ~ macro) ? 1 : 0
        }
        next
    }
    /^[[:space:]]*#else/ {
        if (depth > 0) {
            guard[depth] = 0
        }
        next
    }
    /^[[:space:]]*#endif/ {
        if (depth > 0) {
            guard[depth] = 0
            depth -= 1
        }
        next
    }
    $0 ~ decl {
        decl_seen = 1
        if (!in_guarded_scope()) {
            printf "%s:%d:declaration is not guarded by %s\n",
                FILENAME, NR, macro
            bad = 1
        }
    }
    index($0, init_prefix) > 0 {
        init_seen = 1
        if (!in_guarded_scope()) {
            printf "%s:%d:initialization is not guarded by %s\n",
                FILENAME, NR, macro
            bad = 1
        }
    }
    END {
        if (!decl_seen) {
            printf "%s:1:missing expected declaration\n", FILENAME
            bad = 1
        }
        if (!init_seen) {
            printf "%s:1:missing expected initialization\n", FILENAME
            bad = 1
        }
        exit bad ? 1 : 0
    }
    ' "$path"
}

if ! check_file \
        "$src_root/src/dither-policy-positional-8bit.inc.h" \
        "SIXEL_DITHER_POLICY_POSITIONAL_8BIT_ENABLE_BLUENOISE" \
        "^[[:space:]]*sixel_bluenoise_conf_8bit_t[[:space:]]+bluenoise_conf;" \
        "bluenoise_conf.strength =" >> "$bad" 2>&1; then
    :
fi

if ! check_file \
        "$src_root/src/dither-policy-positional-float32.inc.h" \
        "SIXEL_DITHER_POLICY_POSITIONAL_FLOAT32_ENABLE_BLUENOISE" \
        "^[[:space:]]*sixel_bluenoise_conf_float32_t[[:space:]]+bluenoise_conf;" \
        "bluenoise_conf.strength =" >> "$bad" 2>&1; then
    :
fi

if test -s "$bad"; then
    echo "not ok 1 - positional bluenoise locals are macro-guarded"
    sed 's/^/# /' "$bad"
    exit 1
fi

echo "ok 1 - positional bluenoise locals are macro-guarded"
exit 0
