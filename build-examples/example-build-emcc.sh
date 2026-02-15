#!/bin/sh

set -euovx

SCRIPTDIR=$(cd $(dirname "${0}") && pwd)
if which cygpath; then
  SCRIPTDIR=$(cygpath -u "${SCRIPTDIR}")
fi
EMSDK=${SCRIPTDIR}/emsdk
BUILDDIR="${SCRIPTDIR}"/build-emcc
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

# MSYS/Cygwin shells ship Emscripten helper entrypoints only as .bat files,
# so create small shell wrappers that forward to the .bat via `cmd //c`.
UNAME_LOWER=$(uname -s | tr '[:upper:]' '[:lower:]')
if echo "${UNAME_LOWER}" | grep -Eq 'msys|mingw|cygwin'; then
  EMSCRIPTEN_BINDIR="${EMSDK}/upstream/emscripten"
  EMSCRIPTEN_WRAPPERDIR="${BUILDDIR}/emscripten-msys-wrappers"
  mkdir -p "${EMSCRIPTEN_WRAPPERDIR}"

  # These cover the autotools flow: configure, make, archive, strip.
  for tool in emconfigure emmake emcc emar emranlib emstrip emnm; do
    bat_path="${EMSCRIPTEN_BINDIR}/${tool}.bat"
    wrapper_path="${EMSCRIPTEN_WRAPPERDIR}/${tool}"
    if [ -f "${bat_path}" ]; then
      chmod +x "${bat_path}"
      cat > "${wrapper_path}" <<EOF
#!/bin/sh
cmd //c "${bat_path}" "\$@"
EOF
      chmod +x "${wrapper_path}"
    fi
  done

  PATH="${EMSCRIPTEN_WRAPPERDIR}:${PATH}"
  export PATH
fi
 
cd "${BUILDDIR}" && (
CC=emcc \
../../configure \
  --host=wasm32-unknown-emscripten \
  --disable-shared \
  --with-shebang-file="${SHEBANG_FILE}" \
  --disable-dependency-tracking
make all
make check
)
