#!/bin/sh
# Build a committed source snapshot and reproduce the native-memory study.
set -eu
TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
PATH="${TOP_SRCDIR}/.local/bin:${PATH}"
export PATH
PYTHON=${PYTHON-python3}
CC=${CC-cc}
# Record an immutable commit even when a branch name is supplied.
REVISION=$(git -C "${TOP_SRCDIR}" rev-parse --verify "${REVISION-HEAD}^{commit}")
# Directory of source photographs (*.png or *.jpg), for example the
# photo/crop-640x480 directory of the libsixel-dataset repository.
PHOTOS=${PHOTOS:?set PHOTOS to a directory of source photographs}
PHOTOS=$(CDPATH='' cd -- "${PHOTOS}" && pwd -P)
# Optional second corpus of rendered charts (*.png), for example the
# chart/crop-640x480 directory of the libsixel-dataset repository.
CHARTS=${CHARTS-}
if [ -n "${CHARTS}" ]; then
    CHARTS=$(CDPATH='' cd -- "${CHARTS}" && pwd -P)
fi
# CORPUS_STRIDE=N measures every Nth corpus image by name, for quicker
# development runs; published results use the default of 1.
# Optional directory of the uncropped originals (*.jpg) with the same names,
# for example photo/original; it enables the latency-against-size sweep.
SIZE_PHOTOS=${SIZE_PHOTOS-}
if [ -n "${SIZE_PHOTOS}" ]; then
    SIZE_PHOTOS=$(CDPATH='' cd -- "${SIZE_PHOTOS}" && pwd -P)
fi
study_dir=${STUDY_DIR-$(mktemp -d "${TMPDIR-/tmp}/sixel-format.XXXXXX")}
output_dir=${1-${TOP_SRCDIR}/docs/sixel-format/comparison/figures}
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
headerpad=
case $(uname -s) in
    Darwin)
        library="${study_dir}/source/src/.libs/libsixel.1.dylib"
        # Leave room for install_name_tool below to write the long absolute
        # path of the archived build.
        headerpad=-Wl,-headerpad_max_install_names
        ;;
    *) library="${study_dir}/source/src/.libs/libsixel.so" ;;
esac
# pkg-config intentionally supplies separate compiler/linker arguments.
# shellcheck disable=SC2046
"${CC}" -O3 -std=c99 -Wall -Wextra -shared -fPIC \
    -I"${study_dir}/source/include" $(pkg-config --cflags libwebp) \
    "${TOP_SRCDIR}/tools/format-comparison/codecs.c" "${library}" \
    $(pkg-config --libs libwebp) \
    -Wl,-rpath,"${study_dir}/source/src/.libs" ${headerpad} \
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
    --photos "${PHOTOS}" ${CHARTS:+"--charts=${CHARTS}"} \
    --corpus-stride "${CORPUS_STRIDE-1}" \
    ${SIZE_PHOTOS:+"--size-photos=${SIZE_PHOTOS}"} \
    --output "${output_dir}"
# The stage breakdown needs the originals and a stage-timed Pillow; see
# tools/format-comparison/build_stage_pillow.sh.
if [ -n "${SIZE_PHOTOS}" ] && [ -n "${STAGE_PILLOW_PYTHON-}" ]; then
    "${PYTHON}" "${TOP_SRCDIR}/tools/format-comparison/stages.py" \
        --source "${study_dir}/source" --library "${library}" \
        --adapter "${study_dir}/codecs.so" --revision "${REVISION}" \
        --photos "${SIZE_PHOTOS}" --pillow-python "${STAGE_PILLOW_PYTHON}" \
        --output "${output_dir}"
fi
"${PYTHON}" "${TOP_SRCDIR}/tools/format-comparison/check.py" "${output_dir}"
"${PYTHON}" "${TOP_SRCDIR}/tools/format-comparison/plot.py" "${output_dir}"
