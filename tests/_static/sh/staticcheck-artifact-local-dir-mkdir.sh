#!/bin/sh
# Emit TAP for test-owned ARTIFACT_LOCAL_DIR directory bootstrap policy.
#
# Runners export an isolated path but do not create it. A TAP test that needs
# artifacts creates its directory lazily after any early feature skips.

set -eux

src_root=$1
tests_root=$src_root/tests
autotools_driver=$src_root/build-aux/lso-tap-driver.sh.in
meson_driver=$tests_root/meson-python-tap-wrapper.sh

echo "1..1"
set -v

failed=0

for runner in "$autotools_driver" "$meson_driver"; do
    test -f "$runner" || {
        echo "# missing artifact directory runner: $runner"
        failed=1
        continue
    }
    awk '
/mkdir[[:space:]]+-p/ && /ARTIFACT_LOCAL_DIR/ { found = 1 }
END { exit found ? 0 : 1 }
' "$runner" && {
        echo "# runner creates ARTIFACT_LOCAL_DIR: $runner"
        failed=1
    }
done

awk '
FNR == 1 {
    if (seen && !created) {
        print "# test does not create ARTIFACT_LOCAL_DIR lazily: " file
        failed = 1
    }
    seen = 1
    created = 0
    file = FILENAME
}
/test -d "\$\{ARTIFACT_LOCAL_DIR\}" \|\| mkdir -p "\$\{ARTIFACT_LOCAL_DIR\}"/ {
    created = 1
}
END {
    if (seen && !created) {
        print "# test does not create ARTIFACT_LOCAL_DIR lazily: " file
        failed = 1
    }
    exit failed ? 1 : 0
}
' "$tests_root"/cli/options/regression/*.t || failed=1

test "$failed" -eq 0 || {
    echo "not ok 1 - tests own lazy ARTIFACT_LOCAL_DIR creation"
    exit 0
}

echo "ok 1 - tests own lazy ARTIFACT_LOCAL_DIR creation"
exit 0
