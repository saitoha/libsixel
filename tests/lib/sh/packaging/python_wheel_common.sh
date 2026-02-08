#!/bin/sh
# Common helpers for python wheel TAP tests. Each test creates its own
# artifact and temporary directories under the configured ARTIFACT_ROOT.

set -eu

wheel_common_path=${wheel_common_path:-"$0"}
wheel_helper_dir=${WHEEL_HELPER_DIR-}
if [ -z "${wheel_helper_dir}" ]; then
    wheel_helper_dir=$(CDPATH=; cd "$(dirname "${wheel_common_path}")" && pwd)
fi

setup_wheel_paths() {
    run_venv="${ARTIFACT_LOCAL_DIR}/venv"
    run_python="${run_venv}/bin/python"

    export ARTIFACT_LOCAL_DIR run_venv run_python
}

require_python3() {
    python_bin=$(command -v python3 || true)
    if [ -z "${python_bin}" ]; then
        tap_skip_all "python3 is not available"
    fi
    export python_bin
}

require_venv_support() {
    if "${python_bin}" - <<'PY'; then
import importlib.util
missing = [m for m in ("venv", "ensurepip") if importlib.util.find_spec(m) is None]
if missing:
    raise SystemExit(f"missing modules: {', '.join(missing)}")
PY
        :
    else
        tap_skip_all "python3 lacks venv or ensurepip support"
    fi
}

locate_wheel() {
    wheel_dir="${TOP_BUILDDIR}/python/dist"
    if [ ! -d "${wheel_dir}" ]; then
        tap_skip_all "python/dist artifacts are unavailable"
    fi

    wheel_path=$(find "${wheel_dir}" -maxdepth 1 -type f -name 'libsixel_wheel-*.whl' \
        | head -n 1 || true)
    if [ -z "${wheel_path}" ]; then
        tap_skip_all "python wheel package is missing under python/dist"
    fi

    export wheel_path
}

create_virtualenv() {
    "${python_bin}" -m venv "$1"
}

install_wheel() {
    venv_path=$1
    "${venv_path}/bin/python" -m pip install --no-deps "${wheel_path}"
}

write_roundtrip_script() {
    script_path=$1
    cat >"${script_path}" <<'PY'
import pathlib
from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
from libsixel_wheel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT
from libsixel_wheel.decoder import Decoder, SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT

root = pathlib.Path(__file__).parent
encoded = root / "sample.six"
decoded = root / "roundtrip.png"

pixels = bytes([
    255, 0, 0,
    0, 255, 0,
    0, 0, 255,
    255, 255, 255,
])

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
}
