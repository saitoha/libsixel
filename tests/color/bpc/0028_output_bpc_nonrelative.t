#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
data="${TOP_SRCDIR}/tests/data/color-numeric"
result=0
message=$(SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-Lbuiltin:Ebuiltin:Rabsolute!!" \
    -U "icc:P${data}/output-bpc.icc:bpc=1" "${data}/output-bpc-rgba16.png" 2>&1) || result=$?
test "$result" -ne 0 || {
    printf '\n%s\n' 'not ok 1 - accepted an incompatible ICC BPC request'
    exit 0
}
case "$message" in *'ICC output bpc=1 requires relative cms_intent.'*) true ;; *) false ;; esac || {
    printf '\n%s\n' 'not ok 1 - missing matching ICC BPC diagnostic'
    exit 0
}
printf '\n%s\n' 'ok 1 - strict ICC BPC request rejected'
exit 0
