#!/bin/sh
set -eu

fail()
{
    printf '%s\n' "$*" >&2
    exit 1
}

file_path()
(
    case "$1" in
        /*) path=$1 ;;
        *) path=$PWD/$1 ;;
    esac
    test -f "$path" || fail "Missing input: $path"
    CDPATH='' cd "${path%/*}/"
    printf '%s/%s\n' "$(pwd -P)" "${path##*/}"
)

check_elf_exports()
{
    LC_ALL=C nm -D --defined-only --format=posix "$old_library" > "$work/old-nm"
    LC_ALL=C nm -D --defined-only --format=posix "$current_library" \
        > "$work/current-nm"
    LC_ALL=C awk '
        FILENAME == ARGV[1] {
            # The legacy header puts public function names on their own lines.
            if (match($0, /^sixel_[[:alnum:]_]+\(/)) {
                public[substr($0, 1, RLENGTH - 1)] = 1
            }
            next
        }
        {
            # An unversioned client can also link to a default ELF version.
            sub(/@@.*/, "", $1)
            exported[FILENAME, $1] = 1
        }
        END {
            if ((ARGV[3], "sixel_frame_set_colorspace") in exported) {
                print "Retired frame setter remains an ELF export"
                missing = 1
            }
            for (name in public) {
                if (!((ARGV[2], name) in exported)) continue
                ++checked
                if ((ARGV[3], name) in exported) continue
                print "Missing legacy public ELF export: " name
                missing = 1
            }
            if (checked == 0 || missing) exit 1
            print "PASS legacy public ELF exports (" checked ")"
        }
    ' "$old_header" "$work/old-nm" "$work/current-nm" ||
        fail "The current library must export the v$legacy_version symbols"
}

run_cases()
{
    result=0
    for test_case in rgb565 bgr888 pal8 pal8-partial-band \
            frame clip normalize scale dither encode-bytes registers allocator \
            builtins option-choices selector-order low-level complexion; do
        if LD_LIBRARY_PATH=${2%/*} DYLD_LIBRARY_PATH=${2%/*} \
                "$work/client" "$2" "$test_case" "$work/encoded.six" "$1"; then
            printf 'PASS %s %s\n' "$1" "$test_case"
        else
            printf 'FAIL %s %s\n' "$1" "$test_case" >&2
            result=1
        fi
    done
    return "$result"
}

test "$#" -eq 4 || test "$#" -eq 5 ||
    fail "Usage: $0 OLD_SIXEL_H OLD_LIBRARY CURRENT_LIBRARY CURRENT_SIXEL_H [OLD_VERSION]"

legacy_version=${5:-1.8.7-r2}
case "$legacy_version" in
    1.8.7|1.8.7-r2) ;;
    *) fail "Unsupported legacy version: $legacy_version" ;;
esac

test_dir=$(CDPATH='' cd "$(dirname "$0")" && pwd -P)
old_header=$(file_path "$1")
old_library=$(file_path "$2")
current_library=$(file_path "$3")
current_header=$(file_path "$4")
test "${old_header##*/}" = sixel.h && test "${current_header##*/}" = sixel.h ||
    fail "Both header arguments must name a generated sixel.h"
test "$old_header" -ef "$current_header" &&
    fail "The legacy and current headers must be different files"
test "$old_library" -ef "$current_library" &&
    fail "The legacy and current libraries must be different files"

: "${CC:=cc}"
case $(uname -s) in
    Linux)
        loader_name=libsixel.so.1
        for library in "$old_library" "$current_library"; do
            LC_ALL=C readelf -d "$library" |
                awk '$2 == "(SONAME)" && $NF == "[libsixel.so.1]" { found=1 }
                     END { exit !found }' ||
                fail "$library does not declare SONAME $loader_name"
        done
        set -- -ldl
        ;;
    Darwin)
        loader_name=libsixel.1.dylib
        old_install_name=$(otool -D "$old_library" | awk 'NR == 2')
        current_install_name=$(otool -D "$current_library" | awk 'NR == 2')
        test "${old_install_name##*/}" = "$loader_name" ||
            fail "$old_library does not declare install name $loader_name"
        test "${current_install_name##*/}" = "$loader_name" ||
            fail "$current_library does not declare install name $loader_name"
        set --
        ;;
    *)
        fail "This compatibility check requires Linux or macOS"
        ;;
esac

for library in "$old_library" "$current_library"; do
    test "$library" -ef "${library%/*}/$loader_name" ||
        fail "$library is not available under its loader name $loader_name"
done

work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-legacy-client.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
trap 'exit 1' HUP INT TERM

# Compile the runtime client only once. PIC gives dladdr the address of the
# imported library symbol rather than a possible executable linkage stub.
"$CC" -std=c99 -fPIC -D_GNU_SOURCE -I"${old_header%/*}" \
    -DLEGACY_VERSION="\"$legacy_version\"" \
    "$test_dir/legacy-client.inc.c" "$old_library" "$@" -o "$work/client"
case "$loader_name" in
    libsixel.so.1)
        LC_ALL=C readelf -d "$work/client" |
            awk '$2 == "(NEEDED)" && $NF == "[libsixel.so.1]" { found=1 }
                 END { exit !found }' ||
            fail "The client does not need $loader_name"
        check_elf_exports
        ;;
    libsixel.1.dylib)
        otool -L "$work/client" |
            awk -v name="$old_install_name" \
                'NR > 1 && index($0, name " (") { found=1 }
                 END { exit !found }' ||
            fail "The client does not need $old_install_name"
        ;;
esac

CC="$CC" sh "$test_dir/check-header.sh" "$old_header" "$current_header" "$work"

unset LD_PRELOAD LD_AUDIT DYLD_INSERT_LIBRARIES DYLD_VERSIONED_LIBRARY_PATH
run_cases "v$legacy_version" "$old_library"
run_cases current "$current_library"
