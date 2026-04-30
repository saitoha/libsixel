#!/bin/sh
# Emit TAP for guarding retired post-v1.8.7 frame API exports.

set -eu

src_root=${1:-}

echo "1..2"

if test -z "$src_root"; then
    echo "not ok 1 - sixel.h.in omits retired post-v1.8.7 frame API"
    echo "# src_root argument is required"
    echo "not ok 2 - source omits retired post-v1.8.7 frame ABI"
    echo "# src_root argument is required"
    exit 1
fi

header=$src_root/include/sixel.h.in
source_dir=$src_root/src
failed=0

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-frame-export-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

retired_symbols=$tmpdir/retired-frame-symbols.txt
header_symbols=$tmpdir/header-frame-symbols.txt
source_symbols=$tmpdir/source-frame-symbols.txt
forbidden_header=$tmpdir/forbidden-header-frame-symbols.txt
forbidden_source=$tmpdir/forbidden-source-frame-symbols.txt

cat > "$retired_symbols" <<'EOF'
sixel_frame_get_pixels_float32
sixel_frame_init_float32
sixel_frame_set_pixels
sixel_frame_set_pixels_float32
EOF

if test ! -f "$header"; then
    echo "not ok 1 - sixel.h.in omits retired post-v1.8.7 frame API"
    echo "# missing header: $header"
    failed=1
else
    awk '
    /^sixel_frame_[A-Za-z0-9_]+[[:space:]]*\(/ {
        name = $1
        sub(/\(.*/, "", name)
        print name
    }
    ' "$header" | LC_ALL=C sort -u > "$header_symbols"

    comm -12 "$header_symbols" "$retired_symbols" > "$forbidden_header"
    if test -s "$forbidden_header"; then
        echo "not ok 1 - sixel.h.in omits retired post-v1.8.7 frame API"
        sed 's/^/# retired frame declaration: /' "$forbidden_header"
        failed=1
    else
        echo "ok 1 - sixel.h.in omits retired post-v1.8.7 frame API"
    fi
fi

if test ! -d "$source_dir"; then
    echo "not ok 2 - source omits retired post-v1.8.7 frame ABI"
    echo "# missing source directory: $source_dir"
    failed=1
else
    find "$source_dir" -type f -name '*.c' -exec awk '
    FNR == 1 {
        pending = 0
        candidate = ""
        scan = 0
    }
    /^SIXELAPI([[:space:]]|$)/ {
        pending = 8
        next
    }
    pending > 0 && /^sixel_frame_[A-Za-z0-9_]+[[:space:]]*\(/ {
        name = $1
        sub(/\(.*/, "", name)
        candidate = name
        scan = 128
        pending = 0
        if ($0 ~ /\{/) {
            print candidate
            candidate = ""
            scan = 0
        } else if ($0 ~ /\);[[:space:]]*$/) {
            candidate = ""
            scan = 0
        }
        next
    }
    pending > 0 && /^sixel_/ {
        pending = 0
        next
    }
    candidate != "" {
        if ($0 ~ /\{/) {
            print candidate
            candidate = ""
            scan = 0
            next
        }
        if ($0 ~ /\);[[:space:]]*$/) {
            candidate = ""
            scan = 0
            next
        }
        scan--
        if (scan <= 0) {
            candidate = ""
        }
        next
    }
    pending > 0 {
        pending--
    }
    ' {} + | LC_ALL=C sort -u > "$source_symbols"

    comm -12 "$source_symbols" "$retired_symbols" > "$forbidden_source"
    if test -s "$forbidden_source"; then
        echo "not ok 2 - source omits retired post-v1.8.7 frame ABI"
        sed 's/^/# retired frame export: /' "$forbidden_source"
        failed=1
    else
        echo "ok 2 - source omits retired post-v1.8.7 frame ABI"
    fi
fi

exit "$failed"
