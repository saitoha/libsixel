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
phpdbg_bin=
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
emit_assignment "SIXEL_TEST_PHPDBG" ""
emit_assignment "SIXEL_TEST_PHP_BINDING_ROOT" ""
emit_assignment "SIXEL_TEST_PHP_LIBDIR" ""
emit_assignment "SIXEL_TEST_PHP_LIBPATH" ""

if [ "$enable_php" != "1" ]; then
    exit 0
fi

is_windows_absolute_path() {
    case "$1" in
        [A-Za-z]:[\\/]*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

to_shell_path() {
    input_path=$(printf "%s" "$1" | tr -d '\r')
    if [ -z "$input_path" ]; then
        printf "%s\n" ""
        return 0
    fi

    if is_windows_absolute_path "$input_path"; then
        if command -v cygpath >/dev/null 2>&1; then
            converted_path=$(cygpath -u "$input_path" 2>/dev/null || printf "")
            if [ -n "$converted_path" ]; then
                printf "%s\n" "$converted_path"
                return 0
            fi
        fi
        printf "%s\n" "$input_path" | sed 's#\\#/#g'
        return 0
    fi

    printf "%s\n" "$input_path"
}

to_php_path() {
    input_path=$(printf "%s" "$1" | tr -d '\r')
    if [ -z "$input_path" ]; then
        printf "%s\n" ""
        return 0
    fi

    if command -v cygpath >/dev/null 2>&1; then
        converted_path=$(cygpath -m "$input_path" 2>/dev/null || printf "")
        if [ -n "$converted_path" ]; then
            printf "%s\n" "$converted_path"
            return 0
        fi
    fi

    if is_windows_absolute_path "$input_path"; then
        printf "%s\n" "$input_path" | sed 's#\\#/#g'
        return 0
    fi

    printf "%s\n" "$input_path"
}

resolve_executable() {
    candidate=$(printf "%s" "$1" | tr -d '\r')
    if [ -z "$candidate" ]; then
        printf "%s\n" ""
        return 0
    fi

    normalized_candidate=$(to_shell_path "$candidate")
    for probe in "$normalized_candidate" "$candidate"; do
        if [ -z "$probe" ]; then
            continue
        fi
        if [ -x "$probe" ] || [ -f "$probe" ]; then
            printf "%s\n" "$probe"
            return 0
        fi
        resolved=$(command -v "$probe" 2>/dev/null || printf "")
        if [ -n "$resolved" ] && { [ -x "$resolved" ] || [ -f "$resolved" ]; }; then
            printf "%s\n" "$resolved"
            return 0
        fi
    done

    candidate_forward=$(printf "%s" "$candidate" | sed 's#\\#/#g')
    candidate_basename=${candidate_forward##*/}
    for probe in "$candidate_basename" "php" "php.exe"; do
        if [ -z "$probe" ]; then
            continue
        fi
        resolved=$(command -v "$probe" 2>/dev/null || printf "")
        if [ -n "$resolved" ] && { [ -x "$resolved" ] || [ -f "$resolved" ]; }; then
            printf "%s\n" "$resolved"
            return 0
        fi
    done

    printf "%s\n" ""
}

resolve_phpdbg_candidate() {
    base_php=$1
    for candidate in \
        "$(dirname "$base_php")/phpdbg" \
        "$(dirname "$base_php")/phpdbg.exe" \
        "$(command -v phpdbg 2>/dev/null || printf '')"; do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            printf "%s\n" "$candidate"
            return 0
        fi
    done
    printf "%s\n" ""
}

write_wrapper() {
    wrapper_path=$1
    target_path=$2
    shift 2
    mkdir -p "$(dirname "$wrapper_path")"
    rm -f "$wrapper_path"
    {
        printf "%s\n" "#!/bin/sh"
        printf "%s\n" "set -eu"
        printf "%s" "exec $(quote_single "$target_path")"
        while [ "$#" -gt 0 ]; do
            printf " %s" "$(quote_single "$1")"
            shift
        done
        printf " \"\$@\"\n"
    } > "$wrapper_path"
    chmod +x "$wrapper_path" 2>/dev/null || true
}

probe_ffi_with_php() {
    probe_php=$1
    shift
    probe_timeout=${SIXEL_TEST_PHP_PROBE_TIMEOUT-10}
    probe_basename=$(basename "$probe_php")
    probe_code='try { if (!class_exists("FFI", false)) { exit(1); } FFI::cdef("typedef int libsixel_probe_t;"); exit(0); } catch (Throwable $e) { exit(1); }'

    run_with_timeout() {
        if command -v timeout >/dev/null 2>&1 &&
                printf '%s' "$probe_timeout" | grep -Eq '^[1-9][0-9]*$'; then
            timeout "$probe_timeout" "$@" >/dev/null 2>&1
        else
            "$@" >/dev/null 2>&1
        fi
    }

    case "$probe_basename" in
        phpdbg|phpdbg.exe)
            probe_file="${TMPDIR:-/tmp}/libsixel-php-ffi-probe-$$-$(date +%s).php"
            if ! printf '%s\n' "<?php $probe_code" > "$probe_file"; then
                return 1
            fi
            run_with_timeout "$probe_php" "$@" -qrr "$probe_file"
            probe_status=$?
            rm -f "$probe_file"
            return "$probe_status"
            ;;
        *)
            run_with_timeout "$probe_php" "$@" -r "$probe_code"
            ;;
    esac
}

