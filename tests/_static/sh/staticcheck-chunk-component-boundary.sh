#!/bin/sh
# Emit TAP for chunk component boundary checks.

set -eu

src_root=${1:-}

echo "1..4"

if test -z "$src_root"; then
    echo "not ok 1 - chunk header is opaque and factory-only"
    echo "# src_root argument is required"
    echo "not ok 2 - retired chunk constructor and destructor are absent"
    echo "# src_root argument is required"
    echo "not ok 3 - chunk storage fields are private to src/chunk.c"
    echo "# src_root argument is required"
    echo "not ok 4 - image/chunk classid is registered"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-chunk-boundary-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

failed=0
header=$src_root/src/chunk.h
classid_gperf=$src_root/src/classid-factory.gperf
header_violations=$tmpdir/chunk-header-violations.txt
retired_uses=$tmpdir/chunk-retired-uses.txt
field_uses=$tmpdir/chunk-field-uses.txt

if test ! -f "$header"; then
    echo "not ok 1 - chunk header is opaque and factory-only"
    echo "# missing header: $header"
    failed=1
else
    awk '
    /^struct sixel_chunk[[:space:]]*\{/ ||
    /sixel_chunk_new[[:space:]]*\(/ ||
    /sixel_chunk_destroy[[:space:]]*\(/ ||
    /sixel_chunk_get_[A-Za-z0-9_]+[[:space:]]*\(/ {
        print FILENAME ":" FNR ":" $0
    }
    ' "$header" > "$header_violations"
    if test -s "$header_violations"; then
        echo "not ok 1 - chunk header is opaque and factory-only"
        sed 's/^/# chunk header leak: /' "$header_violations"
        failed=1
    else
        echo "ok 1 - chunk header is opaque and factory-only"
    fi
fi

find "$src_root/src" "$src_root/include" "$src_root/tests" \
    "$src_root/converters" "$src_root/assessment" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.idl' \
        -o -name '*.awk' -o -name '*.gperf' \) \
    ! -path "$src_root/tests/_static/sh/staticcheck-chunk-component-boundary.sh" \
    -exec awk '
    /sixel_chunk_(new|destroy)[[:space:]]*\(/ {
        print FILENAME ":" FNR ":" $0
    }
    ' {} + > "$retired_uses"

if test -s "$retired_uses"; then
    echo "not ok 2 - retired chunk constructor and destructor are absent"
    sed 's/^/# retired chunk API: /' "$retired_uses"
    failed=1
else
    echo "ok 2 - retired chunk constructor and destructor are absent"
fi

find "$src_root/src" "$src_root/include" "$src_root/tests" \
    "$src_root/converters" "$src_root/assessment" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc.c' \) \
    ! -path "$src_root/src/chunk.c" \
    -exec awk '
    /(^|[^A-Za-z0-9_])([A-Za-z0-9_]*chunk|pchunk)->(buffer|size|max_size|source_path|allocator)([^A-Za-z0-9_]|$)/ {
        print FILENAME ":" FNR ":" $0
    }
    /(^|[^A-Za-z0-9_])sixel_chunk_t[[:space:]]+[A-Za-z0-9_]+[[:space:]]*;/ &&
    $0 !~ /chunk_interface/ {
        print FILENAME ":" FNR ":" $0
    }
    ' {} + > "$field_uses"

if test -s "$field_uses"; then
    echo "not ok 3 - chunk storage fields are private to src/chunk.c"
    sed 's/^/# direct chunk field: /' "$field_uses"
    failed=1
else
    echo "ok 3 - chunk storage fields are private to src/chunk.c"
fi

if test ! -f "$classid_gperf"; then
    echo "not ok 4 - image/chunk classid is registered"
    echo "# missing registry: $classid_gperf"
    failed=1
elif awk -F '[,[:space:]]+' '
$2 == "define" && $4 == "sixel_chunk_factory_new" {
    chunk_macro = $3
}
$1 == "image/chunk" && $2 == chunk_macro {
    found = 1
}
END {
    exit found ? 0 : 1
}
' "$classid_gperf"; then
    echo "ok 4 - image/chunk classid is registered"
else
    echo "not ok 4 - image/chunk classid is registered"
    echo "# image/chunk -> sixel_chunk_factory_new is missing"
    failed=1
fi

exit "$failed"
