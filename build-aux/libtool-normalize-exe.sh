#!/bin/sh
# Normalize libtool outputs that accidentally gain a second .exe suffix on MSVC.
#
# Usage: libtool-normalize-exe.sh -- LIBTOOL ...
# The script expects the real libtool command immediately after "--" and
# forwards all remaining arguments unchanged. When the link step succeeds, it
# renames any *.exe.exe files associated with the requested output back to the
# expected *.exe name and moves matching .manifest and .pdb sidecars.

set -eu

if [ "$#" -lt 1 ]; then
    printf '%s\n' "usage: $0 -- LIBTOOL ..." >&2
    exit 1
fi

if [ "$1" = "--" ]; then
    shift
fi

if [ "$#" -lt 1 ]; then
    printf '%s\n' "usage: $0 -- LIBTOOL ..." >&2
    exit 1
fi

libtool_cmd=$1
shift
libtool_args="$@"

set -- $libtool_args
output_path=""
prev=""
while [ $# -gt 0 ]; do
    case "$1" in
        -o)
            shift
            [ $# -gt 0 ] || break
            output_path=$1
            ;;
        -o*)
            output_path=${1#-o}
            ;;
    esac
    shift
done

set -- $libtool_args
"$libtool_cmd" "$@"
status=$?
if [ $status -ne 0 ]; then
    exit $status
fi

if [ -z "$output_path" ]; then
    exit 0
fi

base_dir=`dirname "$output_path"`
base_name=`basename "$output_path"`
case "$base_name" in
    *.exe)
        base_root=${base_name%".exe"}
        ;;
    *)
        base_root=$base_name
        ;;
esac

normalize_target() {
    src=$1
    dest=$2
    if [ -f "$src" ]; then
        mv "$src" "$dest"
        if [ -f "$src.manifest" ]; then
            mv "$src.manifest" "$dest.manifest"
        fi
        if [ -f "$src.pdb" ]; then
            mv "$src.pdb" "$dest.pdb"
        fi
    fi
}

normalize_target "$base_dir/$base_root.exe.exe" \
    "$base_dir/$base_root.exe"
normalize_target "$base_dir/.libs/$base_root.exe.exe" \
    "$base_dir/.libs/$base_root.exe"

exit 0
