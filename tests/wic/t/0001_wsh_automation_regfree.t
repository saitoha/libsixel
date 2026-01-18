#!/bin/sh
# TAP test verifying registration-free WSH automation decoding.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/wsh_automation.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

case "$(uname -s)" in
    CYGWIN*|MINGW*|MSYS*)
        :
        ;;
    *)
        skip_all "Windows host required for WSH automation"
        ;;
esac

ensure_feature_available "HAVE_WICCODEC" "wiccodec" "WIC codec support"

systemroot=${SystemRoot:-}
if [ -n "${systemroot}" ] && [ -x "${systemroot}/System32/cscript.exe" ]; then
    cscript_path="${systemroot}/System32/cscript.exe"
elif command -v cscript >/dev/null 2>&1; then
    cscript_path=$(command -v cscript)
elif command -v cscript.exe >/dev/null 2>&1; then
    cscript_path=$(command -v cscript.exe)
else
    skip_all "cscript not available"
fi

wicsixel_dll=""
for candidate in \
    "${top_builddir}/wic/libwicsixel.dll" \
    "${top_builddir}/wic/wicsixel.dll"; do
    if [ -f "${candidate}" ]; then
        wicsixel_dll=${candidate}
        break
    fi
done

if [ -z "${wicsixel_dll}" ]; then
    skip_all "WIC codec DLL not built"
fi

vbs_source="${top_srcdir}/tests/wic/wsh_decoder_regfree.vbs"
require_file "${vbs_source}"

sixel_input="${images_dir}/snake.six"
require_file "${sixel_input}"

echo "1..1"

pass() {
    printf 'ok 1 - %s\n' "$1"
}

fail() {
    printf 'not ok 1 - %s\n' "$1"
    exit 1
}

regfree_dir="${artifact_dir}/regfree"
mkdir -p "${regfree_dir}"

temp_base=${TMPDIR:-${TEMP:-/tmp}}
exec_root=$(mktemp -d "${temp_base%/}/libsixel-wsh-XXXXXX")
trap 'rm -rf "${exec_root}"' EXIT
exec_regfree_dir="${exec_root}/regfree"
mkdir -p "${exec_regfree_dir}"

cscript_copy="${exec_regfree_dir}/cscript.exe"
cp "${cscript_path}" "${cscript_copy}"
chmod +x "${cscript_copy}" || :

dll_name=$(basename "${wicsixel_dll}")
cp "${wicsixel_dll}" "${exec_regfree_dir}/${dll_name}"

cp "${vbs_source}" "${exec_regfree_dir}/wsh_decoder_regfree.vbs"

if command -v ldd >/dev/null 2>&1; then
    ldd "${wicsixel_dll}" >"${artifact_dir}/wicsixel_ldd.log" 2>&1 || :
    awk '/=>/ && $3 ~ /^\// {print $3}' \
            "${artifact_dir}/wicsixel_ldd.log" \
        | while read -r dep_path; do
            dep_name=$(basename "${dep_path}")
            if [ -f "${dep_path}" ]; then
                cp "${dep_path}" "${exec_regfree_dir}/${dep_name}"
            fi
        done
fi

manifest_path="${exec_regfree_dir}/cscript.exe.manifest"
cat >"${manifest_path}" <<EOF_MANIFEST
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity
    version="1.0.0.0"
    processorArchitecture="*"
    name="libsixel.regfree"
    type="win32" />
  <file name="${dll_name}">
    <comClass
      clsid="{1EAF6501-96E9-4A4E-92A2-2B15B90DDADE}"
      progid="Libsixel.Decoder.1"
      threadingModel="Both" />
    <comClass
      clsid="{1EAF6501-96E9-4A4E-92A2-2B15B90DDADE}"
      progid="Libsixel.Decoder"
      threadingModel="Both" />
  </file>
</assembly>
EOF_MANIFEST

mt_path=""
if command -v mt >/dev/null 2>&1; then
    mt_path=$(command -v mt)
elif command -v mt.exe >/dev/null 2>&1; then
    mt_path=$(command -v mt.exe)
elif command -v cmd >/dev/null 2>&1; then
    mt_path=$(cmd //c "where mt.exe" 2>/dev/null | head -n 1 || :)
fi

if [ -n "${mt_path}" ]; then
    cscript_win=$(cygpath -w "${cscript_copy}")
    manifest_win=$(cygpath -w "${manifest_path}")
    "${mt_path}" -manifest "${manifest_win}" \
        -outputresource:"${cscript_win};#1" \
        >"${artifact_dir}/mt_embed.log" 2>&1 || :
    chmod +x "${cscript_copy}" || :
fi

cp "${manifest_path}" "${regfree_dir}/cscript.exe.manifest"

direct_log="${artifact_dir}/wsh_automation_direct.log"
if "${cscript_copy}" //nologo "${exec_regfree_dir}/wsh_decoder_regfree.vbs" \
        "${sixel_input}" >"${direct_log}" 2>&1; then
    mv "${direct_log}" "${log_file}"
else
    cmd_path=""
    if command -v cmd.exe >/dev/null 2>&1; then
        cmd_path=$(command -v cmd.exe)
    elif command -v cmd >/dev/null 2>&1; then
        cmd_path=$(command -v cmd)
    fi
    if [ -n "${cmd_path}" ]; then
        cscript_win=$(cygpath -wa "${cscript_copy}")
        vbs_win=$(cygpath -wa "${exec_regfree_dir}/wsh_decoder_regfree.vbs")
        input_win=$(cygpath -wa "${sixel_input}")
        cmd_line="\"${cscript_win}\" //nologo \"${vbs_win}\" \"${input_win}\""
        printf '%s\n' "INFO: retry via cmd.exe" >"${log_file}"
        printf '%s\n' "INFO: cmd.exe /d /s /c ${cmd_line}" >>"${log_file}"
        "${cmd_path}" /d /s /c "${cmd_line}" >>"${log_file}" 2>&1 || :
    fi
fi

if [ ! -s "${log_file}" ]; then
    fail "WSH automation decode failed (see ${direct_log})"
fi

if grep -Eq '^OK [0-9]+ [0-9]+$' "${log_file}"; then
    :
else
    fail "automation decode did not report dimensions"
fi

if awk 'BEGIN{ok=0} /^OK / {if ($2 > 0 && $3 > 0) ok=1} END{exit ok?0:1}' \
        "${log_file}"; then
    pass "reg-free WSH automation decode succeeded"
else
    fail "automation decode reported invalid dimensions"
fi
