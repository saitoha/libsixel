#!/bin/sh
# TAP test verifying the wheel produced under python-wheel/dist installs
# and runs correctly inside an isolated virtual environment. The test only
# exercises the prebuilt wheel and must not rely on in-tree Python modules
# or shared libraries from the standard build output.

set -eu

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

skip_all() {
    printf '1..0 # SKIP %s\n' "$1"
    exit 0
}

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/wheel.log"
tmp_dir="${artifact_dir}/tmp"
run_venv="${tmp_dir}/venv"
run_python="${run_venv}/bin/python"
verify_script="${tmp_dir}/verify-bindings.py"

mkdir -p "${artifact_dir}" "${tmp_dir}"

python_bin=$(command -v python3 || true)
if [ -z "${python_bin}" ]; then
    skip_all "python3 is not available"
fi

wheel_dir="${TOP_BUILDDIR}/python-wheel/dist"
if [ ! -d "${wheel_dir}" ]; then
    skip_all "python-wheel/dist artifacts are unavailable"
fi

wheel_path=$(find "${wheel_dir}" -maxdepth 1 -type f \
    -name 'libsixel-*.whl' | head -n 1 || true)
if [ -z "${wheel_path}" ]; then
    skip_all "python wheel package is missing under python-wheel/dist"
fi

# Require venv/ensurepip so we can build an isolated environment for the
# wheel without depending on system site-packages.
if "${python_bin}" - <<'PY' >>"${log_file}" 2>&1; then
import importlib.util
missing = [m for m in ("venv", "ensurepip")
           if importlib.util.find_spec(m) is None]
if missing:
    raise SystemExit(f"missing modules: {', '.join(missing)}")
PY
    :
else
    skip_all "python3 lacks venv or ensurepip support"
fi

cat >"${verify_script}" <<'PY'
import pathlib
from libsixel import SIXEL_PIXELFORMAT_RGB888
from libsixel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT
from libsixel.decoder import Decoder, SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT

root = pathlib.Path(__file__).parent
encoded = root / "sample.six"
decoded = root / "roundtrip.png"

pixels = bytes(
    [
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ]
)

encoder = Encoder()
encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(encoded))
encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)

decoder = Decoder()
decoder.setopt(SIXEL_OPTFLAG_INPUT, str(encoded))
decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(decoded))
decoder.decode()

for path in (encoded, decoded):
    if not path.exists():
        raise SystemExit(f"missing artifact: {path}")
    if path.stat().st_size == 0:
        raise SystemExit(f"empty artifact: {path}")

print("encode/decode succeeded")
PY

echo "1..2"
status=0
case_id=1

if "${python_bin}" -m venv "${run_venv}" >>"${log_file}" 2>&1 && \
   "${run_python}" -m pip install --no-deps "${wheel_path}" \
        >>"${log_file}" 2>&1; then
    pass ${case_id} "installs wheel from python-wheel/dist"
else
    fail ${case_id} "wheel installation failed"
fi
case_id=$((case_id + 1))

if PYTHONPATH="" \
   "${run_python}" "${verify_script}" >>"${log_file}" 2>&1; then
    pass ${case_id} "encodes and decodes via bundled wheel"
else
    fail ${case_id} "python import or round-trip failed"
fi

exit ${status}
