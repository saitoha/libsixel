#!/bin/sh
# Verify explicit adaptive sampling palette does not depend on worker capacity.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

serial_palette=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --palette-sampling=adaptive-grid \
    -w 64 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    -M gpl:- -o /dev/null "${TOP_SRCDIR}/images/snake.png") || {
    echo "not ok 1 - explicit serial adaptive sampling failed"
    exit 0
}

parallel_palette=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    --palette-sampling=adaptive-grid \
    -w 64 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    -M gpl:- -o /dev/null "${TOP_SRCDIR}/images/snake.png") || {
    echo "not ok 1 - explicit parallel adaptive sampling failed"
    exit 0
}

test "${serial_palette}" = "${parallel_palette}" || {
    echo "not ok 1 - explicit adaptive palette changed with thread count"
    exit 0
}

echo "ok 1 - explicit adaptive palette is invariant across thread counts"
exit 0
