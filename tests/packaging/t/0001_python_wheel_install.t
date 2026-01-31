#!/bin/sh
# TAP test that verifies the wheel under python-wheel/dist installs into a
# virtual environment. This script does not exercise encode/decode; it only
# checks installation succeeds.

set -eu

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
WHEEL_HELPER_DIR="${script_dir}/../../lib/sh/packaging"
. "${WHEEL_HELPER_DIR}/python_wheel_common.sh"

test_name=$(basename "$0")
setup_wheel_paths "${test_name}"
require_python3
require_venv_support
locate_wheel

echo "1..1"
set -v
status=0
case_id=1

if create_virtualenv "${run_venv}" && \
   install_wheel "${run_venv}"; then
    pass ${case_id} "installs wheel from python/dist"
else
    fail ${case_id} "wheel installation failed"
fi

exit ${status}
