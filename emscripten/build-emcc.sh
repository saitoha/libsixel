#!/bin/sh

set -euovx

SCRIPTDIR=$(cd $(dirname "${0}") && pwd)
EMSDK=${SCRIPTDIR}/emsdk
TOP_SRCDIR=$(cd "${SCRIPTDIR}"/.. && pwd "${SCRIPTDIR}")
BUILDDIR="${SCRIPTDIR}"/build
mkdir -p "${BUILDDIR}"
SHEBANG_FILE="${BUILDDIR}"/emscripten-node-shebang
printf '#!/usr/bin/env node\n' > "${SHEBANG_FILE}"

if ! which emcc; then
  if [ ! -d "${EMSDK}/.git" ]; then
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git "${EMSDK}"
  fi
  [ -f "${EMSDK}"/emsdk_env.sh ] || exit 1
  sh "${EMSDK}"/emsdk install latest
  sh "${EMSDK}"/emsdk activate latest
  SAVED_PWD=$(pwd)
  cd "${EMSDK}"
  . ./emsdk_env.sh
  cd "${SAVED_PWD}"
elif [ -f "${EMSDK}"/emsdk_env.sh ]; then
  SAVED_PWD=$(pwd)
  cd "${EMSDK}"
  . ./emsdk_env.sh
  cd "${SAVED_PWD}"
  which emcc
fi

cd "${BUILDDIR}" && (
sh ${TOP_SRCDIR}/configure \
  --host=wasm32-unknown-emscripten \
  --enable-amalgamation \
  --enable-amalgamated-tools \
  --disable-shared \
  --enable-static \
  --with-shebang-file="${SHEBANG_FILE}" \
  --without-png \
  --without-jpeg \
  --without-libcurl \
  --without-winhttp \
  --without-onnxruntime \
  --disable-wiccodec \
  --disable-appkit \
  --without-coregraphics \
  --disable-quicklook-extension \
  --disable-quicklook-preview \
  --disable-thumbnailer-command \
  --disable-abort-trace \
  CC=emcc \
  CFLAGS="-O3" \
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
make -j
make check
)
