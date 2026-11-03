#!/bin/sh
# Keep serializer packet capacity inside the declared C object extent.
# Policy: docs/testing/staticcheck.md
# Policy: docs/encoder/output/encode-policy/README.md
set -eux
echo '1..1'
set -v
awk '
function compact(text) {
    gsub(/\/\*([^*]|\*+[^*\/])*\*+\//, "", text)
    gsub(/\/\/[^\n]*/, "", text)
    gsub(/[[:space:]]/, "", text)
    return text
}
function capacity(text) {
    text = compact(text)
    return text ~ /unsignedcharbuffer\[(SIXEL_OUTPUT_PACKET_SIZE\*2|2\*SIXEL_OUTPUT_PACKET_SIZE)\];/
}
function allocation(text) {
    text = compact(text)
    return text ~ /size=sizeof\(sixel_output_t\);/ &&
        text ~ /sixel_allocator_malloc\(allocator,size\)/
}
BEGIN {
    good = "unsigned char buffer[SIXEL_OUTPUT_PACKET_SIZE * 2];"
    alloc = "size = sizeof(sixel_output_t); " \
        "output = sixel_allocator_malloc(allocator, size);"
    if (!capacity(good) || !allocation(alloc) ||
        capacity("unsigned char buffer[1];") ||
        capacity("unsigned char buffer[SIXEL_OUTPUT_PACKET_SIZE];") ||
        capacity("/* " good " */ unsigned char buffer[1];") ||
        allocation("size = sizeof(sixel_output_t) + 1; " \
            "output = sixel_allocator_malloc(allocator, size);") ||
        allocation("size = sizeof(sixel_output_t); " \
            "output = sixel_allocator_malloc(allocator, 1);")) {
        print "# output buffer scanner controls failed"
        failed = 1
    }
}
FILENAME ~ /encoder-core-private.h$/ { header = header $0 "\n" }
FILENAME ~ /encoder-core.c$/ {
    if ($0 ~ /^sixel_output_storage_new\(/) in_storage = 1
    if (in_storage) storage = storage $0 "\n"
    if ($0 ~ /^}/) in_storage = 0
}
END {
    if (!capacity(header) || !allocation(storage)) {
        print "# output buffer declaration and object allocation disagree"
        failed = 1
    }
    exit failed
}
' "$1/src/encoder-core-private.h" "$1/src/encoder-core.c" || {
    echo 'not ok 1 - serializer storage contains two complete packets'
    exit 0
}
echo 'ok 1 - serializer storage contains two complete packets'
exit 0
