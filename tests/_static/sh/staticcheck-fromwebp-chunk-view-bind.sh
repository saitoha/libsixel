#!/bin/sh
# Emit TAP for analyzer-friendly chunk view binding in WebP animation parser.

set -eu

src_root=${1:-}

echo "1..3"

if test -z "$src_root"; then
    echo "not ok 1 - WebP animation parser binds chunk bytes through view"
    echo "# src_root argument is required"
    echo "not ok 2 - WebP animation parser avoids legacy chunk byte getters"
    echo "# src_root argument is required"
    echo "not ok 3 - WebP animation parser binds chunk allocator once"
    echo "# src_root argument is required"
    exit 1
fi

source_file=$src_root/src/fromwebp.c
body=$(mktemp "${TMPDIR:-/tmp}/libsixel-fromwebp-chunk-view-XXXXXX")
legacy_getters=$(mktemp "${TMPDIR:-/tmp}/libsixel-fromwebp-legacy-getters-XXXXXX")
allocator_getters=$(mktemp "${TMPDIR:-/tmp}/libsixel-fromwebp-allocator-XXXXXX")
trap 'rm -f "$body" "$legacy_getters" "$allocator_getters"' EXIT HUP INT TERM

failed=0

if test ! -f "$source_file"; then
    echo "not ok 1 - WebP animation parser binds chunk bytes through view"
    echo "# missing source: $source_file"
    echo "not ok 2 - WebP animation parser avoids legacy chunk byte getters"
    echo "# missing source: $source_file"
    echo "not ok 3 - WebP animation parser binds chunk allocator once"
    echo "# missing source: $source_file"
    exit 1
fi

if ! awk '
BEGIN {
    pending = 0
    in_function = 0
    depth = 0
    found = 0
}
function update_depth(line, i, ch) {
    for (i = 1; i <= length(line); i++) {
        ch = substr(line, i, 1)
        if (ch == "{") {
            depth++
        } else if (ch == "}") {
            depth--
        }
    }
}
/^sixel_webp_parse_anim_stream[[:space:]]*\(/ {
    pending = 1
}
pending != 0 || in_function != 0 {
    printf "%d:%s\n", FNR, $0
}
pending != 0 && /\{/ {
    pending = 0
    in_function = 1
    found = 1
}
in_function != 0 {
    update_depth($0)
    if (depth == 0) {
        exit 0
    }
}
END {
    if (found == 0) {
        exit 1
    }
}
' "$source_file" > "$body"; then
    echo "not ok 1 - WebP animation parser binds chunk bytes through view"
    echo "# sixel_webp_parse_anim_stream was not found in $source_file"
    echo "not ok 2 - WebP animation parser avoids legacy chunk byte getters"
    echo "# sixel_webp_parse_anim_stream was not found in $source_file"
    echo "not ok 3 - WebP animation parser binds chunk allocator once"
    echo "# sixel_webp_parse_anim_stream was not found in $source_file"
    exit 1
fi

if awk '
/sixel_chunk_bytes_view_t[[:space:]]+view[[:space:]]*;/ {
    saw_view_decl = 1
}
/view\.bytes[[:space:]]*=[[:space:]]*NULL[[:space:]]*;/ {
    saw_view_bytes_init = 1
}
/view\.size[[:space:]]*=[[:space:]]*0u[[:space:]]*;/ {
    saw_view_size_init = 1
}
/sixel_chunk_get_bytes[[:space:]]*\([[:space:]]*chunk[[:space:]]*,[[:space:]]*&view[[:space:]]*\)/ {
    saw_get_bytes = 1
}
/view\.bytes[[:space:]]*==[[:space:]]*NULL/ {
    saw_view_bytes_guard = 1
}
/view\.size[[:space:]]*<[[:space:]]*12u/ {
    saw_view_size_guard = 1
}
/data[[:space:]]*=[[:space:]]*view\.bytes[[:space:]]*;/ {
    saw_data_bind = 1
}
/size[[:space:]]*=[[:space:]]*view\.size[[:space:]]*;/ {
    saw_size_bind = 1
}
END {
    ok = saw_view_decl && saw_view_bytes_init && saw_view_size_init &&
        saw_get_bytes && saw_view_bytes_guard && saw_view_size_guard &&
        saw_data_bind && saw_size_bind
    exit ok ? 0 : 1
}
' "$body"; then
    echo "ok 1 - WebP animation parser binds chunk bytes through view"
else
    echo "not ok 1 - WebP animation parser binds chunk bytes through view"
    echo "# parser must bind sixel_chunk_get_bytes(chunk, &view)"
    echo "# and then use view.bytes/view.size for data and size"
    failed=1
fi

awk '
/sixel_chunk_get_(buffer|size)[[:space:]]*\(/ {
    print
}
' "$body" > "$legacy_getters"

if test -s "$legacy_getters"; then
    echo "not ok 2 - WebP animation parser avoids legacy chunk byte getters"
    sed "s|^|# legacy chunk byte getter: $source_file:|" "$legacy_getters"
    failed=1
else
    echo "ok 2 - WebP animation parser avoids legacy chunk byte getters"
fi

awk '
/sixel_chunk_get_allocator[[:space:]]*\(/ {
    call_count++
    if ($0 !~ /allocator[[:space:]]*=[[:space:]]*sixel_chunk_get_allocator[[:space:]]*\([[:space:]]*chunk[[:space:]]*\)/) {
        print
    }
}
/sixel_allocator_t[[:space:]]+\*allocator[[:space:]]*;/ {
    saw_allocator_decl = 1
}
/allocator[[:space:]]*=[[:space:]]*NULL[[:space:]]*;/ {
    saw_allocator_init = 1
}
/allocator[[:space:]]*=[[:space:]]*sixel_chunk_get_allocator[[:space:]]*\([[:space:]]*chunk[[:space:]]*\)/ {
    saw_allocator_bind = 1
}
/sixel_allocator_calloc[[:space:]]*\(/ {
    in_calloc = 1
    next
}
in_calloc != 0 && /allocator[[:space:]]*,/ {
    saw_calloc_allocator = 1
    in_calloc = 0
    next
}
in_calloc != 0 && /\)/ {
    in_calloc = 0
}
/sixel_webp_anim_stream_reset[[:space:]]*\([[:space:]]*stream[[:space:]]*,[[:space:]]*allocator[[:space:]]*\)/ {
    saw_reset_allocator = 1
}
END {
    ok = saw_allocator_decl && saw_allocator_init && saw_allocator_bind &&
        saw_calloc_allocator && saw_reset_allocator && call_count == 1
    if (!ok) {
        exit 1
    }
}
' "$body" > "$allocator_getters" || allocator_status=$?

allocator_status=${allocator_status:-0}
if test "$allocator_status" -ne 0; then
    echo "not ok 3 - WebP animation parser binds chunk allocator once"
    echo "# parser must bind allocator once and reuse it for alloc/free paths"
    if test -s "$allocator_getters"; then
        sed "s|^|# extra allocator getter: $source_file:|" "$allocator_getters"
    fi
    failed=1
else
    echo "ok 3 - WebP animation parser binds chunk allocator once"
fi

exit "$failed"
