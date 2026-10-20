# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# A DLL with a developer-toolchain dependency must not become a release MSI.
param($Context)
$payload = $Context.Payload
$data = [IO.File]::ReadAllBytes($payload)
$text = [Text.Encoding]::ASCII.GetString($data)
$offset = $text.IndexOf('KERNEL32.dll', [StringComparison]::OrdinalIgnoreCase)
if ($offset -lt 0) { throw 'The codec fixture must import KERNEL32.dll.' }
# Preserve PE layout while replacing an import with a non-system DLL name.
while ($offset -ge 0) {
    [Text.Encoding]::ASCII.GetBytes('external.dll').CopyTo($data, $offset)
    $offset = $text.IndexOf('KERNEL32.dll', $offset + 12, [StringComparison]::OrdinalIgnoreCase)
}
$temporary = [IO.Path]::GetTempFileName()
try {
    [IO.File]::WriteAllBytes($temporary, $data)
    $rejected = $false
    try { $null = Get-WicPeInfo $temporary }
    catch {
        if ($_.Exception.Message -ne 'Link WIC dependencies statically before packaging: external.dll') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'A non-system DLL import escaped the packaging check.' }
} finally { Remove-Item -LiteralPath $temporary }
