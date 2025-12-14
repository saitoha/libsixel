#!/bin/sh

# Build and run the VPTE filter unit tests using the in-tree libtool archive.
# The TAP output comes from the test binary itself.

set -eu

if [ "${CROSS_COMPILING:-no}" = "yes" ]; then
    echo "1..0 # SKIP cross compiling"
    exit 0
fi

if [ -z "${TOP_SRCDIR:-}" ] || [ -z "${TOP_BUILDDIR:-}" ]; then
    echo "Bail out! missing TOP_SRCDIR or TOP_BUILDDIR" 1>&2
    exit 1
fi

CC_BIN="${CC:-cc}"
LIBTOOL_BIN="${TOP_BUILDDIR}/libtool"
# Honor build-time flags (coverage, warnings) when compiling the
# filter test binaries so Windows and gcov builds link correctly.
COVERAGE_EXTRA="${FILTER_TEST_COVERAGE:-}"
CFLAGS_EXTRA="${FILTER_TEST_CFLAGS:-} ${COVERAGE_EXTRA}"
CPPFLAGS_EXTRA="${FILTER_TEST_CPPFLAGS:-}"
LDFLAGS_EXTRA="${FILTER_TEST_LDFLAGS:-} ${COVERAGE_EXTRA}"
LIBS_EXTRA="${FILTER_TEST_LIBS:-} ${COVERAGE_EXTRA}"
SRC="${TOP_SRCDIR}/tests/filter/filter_vpte_tests.c"
OBJ="${ARTIFACT_ROOT:-.}/filter_vpte_tests.lo"
BIN="${ARTIFACT_ROOT:-.}/filter_vpte_tests"

mkdir -p "${ARTIFACT_ROOT:-.}"

"${LIBTOOL_BIN}" --mode=compile --tag=CC "${CC_BIN}" \
    ${CPPFLAGS_EXTRA} ${CFLAGS_EXTRA} \
    -I"${TOP_BUILDDIR}" \
    -I"${TOP_BUILDDIR}/include" \
    -I"${TOP_SRCDIR}/include" \
    -I"${TOP_SRCDIR}/src" \
    -I"${TOP_SRCDIR}/tests/filter" \
    -c "${SRC}" -o "${OBJ}"

"${LIBTOOL_BIN}" --mode=link --tag=CC "${CC_BIN}" \
    ${LDFLAGS_EXTRA} \
    -o "${BIN}" "${OBJ}" "${TOP_BUILDDIR}/src/libsixel.la" \
    ${LIBS_EXTRA}

exec "${BIN}"
