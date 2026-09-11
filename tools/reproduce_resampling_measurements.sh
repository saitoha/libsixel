#!/bin/sh
# Reproduce direct and end-to-end resampling documentation evidence.

set -eu

TOP_SRCDIR=${TOP_SRCDIR-${0%/*}/..}
TOP_SRCDIR=$(CDPATH='' cd -- "${TOP_SRCDIR}" && pwd -P)
BUILD_DIR=${BUILD_DIR-${TOP_SRCDIR}}
BUILD_DIR=$(CDPATH='' cd -- "${BUILD_DIR}" && pwd -P)
PYTHON=${PYTHON-python3}
MAKE=${MAKE-make}
GIT=${GIT-git}
IMG2SIXEL_PATH=${IMG2SIXEL_PATH-${BUILD_DIR}/converters/img2sixel}
SIXEL2PNG_PATH=${SIXEL2PNG_PATH-${BUILD_DIR}/converters/sixel2png}
LSQA_PATH=${LSQA_PATH-${BUILD_DIR}/assessment/lsqa}
LIBSIXEL_PATH=${LIBSIXEL_PATH-}

measurement_dir=${1-${TOP_SRCDIR}/docs/functionality/resampling/measurements}
input_image=${2-${TOP_SRCDIR}/images/snake.png}
input_image=$(CDPATH='' cd -- "${input_image%/*}" && pwd -P)/${input_image##*/}
input_label=${input_image}
warmups=${RESAMPLING_WARMUPS-2}
runs=${RESAMPLING_RUNS-9}
source_state=clean

case ${input_image} in
    "${TOP_SRCDIR}"/*) input_label=${input_image#"${TOP_SRCDIR}"/} ;;
esac
input_label=${RESAMPLING_INPUT_LABEL-${input_label}}

"${GIT}" -C "${TOP_SRCDIR}" diff --quiet -- || source_state=dirty
"${GIT}" -C "${TOP_SRCDIR}" diff --cached --quiet -- || source_state=dirty
test "${source_state}" = clean || {
    echo "refusing to record measurements from a dirty tracked worktree" >&2
    echo "commit the implementation before producing durable measurements" >&2
    exit 1
}

"${PYTHON}" -c 'import matplotlib, numpy, PIL' >/dev/null 2>&1 || {
    echo "resampling measurements require matplotlib, numpy, and Pillow" >&2
    exit 1
}

PATH="${TOP_SRCDIR}/.local/bin:${PATH}"
LC_ALL=C
TZ=UTC
SIXEL_THREADS=1
SIXEL_SIMD_LEVEL=scalar
export PATH LC_ALL TZ SIXEL_THREADS SIXEL_SIMD_LEVEL
cd "${TOP_SRCDIR}"

test -d "${measurement_dir}" || mkdir -p "${measurement_dir}"
"${MAKE}" -C "${BUILD_DIR}" all

test -n "${LIBSIXEL_PATH}" || {
    for candidate in \
        "${BUILD_DIR}/src/.libs/libsixel.so" \
        "${BUILD_DIR}/src/.libs/libsixel.dylib" \
        "${BUILD_DIR}/src/.libs/libsixel.1.dylib"
    do
        test -f "${candidate}" || continue
        LIBSIXEL_PATH=${candidate}
        break
    done
}
test -f "${LIBSIXEL_PATH}" || {
    echo "cannot find the built shared libsixel library" >&2
    echo "set LIBSIXEL_PATH to the exact shared-library path" >&2
    exit 1
}

revision=$("${GIT}" -C "${TOP_SRCDIR}" rev-parse HEAD)
compiler_command=$(sed -n 's/^CC = //p' "${BUILD_DIR}/Makefile")
cflags=$(sed -n 's/^CFLAGS = //p' "${BUILD_DIR}/Makefile")
cppflags=$(sed -n 's/^CPPFLAGS = //p' "${BUILD_DIR}/Makefile")
ldflags=$(sed -n 's/^LDFLAGS = //p' "${BUILD_DIR}/Makefile")
configure_arguments=$("${BUILD_DIR}/config.status" --config)

"${PYTHON}" "${TOP_SRCDIR}/tools/plot_resampling_measurements.py" \
    "${input_image}" \
    --input-label "${input_label}" \
    --libsixel "${LIBSIXEL_PATH}" \
    --img2sixel "${IMG2SIXEL_PATH}" \
    --sixel2png "${SIXEL2PNG_PATH}" \
    --lsqa "${LSQA_PATH}" \
    --revision "${revision}" \
    --source-state "${source_state}" \
    --compiler-command "${compiler_command}" \
    --cflags "${cflags}" \
    --cppflags "${cppflags}" \
    --ldflags "${ldflags}" \
    --configure-arguments "${configure_arguments}" \
    --warmups "${warmups}" \
    --runs "${runs}" \
    --output-directory "${measurement_dir}"

"${PYTHON}" "${TOP_SRCDIR}/tools/check_resampling_measurements.py" \
    "${measurement_dir}"
