#!/bin/sh
# Registration has exactly one instance host.
# Policy: docs/programs/quicklook-extension.md
# Test-plan: docs/testing/companion-programs-coverage.md
set -eux

test "${SIXEL_INSTALLCHECK:-0}" = 1 || {
    echo "1..0 # SKIP installed observation requires installcheck"
    exit 0
}
test -z "${DESTDIR:-}${SIXEL_INSTALLCHECK_DESTDIR:-}" || {
    echo "1..0 # SKIP STAGING is not a system installation"
    exit 0
}
test "${CROSS_COMPILING:-no}" != yes || {
    echo "1..0 # SKIP unsupported cross-built installed observer"
    exit 0
}
case "${QUICKLOOK_HOST_SYSTEM:?}" in
    darwin*) ;;
    *) echo "1..0 # SKIP unsupported Quick Look platform"; exit 0 ;;
esac
test "${HAVE_QUICKLOOK_EXTENSION:?}" = 1 || {
    echo "1..0 # SKIP Quick Look extension explicitly disabled"
    exit 0
}
# No user observation is applicable without explicit participation. A zero
# plan is recognized as SKIP by both configured TAP drivers.
test -n "${QUICKLOOK_USER_CONTEXT:-}" || {
    echo "1..0 # SKIP system-only: no explicit participating user context"
    exit 0
}
echo "1..1"
set -v

status=0
"${QUICKLOOK_OBSERVER_PATH:-${TOP_BUILDDIR}/quicklook-extension/sixel-quicklook-package}" installed \
    'duplicates:host' "${QUICKLOOK_EXPECTED_DIR:-${TOP_BUILDDIR}/quicklook-extension/package}" \
    "${QUICKLOOK_USER_CONTEXT:-}" >&2 || status=$?
test "$status" = 0 || {
    echo "not ok 1 - Registration has exactly one instance host"
    exit 0
}
echo "ok 1 - Registration has exactly one instance host"
exit 0
