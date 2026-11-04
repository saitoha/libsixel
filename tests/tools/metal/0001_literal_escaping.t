#!/bin/sh
# Policy: docs/build/support-scripts.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-26
# Quotes, backslashes and trigraph spelling must survive C compilation.
set -eux
echo '1..1'
set -v
actual=$(sh "$TOP_SRCDIR/tools/embed-metal.sh" - - <<'METAL'
"a\b??/"
METAL
) || {
    echo 'not ok 1 - Metal literal generation failed'
    exit 0
}
test "${actual##*
}" = '"\"a\\b\?\?/\"\n",' || {
    echo 'not ok 1 - Metal spelling is not preserved by C escaping'
    exit 0
}
echo 'ok 1 - Metal spelling is preserved by C escaping'
exit 0
