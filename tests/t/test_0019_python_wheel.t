#!/bin/sh
# TAP test validating that the prebuilt Python wheel runs inside an
# isolated virtual environment. The script installs the wheel produced
# by the build (when --enable-python-wheel is active) and exercises a
# tiny encode/decode round-trip to confirm the bindings load correctly.

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

# Skip if the build skipped wheel generation. The autotools flag
# --enable-python-wheel controls this in the CI matrix.
wheel_dir="${TOP_BUILDDIR}/python/dist"
if [ ! -d "${wheel_dir}" ]; then
    skip_all "python wheel artifacts are unavailable (--enable-python-wheel is off)"
fi

wheel_path=$(find "${wheel_dir}" -maxdepth 1 -type f -name 'libsixel_python-*.whl' | head -n 1 || true)
if [ -z "${wheel_path}" ]; then
    skip_all "python wheel artifacts are unavailable (--enable-python-wheel is off)"
fi

# Require local venv/ensurepip modules so we can skip gracefully on
# minimal Python installations rather than failing the entire suite.
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

lib_paths=""
for candidate in "${TOP_BUILDDIR}/src/.libs" \
                 "${TOP_BUILDDIR}/src" \
                 "${TOP_BUILDDIR}/src/libsixel"; do
    if [ -d "${candidate}" ]; then
        if [ -z "${lib_paths}" ]; then
            lib_paths="${candidate}"
        else
            lib_paths="${lib_paths}:${candidate}"
        fi
    fi
done

if [ -z "${lib_paths}" ]; then
    first_match=$(find "${TOP_BUILDDIR}/src" -maxdepth 4 -type f \
        \( -name 'libsixel*.so*' -o -name 'libsixel*.dylib' \
           -o -name 'libsixel*.dll' \) | head -n 1 || true)
    if [ -n "${first_match}" ]; then
        lib_paths=$(dirname "${first_match}")
    fi
fi

if [ -z "${lib_paths}" ]; then
    skip_all "compiled libsixel library is unavailable"
fi

export LD_LIBRARY_PATH="${lib_paths}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export DYLD_LIBRARY_PATH="${lib_paths}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"

cat >"${verify_script}" <<'PY'
import pathlib
from libsixel import SIXEL_PIXELFORMAT_RGB888
from libsixel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT
from libsixel.decoder import Decoder, SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT

# Generate a 2x2 test pattern (red, green, blue, white) so the
# bindings exercise both encoding and decoding paths.
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
    pass ${case_id} "installs prebuilt python wheel"
else
    fail ${case_id} "wheel installation failed"
fi
case_id=$((case_id + 1))

if LD_LIBRARY_PATH="${LD_LIBRARY_PATH}" \
   DYLD_LIBRARY_PATH="${DYLD_LIBRARY_PATH}" \
   "${run_python}" "${verify_script}" >>"${log_file}" 2>&1; then
    pass ${case_id} "encodes and decodes via libsixel wheel"
else
    fail ${case_id} "python import or round-trip failed"
fi

exit ${status}
