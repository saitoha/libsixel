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

ls "${EMSDK}"/upstream/emscripten/em* | \
grep -v -e \.ps1$ -e \.py$ -e \.txt | \
xargs ls -l

# MSYS/Cygwin shells ship Emscripten helper entrypoints only as .bat files,
# so create small shell wrappers that forward to the .bat via `cmd //c`.
UNAME_LOWER=$(uname -s | tr '[:upper:]' '[:lower:]')
if echo "${UNAME_LOWER}" | grep -Eq 'msys|mingw|cygwin'; then
  EMSCRIPTEN_BINDIR="${EMSDK}/upstream/emscripten"
  EMSCRIPTEN_WRAPPERDIR="${BUILDDIR}/emscripten-msys-wrappers"
  mkdir -p "${EMSCRIPTEN_WRAPPERDIR}"

  # These cover the autotools flow: configure, make, archive, strip.
  for tool in emconfigure emmake emcc emar emranlib emstrip; do
    bat_path="${EMSCRIPTEN_BINDIR}/${tool}.bat"
    wrapper_path="${EMSCRIPTEN_WRAPPERDIR}/${tool}"

    if [ -f "${bat_path}" ]; then
      cat > "${wrapper_path}" <<EOF
#!/bin/sh
cmd //c "${bat_path}" "\$@"
EOF
      chmod +x "${wrapper_path}"
    fi
  done

  PATH="${EMSCRIPTEN_WRAPPERDIR}:${PATH}"
  export PATH
  AR=${AR:-emar}
  RANLIB=${RANLIB:-emranlib}
  STRIP=${STRIP:-emstrip}
  export AR RANLIB STRIP
  CONFIG_SITE=/dev/null
  export CONFIG_SITE
fi

cd "${BUILDDIR}" && (
emconfigure sh ${TOP_SRCDIR}/configure \
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
