#!/bin/sh
# Verify keep policy omits an alpha-zero pixel even when -B is present.
# Policy: docs/loader/alpha-policy.md
# Policy: docs/concepts/pixelformat.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"
esc="$(printf '\033')"

keep_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "keep alpha-policy render failed"
    exit 0
}
palette="${keep_output#"${esc}"P0;1q\"1;1;1;1#0;2;}"
test "${palette}" != "${keep_output}" || {
    echo "not ok" 1 - "keep policy did not emit P2=1"
    exit 0
}
palette="${palette%"${esc}"\\}"
red="${palette%%;*}"
green_blue="${palette#*;}"
green="${green_blue%%;*}"
blue="${green_blue#*;}"
test "${green_blue}" != "${palette}" || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}
test "${blue}" != "${green_blue}" || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}
test "${red}" -ge 0 || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}
test "${red}" -le 100 || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}
test "${green}" -ge 0 || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}
test "${green}" -le 100 || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}
test "${blue}" -ge 0 || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}
test "${blue}" -le 100 || {
    echo "not ok" 1 - "keep policy painted the alpha-zero pixel"
    exit 0
}

echo "ok" 1 - "keep policy omits alpha zero and emits P2=1"
exit 0
