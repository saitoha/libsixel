#!/bin/sh
# Emit TAP for analyzer-friendly chunk byte binding in BMP decoder.

set -eu

src_root=${1:-}

echo "1..3"

if test -z "$src_root"; then
    echo "not ok 1 - BMP parser binds chunk bytes through view"
    echo "# src_root argument is required"
    echo "not ok 2 - BMP decoder avoids legacy chunk byte getters"
    echo "# src_root argument is required"
    echo "not ok 3 - BMP decode info avoids direct chunk rebinding"
    echo "# src_root argument is required"
    exit 1
fi

source_file=$src_root/src/frombmp.c
header_file=$src_root/src/frombmp-internal.h
body=$(mktemp "${TMPDIR:-/tmp}/libsixel-frombmp-chunk-view-XXXXXX")
legacy_getters=$(mktemp "${TMPDIR:-/tmp}/libsixel-frombmp-legacy-getters-XXXXXX")
chunk_rebinds=$(mktemp "${TMPDIR:-/tmp}/libsixel-frombmp-chunk-rebinds-XXXXXX")
trap 'rm -f "$body" "$legacy_getters" "$chunk_rebinds"' EXIT HUP INT TERM

failed=0

if test ! -f "$source_file" || test ! -f "$header_file"; then
    echo "not ok 1 - BMP parser binds chunk bytes through view"
    echo "# missing source or header: $source_file $header_file"
    echo "not ok 2 - BMP decoder avoids legacy chunk byte getters"
    echo "# missing source or header: $source_file $header_file"
    echo "not ok 3 - BMP decode info avoids direct chunk rebinding"
    echo "# missing source or header: $source_file $header_file"
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
/^sixel_bmp_parse_header[[:space:]]*\(/ {
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
    echo "not ok 1 - BMP parser binds chunk bytes through view"
    echo "# sixel_bmp_parse_header was not found in $source_file"
    echo "not ok 2 - BMP decoder avoids legacy chunk byte getters"
    echo "# sixel_bmp_parse_header was not found in $source_file"
    echo "not ok 3 - BMP decode info avoids direct chunk rebinding"
    echo "# sixel_bmp_parse_header was not found in $source_file"
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
/info->source_bytes[[:space:]]*=[[:space:]]*view\.bytes[[:space:]]*;/ {
    saw_source_bytes_bind = 1
}
/info->source_size[[:space:]]*=[[:space:]]*view\.size[[:space:]]*;/ {
    saw_source_size_bind = 1
}
/buffer[[:space:]]*=[[:space:]]*info->source_bytes[[:space:]]*;/ {
    saw_buffer_bind = 1
}
/size[[:space:]]*=[[:space:]]*info->source_size[[:space:]]*;/ {
    saw_size_bind = 1
}
END {
    ok = saw_view_decl && saw_view_bytes_init && saw_view_size_init &&
        saw_get_bytes && saw_view_bytes_guard &&
        saw_source_bytes_bind && saw_source_size_bind &&
        saw_buffer_bind && saw_size_bind
    exit ok ? 0 : 1
}
' "$body"; then
    echo "ok 1 - BMP parser binds chunk bytes through view"
else
    echo "not ok 1 - BMP parser binds chunk bytes through view"
    echo "# parser must bind sixel_chunk_get_bytes(chunk, &view)"
    echo "# and then store view.bytes/view.size in decode info"
    failed=1
fi

awk '
/sixel_chunk_get_(buffer|size)[[:space:]]*\(/ {
    print
}
' "$source_file" > "$legacy_getters"

if test -s "$legacy_getters"; then
    echo "not ok 2 - BMP decoder avoids legacy chunk byte getters"
    sed "s|^|# legacy chunk byte getter: $source_file:|" "$legacy_getters"
    failed=1
else
    echo "ok 2 - BMP decoder avoids legacy chunk byte getters"
fi

awk '
/info->chunk/ {
    print FILENAME ":" FNR ":" $0
}
' "$source_file" "$header_file" > "$chunk_rebinds"

if test -s "$chunk_rebinds"; then
    echo "not ok 3 - BMP decode info avoids direct chunk rebinding"
    sed 's|^|# chunk rebind: |' "$chunk_rebinds"
    failed=1
else
    echo "ok 3 - BMP decode info avoids direct chunk rebinding"
fi

exit "$failed"
