#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Check the dithering tutorial programs.
#
#   ./check.sh [DIR]    DIR holds step1 ... step10 and dtool (default: here)
#
# 1. Every step passes its self test.
# 2. The same input always gives the same output.
# 3. Step 4's fs kernel is exactly step 3, so its table can be trusted.
# 4. Serpentine order changes the picture, and the other kernels do too.
# 5. Every shipped palette loads, and dithering with it keeps the tone.
# 6. Step 7 keeps the light of an area, and so differs from step 3.
# 7. Step 8's limits change the picture, and a deadzone leaves a flat area flat.
# 8. Step 9's exact lookups are exactly step 3.
# 9. Step 10 does not depend on the number of threads, and one band is step 3.

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

steps="1 2 3 4 7 8 9 10"
for s in $steps; do
    "$bin/step$s" selftest >/dev/null || fail "step$s selftest"
done

"$bin/dtool" gradient 200 40 >"$work/gradient.ppm" || fail "gradient"
"$bin/dtool" cube 3 >"$work/cube27.ppm" || fail "cube"
cp "$images/snake.ppm" "$work/snake.ppm"

for picture in gradient snake; do
    in="$work/$picture.ppm"
    for s in $steps; do
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

    "$bin/step7" "$in" "$work/cube27.ppm" >"$work/k.ppm" ||
        fail "step7 on $picture"
    cmp -s "$work/fs3.ppm" "$work/k.ppm" &&
        fail "step7 looks the same as step3 on $picture"

    "$bin/step8" "$in" "$work/cube27.ppm" clamp=hard >"$work/hard.ppm"
    "$bin/step8" "$in" "$work/cube27.ppm" clamp=soft >"$work/soft.ppm"
    cmp -s "$work/hard.ppm" "$work/soft.ppm" &&
        fail "step8 clamp=soft looks the same as clamp=hard on $picture"
    "$bin/step8" "$in" "$work/cube27.ppm" jajuni,serpentine,deadzone=5 \
        >/dev/null || fail "step8 with several options on $picture"

    for palette in "$work/cube27.ppm" "$here/palettes/snake16.ppm" \
                   "$here/palettes/snake256.ppm"; do
        "$bin/step3" "$in" "$palette" >"$work/fs3.ppm"
        for method in scan cache grid; do
            "$bin/step9" "$in" "$palette" "$method" >"$work/k.ppm" ||
                fail "step9 $method on $picture"
            cmp -s "$work/fs3.ppm" "$work/k.ppm" ||
                fail "step9 $method differs from step3 on $picture"
        done
        "$bin/step9" "$in" "$palette" memo >/dev/null ||
            fail "step9 memo on $picture"
    done

    "$bin/step3" "$in" "$work/cube27.ppm" >"$work/fs3.ppm"
    "$bin/step2" "$in" "$work/cube27.ppm" >"$work/bayer2.ppm"
    for t in 1 2 3 16; do
        TUTORIAL_THREADS=$t "$bin/step10" "$in" "$work/cube27.ppm" bands=1 \
            >"$work/k.ppm" || fail "step10 one band on $picture"
        cmp -s "$work/fs3.ppm" "$work/k.ppm" ||
            fail "step10 with one band differs from step3 on $t threads"
        TUTORIAL_THREADS=$t "$bin/step10" "$in" "$work/cube27.ppm" \
            bayer,bands=7 >"$work/k.ppm" || fail "step10 bayer on $picture"
        cmp -s "$work/bayer2.ppm" "$work/k.ppm" ||
            fail "step10 bayer differs from step2 on $t threads"
        TUTORIAL_THREADS=$t "$bin/step10" "$in" "$work/cube27.ppm" \
            bands=4,overlap=6 >"$work/t$t.ppm" ||
            fail "step10 bands on $picture"
        cmp -s "$work/t1.ppm" "$work/t$t.ppm" ||
            fail "step10 depends on the number of threads: $t on $picture"
    done
    cmp -s "$work/fs3.ppm" "$work/t1.ppm" &&
        fail "step10 with four bands looks the same as step3 on $picture"
done

# With a deadzone, a flat area close to a palette color stays that color.
"$bin/dtool" flat 64 64 100 100 100 >"$work/flat.ppm"
"$bin/dtool" gray 16 >"$work/gray16.ppm"
"$bin/step1" "$work/flat.ppm" "$work/gray16.ppm" >"$work/a.ppm"
"$bin/step8" "$work/flat.ppm" "$work/gray16.ppm" deadzone=5 >"$work/b.ppm"
cmp -s "$work/a.ppm" "$work/b.ppm" ||
    fail "step8 deadzone=5 does not leave a flat area flat"

for option in nosuch fs,nosuch; do
    "$bin/step4" "$work/gradient.ppm" "$work/cube27.ppm" "$option" \
        >/dev/null 2>&1 && fail "step4 accepted the option $option"
done
"$bin/step2" "$work/gradient.ppm" "$work/cube27.ppm" nosuch >/dev/null 2>&1 &&
    fail "step2 accepted an unknown option"
for spec in "7 fs" "8 clamp=nosuch" "8 deadzone=x" "8 fs,nosuch" "9 nosuch" \
            "9 grid,nosuch" "10 nosuch" "10 bands=0" "10 overlap=-1"; do
    # shellcheck disable=SC2086 # split the spec into arguments
    set -- $spec
    "$bin/step$1" "$work/gradient.ppm" "$work/cube27.ppm" "$2" \
        >/dev/null 2>&1 && fail "step$1 accepted the option $2"
done

for palette in "$here"/palettes/*.ppm; do
    "$bin/step3" "$work/snake.ppm" "$palette" >"$work/p.ppm" ||
        fail "dither with $(basename "$palette")"
done

if [ "$failures" -ne 0 ]; then
    echo "$failures check(s) failed"
    exit 1
fi
echo "all checks passed"
