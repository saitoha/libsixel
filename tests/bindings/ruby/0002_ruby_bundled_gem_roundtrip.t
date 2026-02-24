#!/bin/sh
# TAP test verifying bundled Ruby gem installation and runtime loading.

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

gem install --install-dir "${ARTIFACT_LOCAL_DIR}/gem-home" \
    --bindir "${ARTIFACT_LOCAL_DIR}/gem-bin" \
    --no-document "$1" >/dev/null || {
    fail 1 'gem install failed'
    exit 0
}

GEM_HOME="${ARTIFACT_LOCAL_DIR}/gem-home" \
GEM_PATH="${ARTIFACT_LOCAL_DIR}/gem-home" \
runtime_exec ruby -e "require 'libsixel'; Libsixel.set_threads(1)" || {
    fail 1 'bundled ruby gem runtime check failed'
    exit 0
}

pass 1 'bundled ruby gem roundtrip succeeded'
exit 0
