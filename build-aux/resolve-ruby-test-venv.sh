#!/bin/sh
# Prepare an isolated Ruby gem environment for TAP tests.
#
# Usage:
#   resolve-ruby-test-venv.sh <enable_ruby> <ruby_bin> <gem_dir> <gem_home_dir>

set -eu

if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <enable_ruby> <ruby_bin> <gem_dir> <gem_home_dir>" >&2
    exit 1
fi

enable_ruby=$1
ruby_bin=$2
gem_dir=$3
shared_ruby_gem_home=$4
gem_path=
gem_marker=
gem_signature=
ruby_lib_path=

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

if [ -f "$gem_marker" ]; then
    IFS= read -r installed_gem_signature < "$gem_marker" || \
        installed_gem_signature=
    if [ "$installed_gem_signature" != "$gem_signature" ]; then
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
    printf "%s\n" "$gem_signature" >"$gem_marker"
    emit_assignment "SIXEL_TEST_RUBY" "$ruby_bin"
    emit_assignment "SIXEL_TEST_RUBY_GEM_HOME" "$shared_ruby_gem_home"
    emit_assignment "SIXEL_TEST_RUBYLIB" "$ruby_lib_path"
fi
