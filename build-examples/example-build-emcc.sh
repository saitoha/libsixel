#!/bin/sh

set -eu

scriptdir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
os=$(uname -s)
case "$os" in
  CYGWIN*|MINGW*|MSYS*)
    # Run the example through native cmd without rewriting its switches.
    cd "$scriptdir"
    MSYS2_ARG_CONV_EXCL='*' exec cmd.exe /d /c example-build-emcc.cmd
    ;;
esac

srcdir=$(CDPATH='' cd "$scriptdir/.." && pwd)
if test -f "$srcdir/config.status"; then
  printf '%s\n' 'Use an unconfigured source tree for an out-of-tree Autotools build.' >&2
  exit 1
fi
PATH="$srcdir/.local/bin:$PATH"
export PATH
builddir=${SIXEL_EMCC_BUILD_DIR:-"$scriptdir/build-emcc"}
sdk_version=${EMSDK_VERSION:-7.0.0}

if test -n "${SIXEL_EMSDK_DIR:-}" || ! command -v emcc >/dev/null 2>&1; then
  sdkdir=${SIXEL_EMSDK_DIR:-"$scriptdir/emsdk-$os-$(uname -m)"}
  if test ! -f "$sdkdir/emsdk_env.sh"; then
    git clone --depth 1 --branch "$sdk_version" \
      https://github.com/emscripten-core/emsdk.git "$sdkdir"
  fi
  # Match the native Windows example and use the system curl when available.
  if command -v curl >/dev/null 2>&1; then
    export EMSDK_USE_CURL=1
  fi
  "$sdkdir/emsdk" install "$sdk_version"
  "$sdkdir/emsdk" activate "$sdk_version"
  # The SDK's environment script resolves its location from the working
  # directory in POSIX shells that do not expose the sourced filename.
  saved_pwd=$PWD
  cd "$sdkdir"
  # shellcheck source=/dev/null
  . ./emsdk_env.sh
  cd "$saved_pwd"
fi

# Autotools probes the LLVM utilities in addition to the Emscripten drivers.
if test -n "${EMSDK:-}"; then
  PATH="$EMSDK/upstream/bin:$PATH"
  export PATH
fi

case ${SIXEL_EMCC_CHECK:-0} in
  0) test_option=--disable-tests ;;
  1) test_option=--enable-tests ;;
  *) printf '%s\n' 'SIXEL_EMCC_CHECK must be 0 or 1' >&2; exit 2 ;;
esac

mkdir -p "$builddir"
cd "$builddir"
printf '#!/usr/bin/env node\n' > emscripten-node-shebang

# Keep pkg-config probes in the SDK sysroot instead of the build machine.
CFLAGS="${CFLAGS:--O1 -g}" emconfigure "$srcdir/configure" \
  --host=wasm32-unknown-emscripten \
  --with-shebang-file="$PWD/emscripten-node-shebang" \
  --disable-dependency-tracking \
  --disable-shared \
  --with-png \
  --with-jpeg \
  --without-tiff \
  --without-webp \
  --without-librsvg \
  --without-libfetch \
  "$test_option"
"${MAKE:-make}" all

converters/img2sixel -o example.six "$srcdir/images/snake.png"
converters/sixel2png -i example.six -o example.png
assessment/lsqa -m ms-ssim -b ms-ssim:min=0.98 \
  "$srcdir/images/snake.png" example.png

if test "$test_option" = --enable-tests; then
  "${MAKE:-make}" check
fi
