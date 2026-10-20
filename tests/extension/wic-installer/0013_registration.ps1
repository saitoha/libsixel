# SPDX-License-Identifier: MIT
# Test-plan: docs/testing/companion-programs-coverage.md
# Compare MSI authoring data with the DLL's registration in a private registry
# subtree. RegOverridePredefKey affects only this process; no codec is installed.
param($Context)
$payload = $Context.Payload
$testKey = 'Software\libsixel-wic-tests-' + [guid]::NewGuid().ToString('N')
$root = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($testKey)
$classes = $root.CreateSubKey('Classes')
$machine = $root.CreateSubKey('Machine')
$hkcr = [IntPtr](-2147483648)
$hklm = [IntPtr](-2147483646)
try {
    if ([WicRegistrationCapture]::RegOverridePredefKey($hkcr, $classes.Handle.DangerousGetHandle()) -ne 0) {
        throw 'Cannot redirect HKCR.'
    }
    if ([WicRegistrationCapture]::RegOverridePredefKey($hklm, $machine.Handle.DangerousGetHandle()) -ne 0) {
        throw 'Cannot redirect HKLM.'
    }
    try { [WicRegistrationCapture]::Register($payload) }
    finally {
        [void][WicRegistrationCapture]::RegOverridePredefKey($hkcr, [IntPtr]::Zero)
        [void][WicRegistrationCapture]::RegOverridePredefKey($hklm, [IntPtr]::Zero)
    }
    $expected = @{}
    foreach ($row in (Import-Csv "$($Context.SourceRoot)/wic/registry.tsv" -Delimiter "`t")) {
        $base = if ($row.Root -eq '0') { $classes } else { $machine }
        $key = $base.OpenSubKey($row.Key)
        if ($null -eq $key) { throw "Missing DLL registration key: $($row.Key)" }
        try {
            $actual = $key.GetValue($row.Name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            $kind = $key.GetValueKind($row.Name).ToString()
            $wanted = $row.Value.Replace('[#CodecDll]', $payload).Replace('[WIC_EMPTY]', '')
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
                throw "Registration differs: $($row.Key) / $($row.Name): $actual ($kind), expected $wanted ($wantedKind)"
            }
            $expected["$($row.Root)|$($row.Key)|$($row.Name)"] = $true
        } finally { $key.Dispose() }
    }
    function Assert-NoExtraValues($Key, [string]$RootNumber, [string]$Path = '') {
        foreach ($name in $Key.GetValueNames()) {
            if (-not $expected.ContainsKey("$RootNumber|$Path|$name")) {
                throw "DLL registration is missing from MSI: $RootNumber / $Path / $name"
            }
        }
        foreach ($name in $Key.GetSubKeyNames()) {
            $child = $Key.OpenSubKey($name)
            $childPath = if ($Path) { "$Path\$name" } else { $name }
            try { Assert-NoExtraValues $child $RootNumber $childPath }
            finally { $child.Dispose() }
        }
    }
    Assert-NoExtraValues $classes '0'
    Assert-NoExtraValues $machine '2'
} finally {
    [void][WicRegistrationCapture]::RegOverridePredefKey($hkcr, [IntPtr]::Zero)
    [void][WicRegistrationCapture]::RegOverridePredefKey($hklm, [IntPtr]::Zero)
    $machine.Dispose()
    $classes.Dispose()
    $root.Dispose()
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($testKey)
}
