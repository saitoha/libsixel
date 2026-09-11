#!/bin/sh
# Build a committed source snapshot and reproduce the native-memory study.
set -eu
TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
PATH="${TOP_SRCDIR}/.local/bin:${PATH}"
export PATH
PYTHON=${PYTHON-python3}
CC=${CC-cc}
REVISION=${REVISION-$(git -C "${TOP_SRCDIR}" rev-parse HEAD)}
study_dir=${STUDY_DIR-$(mktemp -d "${TMPDIR-/tmp}/sixel-format.XXXXXX")}
output_dir=${1-${TOP_SRCDIR}/docs/sixel-format-figures/comparison}
mkdir -p "${study_dir}/source" "${output_dir}"
study_dir=$(CDPATH='' cd -- "${study_dir}" && pwd -P)
output_dir=$(CDPATH='' cd -- "${output_dir}" && pwd -P)
git -C "${TOP_SRCDIR}" archive "${REVISION}" | tar -x -C "${study_dir}/source"
cd "${study_dir}/source"
./configure --without-libcurl --without-png --without-jpeg \
    --without-librsvg --without-tiff --without-webp --without-quicklook \
    --without-coregraphics --without-lcms2 --disable-quicklook-extension \
    CFLAGS=-O3 BASH=/bin/sh
make -j "${BUILD_JOBS-8}"
case $(uname -s) in
    Darwin) library="${study_dir}/source/src/.libs/libsixel.1.dylib" ;;
    *) library="${study_dir}/source/src/.libs/libsixel.so" ;;
esac
# pkg-config intentionally supplies separate compiler/linker arguments.
# shellcheck disable=SC2046
"${CC}" -O3 -std=c99 -Wall -Wextra -shared -fPIC \
    -I"${study_dir}/source/include" $(pkg-config --cflags libwebp) \
    "${TOP_SRCDIR}/tools/format-comparison/codecs.c" "${library}" \
    $(pkg-config --libs libwebp) \
    -Wl,-rpath,"${study_dir}/source/src/.libs" \
    -o "${study_dir}/codecs.so"
case $(uname -s) in
    Darwin)
        # A dylib's absolute install name overrides -rpath. Bind the adapter
        # to the exact archived build, never /usr/local's installed libsixel.
        install_name_tool -change /usr/local/lib/libsixel.1.dylib \
            "${library}" "${study_dir}/codecs.so"
        ;;
esac
"${PYTHON}" "${TOP_SRCDIR}/tools/format-comparison/measure.py" \
    --source "${study_dir}/source" --library "${library}" \
    --adapter "${study_dir}/codecs.so" --revision "${REVISION}" \
    --output "${output_dir}"
"${PYTHON}" "${TOP_SRCDIR}/tools/format-comparison/check.py" "${output_dir}"
"${PYTHON}" "${TOP_SRCDIR}/tools/format-comparison/plot.py" "${output_dir}"
