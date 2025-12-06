#!/bin/sh
# TAP test validating that the Libsixel.Decoder automation class can be
# activated in Windows Script Host via a side-by-side manifest. The test
# avoids registry changes by relying on Microsoft.Windows.ActCtx to spin up
# an activation context that exposes the ProgID without COM registration.

set -euxv

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

status=0
test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/wsh-manifest.log"
scripts_dir="${artifact_dir}/scripts"

mkdir -p "${artifact_dir}" "${scripts_dir}"

case $(uname -s 2>/dev/null || printf unknown) in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        :
        ;;
    *)
        skip_all "WSH automation is only available on Windows"
        ;;
esac

if ! command -v cscript >/dev/null 2>&1; then
    skip_all "cscript is unavailable; WSH automation cannot be tested"
fi

codec_root=${TOP_BUILDDIR:-".."}/wic
if [ ! -d "${codec_root}" ]; then
    skip_all "WIC codec was not built; skipping automation coverage"
fi

dll_path=$(find "${codec_root}" -maxdepth 4 -type f \
    \( -name '*wicsixel*.dll' -o -name 'wicsixel.dll' \
       -o -name 'libwicsixel.dll' \) | head -n 1 || true)

if [ -z "${dll_path}" ]; then
    skip_all "WIC codec DLL is unavailable; skipping automation coverage"
fi

sample_sixel=${TOP_SRCDIR:-".."}/images/snake.six
if [ ! -f "${sample_sixel}" ]; then
    skip_all "sample SIXEL input is missing"
fi

to_windows_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

dll_copy="${scripts_dir}/$(basename "${dll_path}")"
man_file="${scripts_dir}/libsixel-wic.manifest"
vbs_file="${scripts_dir}/exercise-decoder.vbs"

cp "${dll_path}" "${dll_copy}"

cat >"${man_file}" <<EOF_MAN
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="1.0.0.0"
                    processorArchitecture="*"
                    name="libsixel.wic.dispatch"
                    type="win32" />
  <file name="$(basename "${dll_copy}")">
    <!--
     - Export both the automation-friendly IDispatch entry point and the
     - IWICBitmapDecoder class. The automation layer internally calls
     - CoCreateInstance on CLSID_SixelDecoder, so the activation context must
     - describe that CLSID as well; advertising only the ProgID exposes the
     - dispatch object but leaves the decoder class invisible, causing
     - LoadFromString to fail with CLASS_E_CLASSNOTAVAILABLE.
     -->
    <comClass clsid="{1EAF6501-96E9-4A4E-92A2-2B15B90DDADE}"
              threadingModel="Both"
              progid="Libsixel.Decoder"
              description="libsixel decoder automation" />
    <comClass clsid="{15B9B4DA-B155-4977-8571-CF005884BCB9}"
              threadingModel="Both"
              description="WIC SIXEL decoder" />
  </file>
</assembly>
EOF_MAN

win_manifest=$(to_windows_path "${man_file}")
win_sample=$(to_windows_path "${sample_sixel}")

cat >"${vbs_file}" <<EOF_VBS
' Instantiate Libsixel.Decoder via a manifest-backed activation context and
' decode a SIXEL file into IPictureDisp. Any failure yields a non-zero exit
' code so the TAP harness can report a failure.
Option Explicit
Dim actctx, decoder, picture, fso, text
Dim width, height

Set fso = CreateObject("Scripting.FileSystemObject")
If Not fso.FileExists("${win_sample}") Then
    WScript.Echo "sample input not found: ${win_sample}"
    WScript.Quit 1
End If

On Error Resume Next
Set actctx = CreateObject("Microsoft.Windows.ActCtx")
If Err.Number <> 0 Then
    WScript.Echo "activation context unavailable: " & Hex(Err.Number)
    WScript.Quit 2
End If

actctx.Manifest = "${win_manifest}"
If Err.Number <> 0 Then
    WScript.Echo "failed to assign manifest: " & Hex(Err.Number)
    WScript.Quit 3
End If

Set decoder = actctx.CreateObject("Libsixel.Decoder")
If Err.Number <> 0 Or decoder Is Nothing Then
    WScript.Echo "decoder activation failed: " & Hex(Err.Number)
    WScript.Quit 4
End If

text = fso.OpenTextFile("${win_sample}", 1).ReadAll
Err.Clear
Set picture = Nothing
Call decoder.LoadFromString(text, picture)
If Err.Number <> 0 Then
    WScript.Echo "LoadFromString failed: " & Hex(Err.Number)
    WScript.Quit 5
End If

If picture Is Nothing Then
    WScript.Echo "LoadFromString returned no picture"
    WScript.Quit 6
End If

width = picture.Width
height = picture.Height
If width <= 0 Or height <= 0 Then
    WScript.Echo "invalid dimensions: " & width & "x" & height
    WScript.Quit 7
End If

WScript.Echo "decoded picture size: " & width & "x" & height
WScript.Quit 0
EOF_VBS

echo "1..1"

if cscript //nologo "${vbs_file}" >>"${log_file}" 2>&1; then
    pass 1 "WSH manifest activates Libsixel.Decoder"
else
    fail 1 "WSH manifest failed; see ${log_file}"
fi

exit ${status}
