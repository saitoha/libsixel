#!/bin/sh
# Prepare an isolated Perl local::lib environment for TAP tests.
#
# Usage:
#   resolve-perl-test-venv.sh <enable_perl> <perl_bin> <perl_source_dir> \
#       <libsixel_lib_dir> <venv_dir>

set -eu

if [ "$#" -ne 5 ]; then
    echo "Usage: $0 <enable_perl> <perl_bin> <perl_source_dir> <libsixel_lib_dir> <venv_dir>" >&2
    exit 1
fi

enable_perl=$1
perl_bin=$2
perl_source_dir=$3
libsixel_lib_dir=$4
shared_perl_venv=$5
perl_local_lib_root=
perl5lib=
perl_mb_opt=
perl_mm_opt=
cpanm_path=
libsixel_shared_lib=
perl_wrapper=

quote_single() {
    if [ -z "$1" ]; then
        printf "''"
        return 0
    fi
    printf "%s" "$1" | sed "s/'/'\\\\''/g;1s/^/'/;\$s/\$/'/"
}

emit_assignment() {
    key=$1
    value=$2
    quoted_value=$(quote_single "$value")
    printf "%s=%s\n" "$key" "$quoted_value"
}

emit_assignment "SIXEL_TEST_PERL" ""
emit_assignment "SIXEL_TEST_PERL_CPANM" ""
emit_assignment "SIXEL_TEST_PERL_LOCAL_LIB_ROOT" ""
emit_assignment "SIXEL_TEST_PERL5LIB" ""
emit_assignment "SIXEL_TEST_PERL_MB_OPT" ""
emit_assignment "SIXEL_TEST_PERL_MM_OPT" ""

if [ "$enable_perl" != "1" ]; then
    exit 0
fi

if [ -z "$perl_bin" ]; then
    exit 0
fi

if [ ! -x "$perl_bin" ]; then
    perl_bin_resolved=$(command -v "$perl_bin" 2>/dev/null || printf "")
    if [ -z "$perl_bin_resolved" ] || [ ! -x "$perl_bin_resolved" ]; then
        exit 0
    fi
    perl_bin=$perl_bin_resolved
fi

if [ ! -f "$perl_source_dir/lib/Image/LibSIXEL.pm" ]; then
    exit 0
fi

if [ ! -d "$libsixel_lib_dir" ]; then
    exit 0
fi

archname=$("$perl_bin" -MConfig -e 'print $Config{archname}' 2>/dev/null || printf "")
if [ -z "$archname" ]; then
    exit 0
fi

perl_local_lib_root=$shared_perl_venv
perl_lib_root="$perl_local_lib_root/lib/perl5"
perl_lib_arch="$perl_lib_root/$archname"
perl5lib="$perl_lib_arch:$perl_lib_root"
perl_mb_opt="--install_base $perl_local_lib_root"
perl_mm_opt="INSTALL_BASE=$perl_local_lib_root"

source_signature=$(
    cd "$perl_source_dir"
    find . -type f \
        -path './lib/*' \
        | LC_ALL=C sort \
        | while IFS= read -r path; do
            cksum "$path"
        done \
        | cksum | awk '{print $1}'
)
source_marker="$perl_local_lib_root/.libsixel-perl-source-signature"

resolve_libsixel_shared_lib() {
    for candidate in \
        "$libsixel_lib_dir/libsixel.so.1" \
        "$libsixel_lib_dir/libsixel.1.so" \
        "$libsixel_lib_dir/libsixel.so" \
        "$libsixel_lib_dir/libsixel.1.dylib" \
        "$libsixel_lib_dir/libsixel.dylib" \
        "$libsixel_lib_dir/libsixel.dll" \
        "$libsixel_lib_dir/libsixel-1.dll"; do
        if [ -f "$candidate" ]; then
            printf "%s\n" "$candidate"
            return 0
        fi
    done
    printf "%s\n" ""
}

resolve_cpanm_path() {
    local_cpanm="$perl_local_lib_root/bin/cpanm"
    if [ -x "$local_cpanm" ]; then
        printf "%s\n" "$local_cpanm"
        return 0
    fi

    if command -v cpanm >/dev/null 2>&1; then
        command -v cpanm
        return 0
    fi

    printf "%s\n" ""
}

