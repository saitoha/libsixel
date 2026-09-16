#!/bin/sh
# Verify --env SIXEL_LOADER_CMS_ENGINE=none, --env SIXEL_LOADER_LIBWEBP_CMS_ENGINE=color-sync reaches the canonical loader setting.
# Policy: docs/loader/color-management.md
# This owns alias/precedence resolution, not CMS execution or color accuracy.
# Inspect the last complete record for this loader and key.
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 && test "${HAVE_WEBP-}" = 1 || {
    echo "ok 1 - CMS alias resolution # SKIP img2sixel/libwebp unavailable"
    exit 0
}

input="${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb.webp"

canonical=$(SIXEL_TRACE_TOPIC=suboption_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_CMS_ENGINE=none \
    --env SIXEL_LOADER_LIBWEBP_CMS_ENGINE=colorsync \
    -Llibwebp! "$input" 2>&1 >/dev/null) || {
    echo "not ok 1 - canonical CMS selection failed"
    exit 0
}
record=${canonical##*"LSXSUB1|schema=9|base=libwebp|key=cms_engine|"}
test "${record%%[![:print:]]*}" = \
    "stored=1|binding=libwebp_cms_engine|value=4" || {
    echo "not ok 1 - last canonical CMS selection has the wrong value"
    exit 0
}

alias=$(SIXEL_TRACE_TOPIC=suboption_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_CMS_ENGINE=none \
    --env SIXEL_LOADER_LIBWEBP_CMS_ENGINE=color-sync \
    -Llibwebp! "$input" 2>&1 >/dev/null) || {
    echo "not ok 1 - alias CMS selection failed"
    exit 0
}
record=${alias##*"LSXSUB1|schema=9|base=libwebp|key=cms_engine|"}
test "${record%%[![:print:]]*}" = \
    "stored=1|binding=libwebp_cms_engine|value=4" || {
    echo "not ok 1 - last alias CMS selection has the wrong value"
    exit 0
}

echo "ok 1 - CMS alias resolves to the expected loader setting"
exit 0
