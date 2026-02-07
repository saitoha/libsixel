#!/bin/sh
# TAP test that verifies the wheel under python-wheel/dist installs into a
# virtual environment. This script does not exercise encode/decode; it only
# checks installation succeeds.

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

if create_virtualenv "${run_venv}" && \
   install_wheel "${run_venv}"; then
    pass ${case_id} "installs wheel from python/dist"
else
    fail ${case_id} "wheel installation failed"
fi

exit ${status}