module_is_usable() {
    module_path=$(
        PERL5LIB="$perl5lib" \
        PERL_LOCAL_LIB_ROOT="$perl_local_lib_root" \
        PERL_MB_OPT="$perl_mb_opt" \
        PERL_MM_OPT="$perl_mm_opt" \
        "$perl_bin" -MImage::LibSIXEL -e \
            'print $INC{"Image/LibSIXEL.pm"} // q{}' \
            2>/dev/null || printf ""
    )
    case "$module_path" in
        "$perl_local_lib_root"/*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

deps_are_usable() {
    PERL5LIB="$perl5lib" \
    PERL_LOCAL_LIB_ROOT="$perl_local_lib_root" \
    PERL_MB_OPT="$perl_mb_opt" \
    PERL_MM_OPT="$perl_mm_opt" \
    "$perl_bin" -MFFI::Platypus -MFFI::Platypus::Buffer \
        -MFFI::Platypus::Closure -e 'exit 0' >/dev/null 2>&1
}

install_perl_dependencies() {
    cpanm_timeout=${SIXEL_TEST_PERL_CPANM_TIMEOUT-300}

    if [ -z "$cpanm_path" ]; then
        return 1
    fi
    if command -v timeout >/dev/null 2>&1 &&
            printf '%s' "$cpanm_timeout" | grep -Eq '^[1-9][0-9]*$'; then
        PERL_CPANM_HOME="$perl_local_lib_root/.cpanm" \
        PERL5LIB="$perl5lib" \
        PERL_LOCAL_LIB_ROOT="$perl_local_lib_root" \
        PERL_MB_OPT="$perl_mb_opt" \
        PERL_MM_OPT="$perl_mm_opt" \
        timeout "$cpanm_timeout" "$perl_bin" "$cpanm_path" \
            --quiet --notest --skip-satisfied \
            --local-lib-contained "$perl_local_lib_root" \
            FFI::Platypus >/dev/null 2>&1 || true
    else
        PERL_CPANM_HOME="$perl_local_lib_root/.cpanm" \
        PERL5LIB="$perl5lib" \
        PERL_LOCAL_LIB_ROOT="$perl_local_lib_root" \
        PERL_MB_OPT="$perl_mb_opt" \
        PERL_MM_OPT="$perl_mm_opt" \
        "$perl_bin" "$cpanm_path" \
            --quiet --notest --skip-satisfied \
            --local-lib-contained "$perl_local_lib_root" \
            FFI::Platypus >/dev/null 2>&1 || true
    fi
    deps_are_usable
}

write_perl_wrapper() {
    perl_wrapper="$perl_local_lib_root/bin/perl"
    mkdir -p "$perl_local_lib_root/bin"
    cat >"$perl_wrapper" <<EOF
#!/bin/sh
set -eu
PERL5LIB='$perl5lib' \\
PERL_LOCAL_LIB_ROOT='$perl_local_lib_root' \\
PERL_MB_OPT='$perl_mb_opt' \\
PERL_MM_OPT='$perl_mm_opt' \\
'$perl_bin' "\$@"
EOF
    chmod +x "$perl_wrapper" 2>/dev/null || true
}

stage_binding() {
    staged_root="$perl_local_lib_root/lib/perl5/Image"
    staged_lib_dir="$staged_root/LibSIXEL"
    mkdir -p "$staged_lib_dir"

    cp "$perl_source_dir/lib/Image/LibSIXEL.pm" \
        "$staged_root/LibSIXEL.pm"
    cp "$perl_source_dir/lib/Image/LibSIXEL/Constants.pm" \
        "$staged_lib_dir/Constants.pm"
    cp "$perl_source_dir/lib/Image/LibSIXEL/GeneratedAttach.pm" \
        "$staged_lib_dir/GeneratedAttach.pm"
    cp "$perl_source_dir/lib/Image/LibSIXEL/Encoder.pm" \
        "$staged_lib_dir/Encoder.pm"
    cp "$perl_source_dir/lib/Image/LibSIXEL/Decoder.pm" \
        "$staged_lib_dir/Decoder.pm"

    rm -f "$staged_lib_dir"/libsixel.so \
        "$staged_lib_dir"/libsixel.so.1 \
        "$staged_lib_dir"/libsixel.dylib \
        "$staged_lib_dir"/libsixel.dll \
        "$staged_lib_dir"/libsixel-1.dll
    case "$libsixel_shared_lib" in
        *.dylib)
            cp "$libsixel_shared_lib" "$staged_lib_dir/libsixel.dylib"
            ;;
        *.dll)
            cp "$libsixel_shared_lib" "$staged_lib_dir/libsixel.dll"
            ;;
        *)
            cp "$libsixel_shared_lib" "$staged_lib_dir/libsixel.so.1"
            cp "$libsixel_shared_lib" "$staged_lib_dir/libsixel.so"
            ;;
    esac
}

if [ ! -d "$perl_local_lib_root" ]; then
    mkdir -p "$perl_local_lib_root"
fi

libsixel_shared_lib=$(resolve_libsixel_shared_lib)
if [ -z "$libsixel_shared_lib" ]; then
    exit 0
fi
lib_signature=$(cksum "$libsixel_shared_lib" | sed 's/ .*//')
combined_signature="${source_signature}:${lib_signature}"

if [ -f "$source_marker" ]; then
    IFS= read -r installed_source_signature < "$source_marker" || \
        installed_source_signature=
else
    installed_source_signature=
fi

if [ "$installed_source_signature" != "$combined_signature" ]; then
    rm -rf "$perl_local_lib_root"
    mkdir -p "$perl_local_lib_root"
fi

if ! deps_are_usable; then
    cpanm_path=$(resolve_cpanm_path)
    if ! install_perl_dependencies; then
        exit 0
    fi
fi

if [ -z "$cpanm_path" ]; then
    cpanm_path=$(resolve_cpanm_path)
fi

if ! module_is_usable; then
    stage_binding
fi

if ! module_is_usable; then
    exit 0
fi

if module_is_usable; then
    printf "%s\n" "$combined_signature" >"$source_marker"
    write_perl_wrapper
    if [ ! -x "$perl_wrapper" ]; then
        exit 0
    fi
    emit_assignment "SIXEL_TEST_PERL" "$perl_wrapper"
    emit_assignment "SIXEL_TEST_PERL_CPANM" "$cpanm_path"
    emit_assignment "SIXEL_TEST_PERL_LOCAL_LIB_ROOT" "$perl_local_lib_root"
    emit_assignment "SIXEL_TEST_PERL5LIB" "$perl5lib"
    emit_assignment "SIXEL_TEST_PERL_MB_OPT" "$perl_mb_opt"
    emit_assignment "SIXEL_TEST_PERL_MM_OPT" "$perl_mm_opt"
fi
