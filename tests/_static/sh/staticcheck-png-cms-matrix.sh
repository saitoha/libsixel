#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
set -eux
echo '1..1'
set -v
test "${SIXEL_STATICCHECK_IMAGE_AUDITS:-0}" = 1 || {
    echo 'ok 1 # SKIP image audits are disabled'
    exit 0
}
# Configuration validates the interpreter when the option is enabled. A broken
# explicit selection is an error, never a successful optional-tool skip.
"$2" "$1/tools/generate_png_cms_matrix.py" --check >&2 || {
    echo 'not ok 1 - PNG CMS matrix inputs and documentation agree'
    exit 0
}
echo 'ok 1 - PNG CMS matrix inputs and documentation agree'
exit 0