resolve_php_ext_dir() {
    base_php=$1
    ext_dir1=$(dirname "$base_php")/ext
    ext_dir2=$(to_shell_path "$ext_dir1")
    for candidate in "$ext_dir1" "$ext_dir2"; do
        if [ -n "$candidate" ] && [ -d "$candidate" ]; then
            printf "%s\n" "$candidate"
            return 0
        fi
    done
    printf "%s\n" ""
}

setup_php_wrapper() {
    wrapper_path=$1
    target_php=$2

    if probe_ffi_with_php "$target_php" -d ffi.enable=1; then
        write_wrapper "$wrapper_path" "$target_php" -d ffi.enable=1
        return 0
    fi
    if probe_ffi_with_php "$target_php" -d extension=ffi -d ffi.enable=1; then
        write_wrapper "$wrapper_path" "$target_php" -d extension=ffi -d ffi.enable=1
        return 0
    fi
    if probe_ffi_with_php "$target_php" -d extension=php_ffi -d ffi.enable=1; then
        write_wrapper "$wrapper_path" "$target_php" -d extension=php_ffi -d ffi.enable=1
        return 0
    fi
    if probe_ffi_with_php "$target_php" -d extension=php_ffi.dll -d ffi.enable=1; then
        write_wrapper "$wrapper_path" "$target_php" -d extension=php_ffi.dll -d ffi.enable=1
        return 0
    fi

    php_ext_dir=$(resolve_php_ext_dir "$target_php")
    if [ -n "$php_ext_dir" ]; then
        if probe_ffi_with_php "$target_php" -d "extension_dir=$php_ext_dir" -d extension=ffi -d ffi.enable=1; then
            write_wrapper "$wrapper_path" "$target_php" -d "extension_dir=$php_ext_dir" -d extension=ffi -d ffi.enable=1
            return 0
        fi
        if probe_ffi_with_php "$target_php" -d "extension_dir=$php_ext_dir" -d extension=php_ffi.dll -d ffi.enable=1; then
            write_wrapper "$wrapper_path" "$target_php" -d "extension_dir=$php_ext_dir" -d extension=php_ffi.dll -d ffi.enable=1
            return 0
        fi
    fi

    return 1
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

package_dir=$(to_shell_path "$package_dir")
shared_php_venv=$(to_shell_path "$shared_php_venv")

configured_php_bin=$(resolve_executable "$configured_php")
if [ -z "$configured_php_bin" ]; then
    exit 0
fi

php_wrapper="$shared_php_venv/bin/php"
if ! setup_php_wrapper "$php_wrapper" "$configured_php_bin"; then
    exit 0
fi
if [ ! -f "$php_wrapper" ]; then
    exit 0
fi
php_bin="$php_wrapper"

phpdbg_real=$(resolve_phpdbg_candidate "$configured_php_bin")
if [ -n "$phpdbg_real" ]; then
    phpdbg_wrapper="$shared_php_venv/bin/phpdbg"
    if setup_php_wrapper "$phpdbg_wrapper" "$phpdbg_real"; then
        phpdbg_bin="$phpdbg_wrapper"
    fi
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
emit_assignment "SIXEL_TEST_PHPDBG" "$phpdbg_bin"
emit_assignment "SIXEL_TEST_PHP_BINDING_ROOT" "$(to_php_path "$binding_root")"
emit_assignment "SIXEL_TEST_PHP_LIBDIR" "$(to_php_path "$lib_dir")"
emit_assignment "SIXEL_TEST_PHP_LIBPATH" "$(to_php_path "$lib_path")"
