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
    if [ -n "${SIXEL_TEST_PYTHON_VENV:-}" ] \
       && [ -x "${SIXEL_TEST_PYTHON_VENV}/bin/python" ]; then
        run_venv="${SIXEL_TEST_PYTHON_VENV}"
    else
        run_venv="${ARTIFACT_LOCAL_DIR}/venv"
    fi
    run_python="${run_venv}/bin/python"
    run_python=$(resolve_venv_python "${run_venv}")

    export ARTIFACT_LOCAL_DIR run_venv run_python
}

# Resolve the virtualenv interpreter path across POSIX and Windows layouts.
resolve_venv_python() {
    venv_path=$1

    if [ -x "${venv_path}/bin/python" ]; then
        printf '%s\n' "${venv_path}/bin/python"
        return 0
    fi

    if [ -x "${venv_path}/Scripts/python.exe" ]; then
        printf '%s\n' "${venv_path}/Scripts/python.exe"
        return 0
    fi

    if [ -x "${venv_path}/Scripts/python" ]; then
        printf '%s\n' "${venv_path}/Scripts/python"
        return 0
    fi

    return 1
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
    if [ -n "${SIXEL_TEST_PYTHON_VENV:-}" ] \
       && [ "$1" = "${SIXEL_TEST_PYTHON_VENV}" ] \
       && [ -x "${SIXEL_TEST_PYTHON_VENV}/bin/python" ]; then
        return 0
    fi
    "${python_bin}" -m venv "$1"
}

install_wheel() {
    venv_path=$1

    venv_python=$(resolve_venv_python "${venv_path}") || return 1

    if [ -n "${SIXEL_TEST_PYTHON_VENV:-}" ] \
       && [ "${venv_path}" = "${SIXEL_TEST_PYTHON_VENV}" ] \
       && "${venv_python}" -c \
          'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' \
          >/dev/null 2>&1; then
        return 0
    fi

    "${venv_python}" -m pip install --no-deps "${wheel_path}"
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
