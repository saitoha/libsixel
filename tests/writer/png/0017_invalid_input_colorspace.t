#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf '\n%s\n' "ok 1 # SKIP sixel2png disabled"
    exit 0
}
status=0
result=$(SIXEL_DECODER_INPUT_COLORSPACE=srgb ${SIXEL_RUNTIME-} \
    "${SIXEL2PNG_PATH}" -T auto </dev/null 2>&1) || status=$?
test "${status}" != 0 || {
    printf '\n%s\n' "not ok 1 - invalid combination succeeded"
    exit 0
}
case "${result}" in
    *'unknown option base value "auto"'*'srgb, display-p3, adobe-rgb, dci-p3, icc'*) ;;
    *) printf '\n%s\n' "not ok 1 - missing diagnostic"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - invalid_input_colorspace"
exit 0
