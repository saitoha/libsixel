#!/bin/sh
# Keep default GPU lookup tests independent of palette construction.
set -eux
echo '1..1'
set -v

awk '
FNR == 1 && NR > 1 { complete += palette && traced; palette = traced = 0 }
/^[[:space:]]+-d bluenoise -b xterm256([[:space:]]|$)/ { palette = 1 }
/LSXGPU2\|consumer=palette-apply\|kernel=/ { traced = 1 }
/--lookup-policy|[[:space:]]-~([[:space:]]|=)/ { explicit_lookup = 1 }
END { exit complete + (palette && traced) != 2 || explicit_lookup }
' "$1/tests/dither/bluenoise/0007_positional_8bit_bluenoise_gpu_default_serial.t" \
    "$1/tests/dither/bluenoise/0063_positional_8bit_bluenoise_gpu_default_parallel.t" || {
    echo 'not ok 1 - default GPU bluenoise lookup uses a fixed palette and checks GPU application'
    exit 0
}
echo 'ok 1 - default GPU bluenoise lookup uses a fixed palette and checks GPU application'
exit 0
