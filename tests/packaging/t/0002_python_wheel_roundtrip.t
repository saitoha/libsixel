#!/bin/sh
# TAP test that installs the wheel and verifies basic encode/decode
# functionality via the bundled Python bindings.

set -eu

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../lib/sh/packaging/python_wheel_common.sh"

test_name=$(basename "$0")
setup_wheel_paths "${test_name}"
require_python3
require_venv_support
locate_wheel

echo "1..1"
status=0
case_id=1
verify_script="${tmp_dir}/verify-bindings.py"

if create_virtualenv "${run_venv}" && \
   install_wheel "${run_venv}" && \
   write_roundtrip_script "${verify_script}" && \
   PYTHONPATH="" "${run_python}" "${verify_script}" >>"${log_file}" 2>&1; then
    pass ${case_id} "encodes and decodes via bundled wheel"
else
    fail ${case_id} "python import or round-trip failed"
fi

exit ${status}
