#!/bin/sh
# Verify libtiff:cms_engine applies the independent RGB ICC through short/env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBTIFF|cms_engine
# Registry binding: libtiff_cms_engine
# Policy: docs/loader/color-management/README.md
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 - CMS image regression # SKIP img2sixel unavailable"
    exit 0
}

# Both build systems publish this loader capability as HAVE_LIBTIFF.
test "${HAVE_LIBTIFF-}" = 1 || {
    echo "ok 1 - CMS image regression # SKIP libtiff unavailable"
    exit 0
}

# The harness supplies this path but does not create the directory.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb.tiff"
reference="${TOP_SRCDIR}/tests/data/color-numeric/cli-rgb-reference.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0092-cms-short-$$.six"
env_output="${artifact_dir}/0092-cms-env-$$.six"

trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --lookup-policy=none "-Llibtiff:Ebuiltin!" \
    "$input" 2>&1 >"${short_output}") || {
    echo "not ok 1 - short CMS conversion failed"
    exit 0
}
record=${trace##*"LSXSUB1|schema=9|base=libtiff|key=cms_engine|"}
test "${record%%[![:print:]]*}" = \
    "stored=1|binding=libtiff_cms_engine|value=2" || {
    echo "not ok 1 - final short CMS setting is not builtin"
    exit 0
}
# Inspect one complete actual-apply record, never a selector/parser record.
record=${trace##*"LSXCMS2|event=apply|"}
record=${record%%[![:print:]]*}
{ case "${record}" in engine=builtin\|*) true ;; *) false ;; esac &&      case "${record}" in *\|ok=1) true ;; *) false ;; esac; } || {
    echo "not ok 1 - short input profile was not applied by builtin"
    exit 0
}

trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --lookup-policy=none --env "SIXEL_LOADER_LIBTIFF_CMS_ENGINE=builtin" "-Llibtiff!" \
    "$input" 2>&1 >"${env_output}") || {
    echo "not ok 1 - env CMS conversion failed"
    exit 0
}
record=${trace##*"LSXSUB1|schema=9|base=libtiff|key=cms_engine|"}
test "${record%%[![:print:]]*}" = \
    "stored=1|binding=libtiff_cms_engine|value=2" || {
    echo "not ok 1 - final env CMS setting is not builtin"
    exit 0
}
# Inspect one complete actual-apply record, never a selector/parser record.
record=${trace##*"LSXCMS2|event=apply|"}
record=${record%%[![:print:]]*}
{ case "${record}" in engine=builtin\|*) true ;; *) false ;; esac &&      case "${record}" in *\|ok=1) true ;; *) false ;; esac; } || {
    echo "not ok 1 - env input profile was not applied by builtin"
    exit 0
}

cmp -s "$short_output" "$env_output" || {
    echo "not ok 1 - short and environment CMS output differ"
    exit 0
}

# Eight exact source colors require no palette reduction. Full scanning
# avoids lookup-table approximation. The sample budget is 1.275 levels for
# SIXEL percentage rounding, 0.5 for decoding, 0.5 for the PNG oracle, and
# 0.025 for floating-point color arithmetic (2.3 RGB byte levels total).
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "$reference" "$short_output" 'builtin:Enone!' 2.3 || {
    echo "not ok 1 - independent managed RGB samples exceed 2.3 levels"
    exit 0
}

echo "ok 1 - builtin CMS applies and matches independent RGB colors"
exit 0
