#!/bin/sh
# TAP test: libpng timeline exposes required loader phases.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}

test -n "${ARTIFACT_LOCAL_DIR-}" || {
    printf "1..0 # SKIP ARTIFACT_LOCAL_DIR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
log_file="${ARTIFACT_LOCAL_DIR}/timeline-libpng-required-phases.json"

SIXEL_LOG_PATH="${log_file}" ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libpng! "${input_png}" >/dev/null || {
    echo "not ok 1 - libpng conversion failed while capturing timeline"
    exit 0
}

awk '
/"worker":"loader\/libpng"/ {
    if ($0 ~ /"role":"header\/read"/ && $0 ~ /"event":"start"/) hs = 1
    if ($0 ~ /"role":"header\/read"/ && $0 ~ /"event":"finish"/) hf = 1
    if ($0 ~ /"role":"decode\/pixels"/ && $0 ~ /"event":"start"/) ds = 1
    if ($0 ~ /"role":"decode\/pixels"/ && $0 ~ /"event":"finish"/) df = 1
    if ($0 ~ /"role":"emit\/frame"/ && $0 ~ /"event":"start"/) es = 1
    if ($0 ~ /"role":"emit\/frame"/ && $0 ~ /"event":"finish"/) ef = 1
}
END {
    exit !(hs && hf && ds && df && es && ef)
}
' "${log_file}" || {
    echo "not ok 1 - required loader phases were not fully visible"
    exit 0
}

echo "ok 1 - libpng timeline exposes required phases"
exit 0
