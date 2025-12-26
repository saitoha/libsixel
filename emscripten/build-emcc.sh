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
emconfigure ${TOP_SRCDIR}/configure \
  --host=wasm32-unknown-emscripten \
  --disable-shared \
  --with-shebang-file="${SHEBANG_FILE}" \
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
           -flto \
  "
emmake make -j
emmake make check -j
)
