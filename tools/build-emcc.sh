#!/bin/sh

set -euovx

EMSDK=$(cd $(dirname "${0}")/emsdk && pwd)
TOP_SRCDIR=$(cd $(dirname "${0}")/.. && pwd)
BUILDDIR=$(mkdir -p "${TOP_SRCDIR}"/build-emcc && cd "${TOP_SRCDIR}"/build-emcc && pwd)

if [ ! -d "${EMSDK}/.git" ]; then
  git clone --depth 1 https://github.com/emscripten-core/emsdk.git "${EMSDK}"
fi
[ -f "${EMSDK}"/emsdk_env.sh ] || exit 1
cd "${EMSDK}" && pwd
cd "${EMSDK}" && ./emsdk install latest
cd "${EMSDK}" && ./emsdk activate latest

source "${EMSDK}"/emsdk_env.sh

emcc --show-ports

exit 0

cd "${BUILDDIR}" && (
sh ${TOP_SRCDIR}/configure \
  --disable-simd \
  --disable-shared \
  --enable-static \
  --without-png \
  --without-jpeg \
  --without-libcurl \
  --without-onnxruntime \
  --without-wiccodec \
  --disable-appkit \
  --without-coregraphics \
  --disable-quicklook-extension \
  --disable-quicklook-preview \
  --enable-amalgamation \
  --enable-amalgamated-tools \
  --host=wasm32-unknown-emscripten \
  CC=emcc \
  CFLAGS=-O3 \
  LDFLAGS="-sWASM_BIGINT=1 \
           -sSINGLE_FILE=1 \
           -sENVIRONMENT=node \
           -sABORTING_MALLOC=0 \
           -sNODERAWFS=1 \
           -sFORCE_FILESYSTEM=1 \
           -sALLOW_MEMORY_GROWTH=1 \
           -sINITIAL_MEMORY=67108864 \
           -sSTACK_SIZE=2097152 \
  "
make -C"${BUILDDIR}" -j
)
