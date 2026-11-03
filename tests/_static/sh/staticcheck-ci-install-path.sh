#!/bin/sh
# Autoconf rebases relative INSTALL values for each generated Makefile.
# Policy: docs/testing/staticcheck.md
# Policy: docs/misc/platforms/cosmopolitan.md
# Coverage: COSMO-06
set -eux
echo '1..1'
set -v
awk '
function bare_install(text) {
    return text ~ /(^|[[:space:]])INSTALL=["\047]?[[:alpha:]_][[:alnum:]_.+-]*(["\047]|[[:space:]]|$)/
}
BEGIN {
    if (!bare_install("INSTALL=cosmoinstall") ||
        !bare_install("INSTALL=\"install -c\"") ||
        !bare_install("INSTALL=\047install\047") ||
        bare_install("INSTALL=\"$(command -v cosmoinstall)\"") ||
        bare_install("INSTALL=/usr/bin/install") ||
        bare_install("INSTALL=./build-aux/install-sh") ||
        bare_install("INSTALL=build-aux/install-sh")) {
        print "# installer path scanner controls failed"
        failed = 1
    }
}
/^[[:space:]]*#/ { next }
bare_install($0) {
    print "# " FILENAME ":" FNR ": resolve INSTALL to a path"
    failed = 1
}
END { exit failed }
' "$1/.github/workflows/ci.yml" \
    "$1/.github/actions/ci-steps/action.yml" || {
    echo 'not ok 1 - CI installers use paths across build directories'
    exit 0
}
echo 'ok 1 - CI installers use paths across build directories'
exit 0
