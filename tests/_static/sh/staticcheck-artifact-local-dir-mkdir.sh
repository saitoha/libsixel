#!/bin/sh
# Emit TAP for ARTIFACT_LOCAL_DIR directory bootstrap policy.
#
# Artifact directories belong to the test harness, not individual TAP files.
# Keeping directory creation in both runners avoids repeated filesystem setup
# in tests and gives every test the same isolation contract.

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
' "$runner" || {
        echo "# runner does not create ARTIFACT_LOCAL_DIR: $runner"
        failed=1
    }
done

if test "$failed" -ne 0; then
    echo "not ok 1 - harness owns ARTIFACT_LOCAL_DIR creation"
    exit 1
fi

echo "ok 1 - harness owns ARTIFACT_LOCAL_DIR creation"
