#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Check the dithering tutorial programs.
#
#   ./check.sh [DIR]    DIR holds step1 ... step4 and dtool (default: here)
#
# 1. Every step passes its self test.
# 2. The same input always gives the same output.
# 3. Step 4's fs kernel is exactly step 3, so its table can be trusted.
# 4. Serpentine order changes the picture, and the other kernels do too.
# 5. Every shipped palette loads, and dithering with it keeps the tone.

set -u

here=$(cd "$(dirname "$0")" && pwd)
bin=$(cd "${1:-$here}" && pwd)
images="$here/../../../../images"
work=$(mktemp -d "${TMPDIR:-/tmp}/dither-tutorial.XXXXXX") || exit 1
trap 'rm -rf "$work"' EXIT INT TERM
failures=0

fail() {
    echo "FAIL: $*"
    failures=$((failures + 1))
}

for s in 1 2 3 4; do
    "$bin/step$s" selftest >/dev/null || fail "step$s selftest"
done

"$bin/dtool" gradient 200 40 >"$work/gradient.ppm" || fail "gradient"
"$bin/dtool" cube 3 >"$work/cube27.ppm" || fail "cube"
cp "$images/snake.ppm" "$work/snake.ppm"

for picture in gradient snake; do
    in="$work/$picture.ppm"
    for s in 1 2 3 4; do
        "$bin/step$s" "$in" "$work/cube27.ppm" >"$work/a.ppm" ||
            fail "step$s on $picture"
        "$bin/step$s" "$in" "$work/cube27.ppm" >"$work/b.ppm"
        cmp -s "$work/a.ppm" "$work/b.ppm" ||
            fail "step$s is not repeatable on $picture"
    done

    "$bin/step3" "$in" "$work/cube27.ppm" >"$work/fs3.ppm"
    "$bin/step4" "$in" "$work/cube27.ppm" fs >"$work/fs4.ppm"
    cmp -s "$work/fs3.ppm" "$work/fs4.ppm" ||
        fail "step4 fs differs from step3 on $picture"

    for option in serpentine atkinson jajuni stucki sierra \
                  atkinson,serpentine; do
        "$bin/step4" "$in" "$work/cube27.ppm" "$option" >"$work/k.ppm" ||
            fail "step4 $option on $picture"
        cmp -s "$work/fs4.ppm" "$work/k.ppm" &&
            fail "step4 $option looks the same as fs on $picture"
    done
done

for option in nosuch fs,nosuch; do
    "$bin/step4" "$work/gradient.ppm" "$work/cube27.ppm" "$option" \
        >/dev/null 2>&1 && fail "step4 accepted the option $option"
done
"$bin/step2" "$work/gradient.ppm" "$work/cube27.ppm" nosuch >/dev/null 2>&1 &&
    fail "step2 accepted an unknown option"

for palette in "$here"/palettes/*.ppm; do
    "$bin/step3" "$work/snake.ppm" "$palette" >"$work/p.ppm" ||
        fail "dither with $(basename "$palette")"
done

if [ "$failures" -ne 0 ]; then
    echo "$failures check(s) failed"
    exit 1
fi
echo "all checks passed"
