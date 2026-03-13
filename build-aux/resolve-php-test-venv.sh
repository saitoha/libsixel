#!/bin/sh
# Prepare an isolated PHP test environment from the packaged archive.
#
# Usage:
#   resolve-php-test-venv.sh <enable_php> <php_bin> <package_dir> <venv_dir>

set -eu

if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <enable_php> <php_bin> <package_dir> <venv_dir>" >&2
    exit 1
fi

enable_php=$1
configured_php=$2
package_dir=$3
shared_php_venv=$4
php_bin=
binding_root=
lib_dir=
lib_path=
archive_path=
archive_marker=
archive_signature=
extract_root=
stage_root=

quote_single() {
    if [ -z "$1" ]; then
        printf "''"
        return 0
    fi
    printf "%s" "$1" | sed "s/'/'\\''/g;1s/^/'/;\$s/\$/'/"
}

emit_assignment() {
    key=$1
    value=$2
    quoted_value=$(quote_single "$value")
    printf "%s=%s\n" "$key" "$quoted_value"
}

emit_assignment "SIXEL_TEST_PHP" ""
emit_assignment "SIXEL_TEST_PHP_BINDING_ROOT" ""
emit_assignment "SIXEL_TEST_PHP_LIBDIR" ""
emit_assignment "SIXEL_TEST_PHP_LIBPATH" ""

if [ "$enable_php" != "1" ]; then
    exit 0
fi

resolve_php_bin() {
    if [ -x "$shared_php_venv/bin/php" ]; then
        printf "%s\n" "$shared_php_venv/bin/php"
        return 0
    fi

    if [ -x "$shared_php_venv/Scripts/php.exe" ]; then
        printf "%s\n" "$shared_php_venv/Scripts/php.exe"
        return 0
    fi

    if [ -x "$shared_php_venv/php/bin/php" ]; then
        printf "%s\n" "$shared_php_venv/php/bin/php"
        return 0
    fi

    case "$configured_php" in
        "$shared_php_venv"/*)
            if [ -x "$configured_php" ]; then
                printf "%s\n" "$configured_php"
                return 0
            fi
            ;;
    esac

    printf "%s\n" ""
}

find_archive() {
    find "$package_dir" -maxdepth 1 -type f \
        \( -name 'libsixel-php-*.tar.gz' -o -name 'libsixel-php-*.zip' \) \
        2>/dev/null | LC_ALL=C sort | head -n 1
}

resolve_binding_root() {
    resolved_root=$(find "$extract_root" -mindepth 1 -maxdepth 1 -type d \
        -name 'libsixel-php-*' 2>/dev/null | LC_ALL=C sort | head -n 1)
    if [ -n "$resolved_root" ] && [ -f "$resolved_root/src/autoload.php" ]; then
        printf "%s\n" "$resolved_root"
        return 0
    fi

    if [ -f "$extract_root/src/autoload.php" ]; then
        printf "%s\n" "$extract_root"
        return 0
    fi

    printf "%s\n" ""
}

extract_archive() {
    archive=$1
    rm -rf "$stage_root"
    mkdir -p "$extract_root"

    case "$archive" in
        *.tar.gz)
            tar -xzf "$archive" -C "$extract_root" >/dev/null 2>&1 || return 1
            ;;
        *.zip)
            if command -v unzip >/dev/null 2>&1; then
                unzip -oq "$archive" -d "$extract_root" >/dev/null 2>&1 || return 1
            elif command -v bsdtar >/dev/null 2>&1; then
                bsdtar -xf "$archive" -C "$extract_root" >/dev/null 2>&1 || return 1
            elif command -v tar >/dev/null 2>&1; then
                tar -xf "$archive" -C "$extract_root" >/dev/null 2>&1 || return 1
            else
                return 1
            fi
            ;;
        *)
            return 1
            ;;
    esac

    return 0
}

resolve_libpath() {
    find "$lib_dir" -maxdepth 1 -type f \
        \( -name '*.so' -o -name '*.so.*' -o -name '*.dylib' -o -name '*.dll' \) \
        2>/dev/null | LC_ALL=C sort | head -n 1
}

php_bin=$(resolve_php_bin)
if [ -z "$php_bin" ] || [ ! -x "$php_bin" ]; then
    exit 0
fi

if ! "$php_bin" -m 2>/dev/null | grep -Eq '^FFI$'; then
    exit 0
fi

archive_path=$(find_archive)
if [ -z "$archive_path" ] || [ ! -f "$archive_path" ]; then
    exit 0
fi

archive_signature=$(cksum "$archive_path" | sed 's/ .*//')
archive_marker="$shared_php_venv/.libsixel-php-package-signature"
stage_root="$shared_php_venv/libsixel-php-package"
extract_root="$stage_root/extracted"

installed_signature=
if [ -f "$archive_marker" ]; then
    IFS= read -r installed_signature < "$archive_marker" || installed_signature=
fi

if [ ! -d "$extract_root" ] || [ "$installed_signature" != "$archive_signature" ]; then
    if ! extract_archive "$archive_path"; then
        exit 0
    fi
fi

binding_root=$(resolve_binding_root)
if [ -z "$binding_root" ] || [ ! -f "$binding_root/src/autoload.php" ]; then
    if ! extract_archive "$archive_path"; then
        exit 0
    fi
    binding_root=$(resolve_binding_root)
fi
if [ -z "$binding_root" ] || [ ! -f "$binding_root/src/autoload.php" ]; then
    exit 0
fi

lib_dir="$binding_root/src/_libs"
if [ ! -d "$lib_dir" ]; then
    exit 0
fi

lib_path=$(resolve_libpath)
if [ -z "$lib_path" ] || [ ! -f "$lib_path" ]; then
    exit 0
fi

mkdir -p "$shared_php_venv"
printf "%s\n" "$archive_signature" > "$archive_marker"

emit_assignment "SIXEL_TEST_PHP" "$php_bin"
emit_assignment "SIXEL_TEST_PHP_BINDING_ROOT" "$binding_root"
emit_assignment "SIXEL_TEST_PHP_LIBDIR" "$lib_dir"
emit_assignment "SIXEL_TEST_PHP_LIBPATH" "$lib_path"
