#!/bin/sh
# Build CI harnesses and libsixel with the same Clang coverage instrumentation.
set -eu

buildtool=$1
shift
export CC=clang-21 CXX=clang++-21
# Add feedback from library objects. Keep ASan/UBSan on the harnesses.
coverage=-fsanitize-coverage=inline-8bit-counters,pc-table,trace-cmp
# Configure links instrumented probes before the fuzz harness is available.
probe_runtime=-fsanitize=address
jobs=$(nproc)

# Static linkage puts all instrumented objects under one libFuzzer runtime.
# Disable optional backends other than the dedicated WebP component.
case "$buildtool" in
    autotools)
        CFLAGS="-O1 -g -fno-omit-frame-pointer $coverage" \
            LDFLAGS="$probe_runtime" \
            ./configure --enable-fuzz --disable-shared --disable-tests \
            --without-metal --without-vulkan --with-webp \
            --without-png --without-jpeg --without-tiff --without-lcms2
        make -j"$jobs" all-src-lib
        make -j"$jobs" -C fuzz "$@"
        ;;
    meson)
        meson setup builddir -Dfuzz=true -Dfuzz_sanitizers=full \
            -Ddefault_library=static -Dtests=false -Doptimization=1 \
            -Ddebug=true "-Dc_args=['-fno-omit-frame-pointer', '$coverage']" \
            "-Dc_link_args=$probe_runtime" \
            -Dmetal=disabled -Dvulkan=disabled -Dwebp=enabled \
            -Dpng=disabled -Djpeg=disabled -Dtiff=disabled -Dlcms2=disabled
        meson compile -C builddir -j "$jobs" "$@"
        ;;
    *)
        echo "Unknown build system: $buildtool" >&2
        exit 2
        ;;
esac
