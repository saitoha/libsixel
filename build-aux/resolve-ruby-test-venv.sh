#!/bin/sh
# Prepare an isolated Ruby gem environment for TAP tests.
#
# Usage:
#   resolve-ruby-test-venv.sh <enable_ruby> <ruby_bin> <gem_dir> <gem_home_dir> [source_dir] [libsixel_lib_dir]

set -eu

if [ "$#" -ne 4 ] && [ "$#" -ne 6 ]; then
    echo "Usage: $0 <enable_ruby> <ruby_bin> <gem_dir> <gem_home_dir> [source_dir] [libsixel_lib_dir]" >&2
    exit 1
fi

enable_ruby=$1
ruby_bin=$2
gem_dir=$3
shared_ruby_gem_home=$4
source_dir=${5-}
libsixel_lib_dir=${6-}
gem_path=
gem_marker=
gem_signature=
ruby_lib_path=
source_signature=
combined_signature=

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

resolve_libsixel_shared_lib() {
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

compute_source_signature() {
    src_dir=$1
    lib_dir=$2
    sig_file="${TMPDIR:-/tmp}/libsixel-ruby-source-signature-$$.tmp"
    src_lib=

    rm -f "$sig_file"
    : > "$sig_file"

    if [ -n "$src_dir" ] && [ -d "$src_dir" ]; then
        for path in \
            "$src_dir/gem_builder.rb" \
            "$src_dir/libsixel-ruby.gemspec"; do
            if [ -f "$path" ]; then
                cksum "$path" >> "$sig_file"
            fi
        done
        if [ -d "$src_dir/lib" ]; then
            find "$src_dir/lib" -type f -name '*.rb' \
                | LC_ALL=C sort \
                | while IFS= read -r path; do
                    cksum "$path"
                done >> "$sig_file"
        fi
    fi

    src_lib=$(resolve_libsixel_shared_lib "$lib_dir")
    if [ -n "$src_lib" ]; then
        cksum "$src_lib" >> "$sig_file"
    fi

    if [ ! -s "$sig_file" ]; then
        rm -f "$sig_file"
        printf "%s\n" ""
        return 0
    fi

    cksum "$sig_file" | awk '{print $1}'
    rm -f "$sig_file"
}

emit_assignment "SIXEL_TEST_RUBY" ""
emit_assignment "SIXEL_TEST_RUBY_GEM_HOME" ""
emit_assignment "SIXEL_TEST_RUBYLIB" ""

if [ "$enable_ruby" != "1" ]; then
    exit 0
fi

if [ -z "$ruby_bin" ]; then
    exit 0
fi

gem_path=$(find "$gem_dir" -maxdepth 1 -type f -name 'libsixel-ruby-*.gem' \
    2>/dev/null | head -n 1)
if [ -z "$gem_path" ]; then
    exit 0
fi

gem_marker="$shared_ruby_gem_home/.libsixel-ruby-gem-path"
gem_signature=$(cksum "$gem_path" | sed 's/ .*//')
source_signature=$(compute_source_signature "$source_dir" "$libsixel_lib_dir")
combined_signature="$gem_signature"
if [ -n "$source_signature" ]; then
    combined_signature="${gem_signature}:${source_signature}"
fi

if [ -f "$gem_marker" ]; then
    IFS= read -r installed_gem_signature < "$gem_marker" || \
        installed_gem_signature=
    if [ "$installed_gem_signature" != "$combined_signature" ]; then
        rm -rf "$shared_ruby_gem_home"
    fi
fi

if [ ! -d "$shared_ruby_gem_home" ]; then
    mkdir -p "$shared_ruby_gem_home"
fi

resolve_ruby_lib_path() {
    gem_root=
    gem_root=$(find "$shared_ruby_gem_home/gems" -maxdepth 1 -type d \
        -name 'libsixel-ruby-*' 2>/dev/null | LC_ALL=C sort | head -n 1)
    if [ -n "$gem_root" ] && [ -d "$gem_root/lib" ]; then
        printf "%s\n" "$gem_root/lib"
        return 0
    fi
    printf "%s\n" ""
}

can_require_libsixel() {
    ruby_libdir=$1
    if [ -z "$ruby_libdir" ]; then
        return 1
    fi
    RUBYLIB="$ruby_libdir" \
    GEM_HOME="$shared_ruby_gem_home" \
    GEM_PATH="$shared_ruby_gem_home" \
    "$ruby_bin" -e 'begin; require "libsixel"; rescue LoadError; exit 1; end' \
        >/dev/null 2>&1
}

ruby_lib_path=$(resolve_ruby_lib_path)
if ! can_require_libsixel "$ruby_lib_path"; then
    "$ruby_bin" -S gem install --local --no-document \
        --install-dir "$shared_ruby_gem_home" \
        "$gem_path" >/dev/null 2>&1 || true
fi

ruby_lib_path=$(resolve_ruby_lib_path)
if ! can_require_libsixel "$ruby_lib_path"; then
    rm -rf "$shared_ruby_gem_home"
    mkdir -p "$shared_ruby_gem_home"
    "$ruby_bin" -S gem install --local --no-document \
        --install-dir "$shared_ruby_gem_home" \
        "$gem_path" >/dev/null 2>&1 || true
fi

ruby_lib_path=$(resolve_ruby_lib_path)
if can_require_libsixel "$ruby_lib_path"; then
    printf "%s\n" "$combined_signature" >"$gem_marker"
    emit_assignment "SIXEL_TEST_RUBY" "$ruby_bin"
    emit_assignment "SIXEL_TEST_RUBY_GEM_HOME" "$shared_ruby_gem_home"
    emit_assignment "SIXEL_TEST_RUBYLIB" "$ruby_lib_path"
fi
