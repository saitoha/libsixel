#!/bin/sh

set -euovx

SCRIPTDIR=$(cd $(dirname "${0}") && pwd)
if which cygpath; then
  SCRIPTDIR=$(cygpath -u "${SCRIPTDIR}")
fi
BUILDDIR="${SCRIPTDIR}"/build-cosmopolitan
mkdir -p "${BUILDDIR}"

if ! which cosmocc; then
  if test ! -f cosmocc.zip; then
    curl -O https://cosmo.zip/pub/cosmocc/cosmocc.zip
  fi
  unzip -d cosmopolitan -o cosmocc.zip
  chmod +x cosmopolitan/bin/cosmo*
  PATH=$PWD/cosmopolitan/bin:$PATH
  export PATH
fi

 
cd "${BUILDDIR}" && (
    CC=cosmocc \
    AR=cosmoar \
    RANLIB=cosmoranlib \
    INSTALL=cosmoinstall \
  ../../configure \
    --enable-simd \
    --disable-shared \
    --enable-static \
    --without-png \
    --without-jpeg \
    --without-libcurl \
    --disable-wiccodec \
    --disable-appkit \
    --without-coregraphics \
    --disable-quicklook-extension \
    --disable-quicklook-preview
  make check -j4
)
