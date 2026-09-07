#!/bin/sh
# Emit TAP for test-owned ARTIFACT_LOCAL_DIR directory bootstrap policy.
# Policy: docs/testing/guide.md
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
    if (seen) {
        inspect_previous()
    }
    seen = 1
    created = 0
    skip_before_create = 0
    plan_before_create = 0
    verbose_before_create = 0
    file = FILENAME
}
/exit 0/ && !created {
    skip_before_create = 1
}
/^echo "1\.\.1"$/ && !created {
    plan_before_create = 1
}
/^set -v$/ && !created {
    verbose_before_create = 1
}
/test -d "\$\{ARTIFACT_LOCAL_DIR\}" \|\| mkdir -p "\$\{ARTIFACT_LOCAL_DIR\}"/ {
    if (!skip_before_create) {
        print "# test creates artifacts before its feature skip: " file
        failed = 1
    }
    if (!plan_before_create || !verbose_before_create) {
        print "# test creates artifacts before TAP setup: " file
        failed = 1
    }
    created = 1
}
/#[[:space:]]*SKIP/ && created {
    print "# test contains a feature skip after artifact creation: " file
    failed = 1
}
function inspect_previous() {
    if (!created) {
        print "# test does not create ARTIFACT_LOCAL_DIR lazily: " file
        failed = 1
    }
}
END {
    if (seen) {
        inspect_previous()
    }
    exit failed ? 1 : 0
}
' "$tests_root"/cli/options/regression/*_image_regression.t || failed=1

test "$failed" -eq 0 || {
    echo "not ok 1 - tests own lazy ARTIFACT_LOCAL_DIR creation"
    exit 0
}

echo "ok 1 - tests own lazy ARTIFACT_LOCAL_DIR creation"
exit 0
