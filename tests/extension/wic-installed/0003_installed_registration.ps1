# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Check each declared machine registration value in the target registry view.
param($Context)
if (-not $Context.InstalledDll) { throw 'No installed WIC component path.' }
$view = if ($Context.Architecture -eq 'x86') { 'Registry32' } else { 'Registry64' }
$machine = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', $view)
try {
    foreach ($row in (Import-Csv "$($Context.SourceRoot)/wic/registry.tsv" -Delimiter "`t")) {
        $path = if ($row.Root -eq '0') { 'Software\Classes\' + $row.Key } else { $row.Key }
        $key = $machine.OpenSubKey($path)
        if ($null -eq $key) { throw "Missing installed registry key: $path" }
        try {
            $actual = $key.GetValue($row.Name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            $kind = $key.GetValueKind($row.Name).ToString()
            $wanted = $row.Value.Replace('[#CodecDll]', $Context.InstalledDll).Replace('[WIC_EMPTY]', '')
            $wantedKind = 'String'
            if ($wanted.StartsWith('#x')) {
                $wantedKind = 'Binary'
                $wanted = $wanted.Substring(2)
                $actual = [BitConverter]::ToString($actual).Replace('-', '')
            } elseif ($wanted.StartsWith('#')) {
                $wantedKind = 'DWord'
                $wanted = $wanted.Substring(1)
            }
            if ($kind -ne $wantedKind -or [string]$actual -cne $wanted) {
                throw "Installed registration differs: $path / $($row.Name)"
            }
        } finally { $key.Dispose() }
    }
} finally { $machine.Dispose() }
