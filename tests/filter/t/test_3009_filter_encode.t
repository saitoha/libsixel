#!/bin/sh

# Build and run the encode filter unit tests using the in-tree libtool archive.
# The TAP output comes from the test binary itself.

set -eu

if [ -z "${TOP_SRCDIR:-}" ] || [ -z "${TOP_BUILDDIR:-}" ]; then
    echo "Bail out! missing TOP_SRCDIR or TOP_BUILDDIR" 1>&2
    exit 1
fi

CC_BIN="${CC:-cc}"
LIBTOOL_BIN="${TOP_BUILDDIR}/libtool"
SRC="${TOP_SRCDIR}/tests/filter/filter_encode_tests.c"
OBJ="${ARTIFACT_ROOT:-.}/filter_encode_tests.lo"
BIN="${ARTIFACT_ROOT:-.}/filter_encode_tests"

mkdir -p "${ARTIFACT_ROOT:-.}"

"${LIBTOOL_BIN}" --mode=compile --tag=CC "${CC_BIN}" \
    -I"${TOP_BUILDDIR}" \
    -I"${TOP_BUILDDIR}/include" \
    -I"${TOP_SRCDIR}/include" \
    -I"${TOP_SRCDIR}/src" \
    -I"${TOP_SRCDIR}/tests/filter" \
    -c "${SRC}" -o "${OBJ}"

"${LIBTOOL_BIN}" --mode=link --tag=CC "${CC_BIN}" \
    -o "${BIN}" "${OBJ}" "${TOP_BUILDDIR}/src/libsixel.la"

exec "${BIN}"
