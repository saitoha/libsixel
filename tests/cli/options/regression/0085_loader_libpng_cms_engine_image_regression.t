#!/bin/sh
# Verify libpng:cms_engine applies the independent RGB ICC through short/env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBPNG|cms_engine
# Registry binding: libpng_cms_engine
# Policy: docs/loader/color-management.md
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - CMS image regression # SKIP img2sixel unavailable"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    echo "ok 1 - CMS image regression # SKIP libpng unavailable"
    exit 0
}

# The harness supplies this path but does not create the directory.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb.png"
reference="${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb-reference.png"
short_output="${ARTIFACT_LOCAL_DIR}/0085-cms-short.six"
env_output="${ARTIFACT_LOCAL_DIR}/0085-cms-env.six"

trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" "-Llibpng:Ebuiltin!" \
    "$input" 2>&1 >"${short_output}") || {
    echo "not ok 1 - short CMS conversion failed"
    exit 0
}
record=${trace##*"LSXSUB1|schema=9|base=libpng|key=cms_engine|"}
test "${record%%[![:print:]]*}" = \
    "stored=1|binding=libpng_cms_engine|value=2" || {
    echo "not ok 1 - final short CMS setting is not builtin"
    exit 0
}
# Inspect one complete actual-apply record, never a selector/parser record.
record=${trace##*"LSXCMS2|event=apply|"}
record=${record%%[![:print:]]*}
test "${record#engine=builtin|}" != "$record" && \
    test "${record%|ok=1}" != "$record" || {
    echo "not ok 1 - short input profile was not applied by builtin"
    exit 0
}

trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --env "SIXEL_LOADER_LIBPNG_CMS_ENGINE=builtin" "-Llibpng!" \
    "$input" 2>&1 >"${env_output}") || {
    echo "not ok 1 - env CMS conversion failed"
    exit 0
}
record=${trace##*"LSXSUB1|schema=9|base=libpng|key=cms_engine|"}
test "${record%%[![:print:]]*}" = \
    "stored=1|binding=libpng_cms_engine|value=2" || {
    echo "not ok 1 - final env CMS setting is not builtin"
    exit 0
}
# Inspect one complete actual-apply record, never a selector/parser record.
record=${trace##*"LSXCMS2|event=apply|"}
record=${record%%[![:print:]]*}
test "${record#engine=builtin|}" != "$record" && \
    test "${record%|ok=1}" != "$record" || {
    echo "not ok 1 - env input profile was not applied by builtin"
    exit 0
}

cmp -s "$short_output" "$env_output" || {
    echo "not ok 1 - short and environment CMS output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=none -m MS-SSIM \
    -b MS-SSIM:0.98 "$reference" "$short_output" >/dev/null || {
    echo "not ok 1 - independent managed RGB quality below 0.98"
    exit 0
}

echo "ok 1 - builtin CMS applies and matches independent RGB colors"
exit 0
