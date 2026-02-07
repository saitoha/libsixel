#!/bin/sh
# TAP test that installs the wheel and verifies basic encode/decode
# functionality via the bundled Python bindings.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

# Skip immediately when Python wheel packaging is disabled in this build.
if [ "${ENABLE_PYTHON_WHEEL:-0}" != "1" ]; then
    skip_all "python wheel support is disabled in this build"
fi

. "${TOP_SRCDIR}/tests/lib/sh/packaging/python_wheel_common.sh"

setup_wheel_paths "${test_name}"
require_python3
require_venv_support
locate_wheel

echo "1..1"
set -v
status=0
case_id=1
verify_script="${ARTIFACT_LOCAL_DIR}/verify-bindings.py"

if create_virtualenv "${run_venv}" && \
   install_wheel "${run_venv}" && \
   write_roundtrip_script "${verify_script}" && \
   PYTHONPATH="" "${run_python}" "${verify_script}"; then
    pass ${case_id} "encodes and decodes via bundled wheel"
else
    fail ${case_id} "python import or round-trip failed"
fi

exit 0
