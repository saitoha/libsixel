#!/bin/sh

# Run the prebuilt VPTE filter unit tests. The TAP output comes from the test
# binary itself.

set -eu

if [ "${CROSS_COMPILING:-no}" = "yes" ]; then
    echo "1..0 # SKIP cross compiling"
    exit 0
fi

if [ -z "${TOP_BUILDDIR:-}" ]; then
    echo "Bail out! missing TOP_BUILDDIR" 1>&2
    exit 1
fi

BIN="${TOP_BUILDDIR}/tests/filter/filter_vpte_tests"

if [ ! -x "${BIN}" ]; then
    echo "Bail out! missing test binary: ${BIN}" 1>&2
    exit 1
fi

exec "${BIN}"
