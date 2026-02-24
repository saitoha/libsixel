#!/bin/sh
# TAP test verifying bundled Ruby gem build succeeds.

set -eux

command -v ruby >/dev/null 2>&1 || {
    printf '1..0 # SKIP ruby is unavailable\n'
    exit 0
}

command -v gem >/dev/null 2>&1 || {
    printf '1..0 # SKIP rubygems is unavailable\n'
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo '1..1'
set -v

runtime_exec ruby "${TOP_SRCDIR}/ruby/gem_builder.rb" \
    --libdir "${TOP_BUILDDIR}/src/.libs" \
    --distdir "${ARTIFACT_LOCAL_DIR}" || {
    fail 1 'bundled ruby gem build failed'
    exit 0
}

set -- "${ARTIFACT_LOCAL_DIR}"/libsixel-ruby-*.gem
test -f "$1" || {
    fail 1 'bundled ruby gem artifact is missing'
    exit 0
}

pass 1 'bundled ruby gem build succeeded'
exit 0
