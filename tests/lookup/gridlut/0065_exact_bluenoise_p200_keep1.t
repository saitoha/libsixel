#!/bin/sh
# Verify exact bluenoise p200 keep1.
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
set -eux
echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup gridlut \
    "lookup/gridlut/0065_exact_bluenoise_p200_keep1" 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '# %s\n' "$trace"
    echo "not ok 1 - exact bluenoise p200 keep1"
    exit 0
}
# Require execution evidence as well as matching output.
case "$trace" in
    *'kernel=gridlut'*) true ;;
    *) false ;;
esac || {
    printf '# %s\n' "$trace"
    echo "not ok 1 - expected GPU lookup kernel was not selected"
    exit 0
}
echo "ok 1 - exact bluenoise p200 keep1"
exit 0
