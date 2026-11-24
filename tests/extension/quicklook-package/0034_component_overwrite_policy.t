#!/bin/sh
# Components use whole-bundle upgrade without conflicting overwrite policies.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP requires quicklook_extension=1"
    exit 0
}
echo "1..1"
set -v

"${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package" check component-upgrade-bundle \
    "${TOP_BUILDDIR}/quicklook-extension/package" || {
    printf '\n%s\n' "not ok 1 - Components use whole-bundle upgrade without conflicting overwrite policies."
    exit 0
}

printf '\n%s\n' "ok 1 - Components use whole-bundle upgrade without conflicting overwrite policies."
exit 0
