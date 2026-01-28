#!/bin/sh
# Common helpers for python wheel TAP tests. Each test creates its own
# artifact and temporary directories under the configured ARTIFACT_ROOT.

set -eu

wheel_common_path=${wheel_common_path:-"$0"}
wheel_helper_dir=${WHEEL_HELPER_DIR-}
if [ -z "${wheel_helper_dir}" ]; then
    wheel_helper_dir=$(CDPATH=; cd "$(dirname "${wheel_common_path}")" && pwd)
fi
. "${wheel_helper_dir}/../common/tap.sh"

setup_wheel_paths() {
    test_name=$1
    test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
    category_name=$(basename "$(dirname "${test_dir}")")
    artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
    artifact_dir="${artifact_root}/${category_name}/${test_name}"
    log_file="${artifact_dir}/wheel.log"
    tmp_dir="${artifact_dir}/tmp"
    run_venv="${tmp_dir}/venv"
    run_python="${run_venv}/bin/python"

    export artifact_dir log_file tmp_dir run_venv run_python
    mkdir -p "${artifact_dir}" "${tmp_dir}"
}

require_python3() {
    python_bin=$(command -v python3 || true)
    if [ -z "${python_bin}" ]; then
        tap_skip_all "python3 is not available"
    fi
    export python_bin
}

require_venv_support() {
    if "${python_bin}" - <<'PY' >>"${log_file}" 2>&1; then
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
    wheel_dir="${TOP_BUILDDIR}/python-wheel/dist"
    if [ ! -d "${wheel_dir}" ]; then
        tap_skip_all "python-wheel/dist artifacts are unavailable"
    fi

    wheel_path=$(find "${wheel_dir}" -maxdepth 1 -type f -name 'libsixel-*.whl' \
        | head -n 1 || true)
    if [ -z "${wheel_path}" ]; then
        tap_skip_all "python wheel package is missing under python-wheel/dist"
    fi

    export wheel_path
}

create_virtualenv() {
    "${python_bin}" -m venv "$1" >>"${log_file}" 2>&1
}

install_wheel() {
    venv_path=$1
    "${venv_path}/bin/python" -m pip install --no-deps "${wheel_path}" \
        >>"${log_file}" 2>&1
}

write_roundtrip_script() {
    script_path=$1
    cat >"${script_path}" <<'PY'
import pathlib
from libsixel import SIXEL_PIXELFORMAT_RGB888
from libsixel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT
from libsixel.decoder import Decoder, SIXEL_OPTFLAG_INPUT, SIXEL_OPTFLAG_OUTPUT

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
