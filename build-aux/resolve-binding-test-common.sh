#!/bin/sh
# Shared helpers for binding test virtualenv resolver scripts.

sixel_quote_single() {
    if [ -z "$1" ]; then
        printf "''"
        return 0
    fi
    printf "%s" "$1" | sed "s/'/'\\\\''/g;1s/^/'/;\$s/\$/'/"
}

sixel_resolve_libsixel_shared_lib() {
    lib_dir=$1
    if [ -z "$lib_dir" ] || [ ! -d "$lib_dir" ]; then
        printf "%s\n" ""
        return 0
    fi
    for candidate in \
        "$lib_dir/libsixel.so.1" \
        "$lib_dir/libsixel.1.so" \
        "$lib_dir/libsixel.so" \
        "$lib_dir/libsixel.1.dylib" \
        "$lib_dir/libsixel.dylib" \
        "$lib_dir/libsixel.dll" \
        "$lib_dir/libsixel-1.dll"; do
        if [ -f "$candidate" ]; then
            printf "%s\n" "$candidate"
            return 0
        fi
    done
    printf "%s\n" ""
}
