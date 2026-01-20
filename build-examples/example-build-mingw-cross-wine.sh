#!/bin/sh

set -exv

: "${MAKE:=make}"

SCRIPTDIR=$(cd $(dirname "${0}") && pwd)
if which cygpath; then
  SCRIPTDIR=$(cygpath -u "${SCRIPTDIR}")
fi
BUILDDIR="${SCRIPTDIR}"/build-mingw-cross
mkdir -p "${BUILDDIR}"

cd "${BUILDDIR}" && {
    ../../configure \
        --host=x86_64-w64-mingw32 \
        --disable-shared
    MVK_CONFIG_LOG_LEVEL=0 WINEDEBUG=-all SIXEL_RUNTIME=wine SIXEL_BIN_EXT=.exe ${MAKE} check -j4
}
