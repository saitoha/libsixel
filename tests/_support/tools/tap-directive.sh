#!/bin/sh
# Exercise one maintained driver parser against a tracked TAP specimen.
set -eu
mkdir -p "$ARTIFACT_LOCAL_DIR"
# The outer harness bounds this parser test. DragonFlyBSD timeout can sleep
# until its alarm if cat exits before timeout starts waiting for SIGCHLD.
sed 's/^@OPENVMS_GNV_TRUE@/#/' \
    "$TOP_SRCDIR/build-aux/lso-tap-driver.sh.in" |
    SIXEL_TEST_SHELL=sh reduce_fork=$1 sh -s -- \
        --test-name directive --log-file "$ARTIFACT_LOCAL_DIR/driver.log" \
        --trs-file "$ARTIFACT_LOCAL_DIR/driver.trs" --timeout 0 \
        -- cat "$TOP_SRCDIR/tests/tools/tap/data/$2.txt" >/dev/null

# Both outputs are observations: metadata must classify exactly one point,
# and the diagnostic log must preserve the specimen's original TAP point.
# shellcheck disable=SC2016
"${AM_TAP_AWK:-awk}" -v expected="$3" '
    FNR == NR && FNR == 2 { point=$0; next }
    /^:test-result:/ { count++; valid += $2 == expected }
    /^:global-test-result:/ { global += $2 == expected }
    $0 == point { output++ }
    END { exit count != 1 || valid != 1 || global != 1 || output != 1 }
' "$TOP_SRCDIR/tests/tools/tap/data/$2.txt" \
    "$ARTIFACT_LOCAL_DIR/driver.trs" "$ARTIFACT_LOCAL_DIR/driver.log"
