#!/bin/sh
# TAP test that validates the standalone Python wheel works in a clean
# virtual environment without relying on in-tree artifacts.

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
verify_script="${tmp_dir}/verify_bindings.py"

rm -rf "${artifact_dir}"
mkdir -p "${tmp_dir}"
mkdir -p "${artifact_dir}"

python_bin=$(command -v python3 || true)
if [ -z "${python_bin}" ]; then
    skip_all "python3 is not available"
fi

if "${python_bin}" - <<'PY' >>"${log_file}" 2>&1; then
import importlib.util
missing = [m for m in ("venv", "ensurepip")
           if importlib.util.find_spec(m) is None]
if missing:
    raise SystemExit(f"missing modules: {', '.join(missing)}")
PY
    :
else
    skip_all "python3 lacks venv or ensurepip"
fi

wheel_path=""
# When python wheel support is not built, the search below will fail to find
# any artifacts and the test will be skipped with a clear message.
wheel_roots="${TOP_BUILDDIR:-"$(pwd)"}"
for root in ${wheel_roots}; do
    for search_dir in "${root}/python/dist" \
                      "${root}/python-wheel/dist" \
                      "${root}/python-wheel/wheelhouse"; do
        if [ -d "${search_dir}" ]; then
            candidate=$(find "${search_dir}" -maxdepth 1 -type f \
                -name 'libsixel*.whl' | head -n 1 || true)
            if [ -n "${candidate}" ]; then
                wheel_path="${candidate}"
                break 2
            fi
        fi
    done
    if [ -z "${wheel_path}" ] && [ -d "${root}" ]; then
        candidate=$(find "${root}" -maxdepth 4 -type f -name 'libsixel*.whl' \
            | head -n 1 || true)
        if [ -n "${candidate}" ]; then
            wheel_path="${candidate}"
            break
        fi
    fi
done

if [ -z "${wheel_path}" ]; then
    skip_all "python wheel artifacts are unavailable"
fi

echo "1..2"
status=0
case_id=1

if "${python_bin}" -m venv "${run_venv}" >>"${log_file}" 2>&1 && \
   PYTHONNOUSERSITE=1 "${run_python}" -m pip install --no-deps \
        --no-index "${wheel_path}" >>"${log_file}" 2>&1; then
    pass ${case_id} "installs prebuilt python wheel"
else
    fail ${case_id} "wheel installation failed"
fi
case_id=$((case_id + 1))

cat >"${verify_script}" <<'PY'
import importlib.util
import pathlib
import sys

spec = importlib.util.find_spec("libsixel_wheel")
if spec is None or not spec.submodule_search_locations:
    raise SystemExit("libsixel_wheel package is unavailable")

package_root = pathlib.Path(list(spec.submodule_search_locations)[0])
binary_dir = package_root / ".binaries"
if not binary_dir.exists():
    raise SystemExit(f"missing .binaries directory under {package_root}")

binary_candidates = list(binary_dir.glob("libsixel*.so*"))
binary_candidates.extend(binary_dir.glob("libsixel*.dll"))
binary_candidates.extend(binary_dir.glob("libsixel*.dylib"))
if not binary_candidates:
    raise SystemExit(f"no libsixel binaries found in {binary_dir}")

expected_names = ["libsixel-1.so", "libsixel.so.1", "libsixel.so"]
current_names = {candidate.name for candidate in binary_candidates}
if not any(name in current_names for name in expected_names):
    target = binary_candidates[0]
    for name in expected_names:
        link = binary_dir / name
        if not link.exists():
            link.symlink_to(target.name)

import libsixel_wheel as sixel

venv_root = pathlib.Path(sys.prefix).resolve()
module_path = pathlib.Path(sixel.__file__).resolve()
if venv_root not in module_path.parents:
    raise SystemExit(f"libsixel_wheel loaded outside venv: {module_path}")

lib_path = sixel.lib_path()
if lib_path is None:
    raise SystemExit("libsixel shared library failed to load")
if venv_root not in pathlib.Path(lib_path).resolve().parents:
    raise SystemExit(f"libsixel library not isolated in venv: {lib_path}")

root = pathlib.Path(__file__).parent
encoded = root / "wheel_sample.six"
decoded = root / "wheel_roundtrip.png"

pixels = bytes(
    [
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ]
)

encoder = sixel.sixel_encoder_new()
sixel.sixel_encoder_setopt(encoder, sixel.SIXEL_OPTFLAG_OUTPUT, str(encoded))
sixel.sixel_encoder_encode_bytes(
    encoder,
    pixels,
    2,
    2,
    sixel.SIXEL_PIXELFORMAT_RGB888,
    None,
)

decoder = sixel.sixel_decoder_new()
sixel.sixel_decoder_setopt(decoder, sixel.SIXEL_OPTFLAG_INPUT, str(encoded))
sixel.sixel_decoder_setopt(decoder, sixel.SIXEL_OPTFLAG_OUTPUT, str(decoded))
sixel.sixel_decoder_decode(decoder)

for path in (encoded, decoded):
    if not path.exists():
        raise SystemExit(f"missing artifact: {path}")
    if path.stat().st_size == 0:
        raise SystemExit(f"empty artifact: {path}")

print("wheel encode/decode succeeded")
PY

if env -i \
    PATH="$(dirname "${run_python}"):${PATH}" \
    HOME="${HOME}" \
    TMPDIR="${tmp_dir}" \
    PYTHONPATH= \
    PYTHONHOME= \
    PYTHONNOUSERSITE=1 \
    "${run_python}" "${verify_script}" >>"${log_file}" 2>&1; then
    pass ${case_id} "imports wheel and completes encode/decode"
else
    fail ${case_id} "python import or round-trip failed"
fi

exit ${status}
