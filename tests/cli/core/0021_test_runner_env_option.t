#!/bin/sh
# Confirm test_runner -%/--env behaves like wrapper-level --env.
set -eux

test "${HAVE_TEST_RUNNER-}" = 1 || {
    printf "1..0 # SKIP test_runner is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

out_wrapper="${ARTIFACT_LOCAL_DIR}/test_runner_env_wrapper.out"
out_option="${ARTIFACT_LOCAL_DIR}/test_runner_env_option.out"

run_test_runner --env SIXEL_PALETTE_KMEANS_INITTYPE=pca \
    "palette/0001_kmeans_init" >"${out_wrapper}" || {
    echo "not ok" 1 "test_runner wrapper --env execution failed"
    exit 0
}

run_test_runner -% SIXEL_PALETTE_KMEANS_INITTYPE=pca \
    "palette/0001_kmeans_init" >"${out_option}" || {
    echo "not ok" 1 "test_runner -% execution failed"
    exit 0
}

cmp -s "${out_wrapper}" "${out_option}" || {
    echo "not ok" 1 "test_runner -% output differs from wrapper --env"
    exit 0
}

run_test_runner -% INVALID "palette/0001_kmeans_init" > /dev/null && {
    echo "not ok" 1 "invalid test_runner -% argument should fail"
    exit 0
}

echo "ok" 1 "invalid test_runner -% argument rejected"
exit 0
