# SPDX-License-Identifier: MIT
# Build an MSI with Windows' own Installer automation API and cabinet tool.
# No SDK, WiX, NSIS, Python, PowerShell modules, or target DLL execution is used.
[CmdletBinding()]
param(
    [ValidateSet('Build', 'Install')][string]$Action = 'Build',
    [string]$Dll,
    [string]$Version,
    [string]$License,
    [string]$Output
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

function Invoke-MsiMethod($Object, [string]$Name, [object[]]$Arguments = @()) {
    # PowerShell pipeline results can carry PSObject wrappers. Reflection does
    # not unwrap those for IDispatch (notably OpenDatabase's VARIANT argument).
    for ($i = 0; $i -lt $Arguments.Count; $i++) {
        if ($null -ne $Arguments[$i]) { $Arguments[$i] = $Arguments[$i].PSObject.BaseObject }
    }
    $Object.GetType().InvokeMember($Name, 'InvokeMethod', $null, $Object, $Arguments)
}

function Set-MsiProperty($Object, [string]$Name, [object[]]$Arguments) {
    for ($i = 0; $i -lt $Arguments.Count; $i++) {
        if ($null -ne $Arguments[$i]) { $Arguments[$i] = $Arguments[$i].PSObject.BaseObject }
    }
    $Object.GetType().InvokeMember($Name, 'SetProperty', $null, $Object, $Arguments)
}

function Close-MsiObject($Object) {
    if ($null -ne $Object) {
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($Object)
    }
}

function Invoke-MsiSql($Database, [string]$Sql, $Record = $null) {
    $view = Invoke-MsiMethod $Database 'OpenView' @($Sql)
    try {
        Invoke-MsiMethod $view 'Execute' @($Record)
        Invoke-MsiMethod $view 'Close'
    } finally {
        Close-MsiObject $view
    }
}

function Add-MsiRow($Installer, $Database, [string]$Table, [object[]]$Values) {
    $record = Invoke-MsiMethod $Installer 'CreateRecord' @($Values.Count)
    try {
        for ($i = 0; $i -lt $Values.Count; $i++) {
            if ($null -eq $Values[$i] -or
                ($Values[$i] -is [string] -and $Values[$i].Length -eq 0)) { continue }
            $field = if ($Values[$i] -is [int]) { 'IntegerData' } else { 'StringData' }
            Set-MsiProperty $record $field @(($i + 1), $Values[$i])
        }
        $parameters = (@('?') * $Values.Count) -join ', '
        $columns = $script:msiColumns[$Table]
        Invoke-MsiSql $Database ('INSERT INTO `' + $Table + '` (' + $columns + ') VALUES (' + $parameters + ')') $record
    } finally {
        Close-MsiObject $record
    }
}

function Get-WicPeInfo([string]$Path) {
    # Inspect PE metadata without loading the DLL: cross-architecture packaging
    # must work, and packaging must never register a codec on the build machine.
    $data = [IO.File]::ReadAllBytes($Path)
    $pe = [BitConverter]::ToInt32($data, 60)
    if ($data[0] -ne 77 -or $data[1] -ne 90 -or
        [BitConverter]::ToUInt32($data, $pe) -ne 0x4550) {
        throw 'WIC payload is not a PE DLL.'
    }
    $arch = switch ([BitConverter]::ToUInt16($data, $pe + 4)) {
        0x014c { 'x86' }
        0x8664 { 'x64' }
        0xaa64 { 'arm64' }
        default { throw 'Unsupported WIC architecture.' }
    }
    if (([BitConverter]::ToUInt16($data, $pe + 22) -band 0x2000) -eq 0) {
        throw 'WIC payload must be a DLL.'
    }
    $opt = $pe + 24
    $directories = switch ([BitConverter]::ToUInt16($data, $opt)) {
        0x010b { $opt + 96 }
        0x020b { $opt + 112 }
        default { throw 'Unsupported PE optional header.' }
    }
    $sectionCount = [BitConverter]::ToUInt16($data, $pe + 6)
    $sectionStart = $opt + [BitConverter]::ToUInt16($data, $pe + 20)
    function Get-PeOffset([uint32]$Rva) {
        for ($s = 0; $s -lt $sectionCount; $s++) {
            $section = $sectionStart + $s * 40
            $start = [BitConverter]::ToUInt32($data, $section + 12)
            $size = [BitConverter]::ToUInt32($data, $section + 16)
            if ($Rva -ge $start -and ($Rva - $start) -lt $size) {
                return [int]([BitConverter]::ToUInt32($data, $section + 20) + $Rva - $start)
            }
        }
        throw 'PE address is outside file-backed sections.'
    }
    function Get-PeString([uint32]$Rva) {
        $start = Get-PeOffset $Rva
        $end = $start
        while ($end -lt $data.Length -and $data[$end] -ne 0) { $end++ }
        if ($end -eq $data.Length) { throw 'Unterminated PE string.' }
        return [Text.Encoding]::ASCII.GetString($data, $start, $end - $start)
    }
    $imports = @()
    $rva = [BitConverter]::ToUInt32($data, $directories + 8)
    if ($rva) {
        $pos = Get-PeOffset $rva
        while ([BitConverter]::ToUInt32($data, $pos + 12)) {
            $imports += Get-PeString ([BitConverter]::ToUInt32($data, $pos + 12))
            $pos += 20
        }
    }
    if ([BitConverter]::ToUInt32($data, $directories + 13 * 8)) {
        throw 'WIC payload must not have delay-loaded dependencies.'
    }
    $systemDlls = @('advapi32.dll', 'bcrypt.dll', 'gdi32.dll', 'kernel32.dll',
        'msvcrt.dll', 'ntdll.dll', 'ole32.dll', 'oleaut32.dll', 'shell32.dll',
        'shlwapi.dll', 'ucrtbase.dll', 'user32.dll', 'windowscodecs.dll')
    foreach ($name in $imports) {
        if ($name -notin $systemDlls -and $name -notmatch '^(api-ms-win-|ext-ms-win-)') {
            throw "Link WIC dependencies statically before packaging: $name"
        }
    }
    $exports = @()
    $rva = [BitConverter]::ToUInt32($data, $directories)
    if ($rva) {
        $export = Get-PeOffset $rva
        $count = [BitConverter]::ToUInt32($data, $export + 24)
        $names = Get-PeOffset ([BitConverter]::ToUInt32($data, $export + 32))
        for ($i = 0; $i -lt $count; $i++) {
            $exports += Get-PeString ([BitConverter]::ToUInt32($data, $names + $i * 4))
        }
    }
    foreach ($name in @('DllGetClassObject', 'DllCanUnloadNow', 'DllRegisterServer', 'DllUnregisterServer')) {
        if ($name -cnotin $exports) { throw "Missing WIC COM export: $name" }
    }
    [pscustomobject]@{ Architecture = $arch; Imports = $imports }
}

function New-WicMsi([string]$Dll, [string]$Version, [string]$License, [string]$Output) {
    $Dll = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Dll)
    $License = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($License)
    $Output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)
    $pe = Get-WicPeInfo $Dll
    $arch = $pe.Architecture
    if ($Version -notmatch '^(\d+)\.(\d+)\.(\d+)(?:[-+].*)?$') {
        throw 'MSI requires a major.minor.patch version, optionally followed by a prerelease suffix.'
    }
    $parts = @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3])
    if ($parts[0] -gt 255 -or $parts[1] -gt 255 -or $parts[2] -gt 65535) {
        throw 'Version exceeds Windows Installer limits (255.255.65535).'
    }
    $msiVersion = $parts -join '.'
    # These identities belong to installed resources, not individual builds.
    # Keep them stable across releases; each architecture owns separate resources.
    $ids = @{
        x86 = @('{45EC79EC-0D93-4F4E-A6E9-FBF10037C828}', '{FBA2C153-19E4-4EB6-9033-F99D403EEFCF}', '{CA15AFF5-8F45-48DB-BC75-484347C6B0B8}')
        x64 = @('{D9790752-C7CB-46DA-94A5-381FB503D28E}', '{F9091B61-EA0B-47AF-B878-51A2FA508364}', '{BA6426BA-76FC-4949-919A-AED0C34FEE5B}')
        arm64 = @('{63A667CC-482C-43E3-A994-C5455C5B771C}', '{AD199BFA-3085-43AE-B97F-0B037A733E4F}', '{A0A270FA-7869-473C-B225-3D443896E2C5}')
    }[$arch]
    $template = @{x86 = 'Intel'; x64 = 'x64'; arm64 = 'Arm64'}[$arch]
    $architectureCondition = @{
        x86 = 'NOT VersionNT64'
        x64 = 'VersionNT64 AND NOT Arm64'
        arm64 = 'Arm64'
    }[$arch]
    $attributes = if ($arch -eq 'x86') { 0 } else { 256 }
    $programFiles = if ($arch -eq 'x86') { 'ProgramFilesFolder' } else { 'ProgramFiles64Folder' }
    $temp = Join-Path ([IO.Path]::GetTempPath()) ('libsixel-wic-' + [guid]::NewGuid().ToString('N'))
    $installer = $null
    $database = $null
    [void][IO.Directory]::CreateDirectory($temp)
    try {
        # ASCII cabinet member names also match the File table primary keys.
        # Relative paths keep makecab's ANSI directive parser independent of
        # non-ASCII checkout/output paths. Only this private directory is removed.
        Copy-Item -LiteralPath $Dll -Destination (Join-Path $temp 'CodecDll')
        Copy-Item -LiteralPath $License -Destination (Join-Path $temp 'LicenseFile')
        @'
.OPTION EXPLICIT
.Set Cabinet=ON
.Set Compress=ON
.Set CompressionType=MSZIP
.Set CabinetNameTemplate=payload.cab
.Set DiskDirectoryTemplate=.
.Set MaxDiskSize=0
CodecDll
LicenseFile
'@ | Set-Content -LiteralPath (Join-Path $temp 'payload.ddf') -Encoding ASCII
        Push-Location -LiteralPath $temp
        try {
            & "$env:SystemRoot\System32\makecab.exe" /F payload.ddf /V1 | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "makecab failed: $LASTEXITCODE" }
        } finally { Pop-Location }

        $installer = New-Object -ComObject WindowsInstaller.Installer
        $database = Invoke-MsiMethod $installer 'OpenDatabase' @((Join-Path $temp 'package.msi'), 3)
        $schemas = @(
            'CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL PRIMARY KEY `Property`)',
            'CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)',
            'CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)',
            'CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)',
            'CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)',
            'CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)',
            'CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)',
            'CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)',
            'CREATE TABLE `Upgrade` (`UpgradeCode` CHAR(38) NOT NULL, `VersionMin` CHAR(20), `VersionMax` CHAR(20), `Language` CHAR(255), `Attributes` SHORT NOT NULL, `Remove` CHAR(255), `ActionProperty` CHAR(72) NOT NULL PRIMARY KEY `UpgradeCode`, `VersionMin`, `VersionMax`, `Language`, `Attributes`)',
            'CREATE TABLE `LaunchCondition` (`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Condition`)',
            'CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)',
            'CREATE TABLE `InstallUISequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)',
            'CREATE TABLE `AdminExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)'
        )
        $script:msiColumns = @{}
        foreach ($sql in $schemas) {
            Invoke-MsiSql $database $sql
            # Use the declared column order for parameterized INSERT statements.
            $table = [regex]::Match($sql, '^CREATE TABLE `(\w+)`').Groups[1].Value
            $columns = foreach ($column in [regex]::Matches($sql, '`(\w+)` (?:CHAR|SHORT|LONG)\b')) {
                '`' + $column.Groups[1].Value + '`'
            }
            $script:msiColumns[$table] = $columns -join ', '
        }
        $properties = [ordered]@{
            ProductCode = [guid]::NewGuid().ToString('B').ToUpperInvariant()
            UpgradeCode = $ids[0]
            ProductName = "libsixel WIC codec $Version ($arch)"
            ProductVersion = $msiVersion
            ProductLanguage = '1033'
            Manufacturer = 'libsixel developers'
            ALLUSERS = '1'
            INSTALLLEVEL = '1'
            ARPNOMODIFY = '1'
            ARPHELPLINK = 'https://github.com/saitoha/libsixel'
            SecureCustomProperties = 'WIC_OLDER;WIC_NEWER'
        }
        foreach ($key in $properties.Keys) {
            Add-MsiRow $installer $database 'Property' @($key, $properties[$key])
        }
        Add-MsiRow $installer $database 'Directory' @('TARGETDIR', $null, 'SourceDir')
        Add-MsiRow $installer $database 'Directory' @($programFiles, 'TARGETDIR', '.')
        Add-MsiRow $installer $database 'Directory' @('LibsixelDir', $programFiles, 'libsixel')
        Add-MsiRow $installer $database 'Directory' @('INSTALLDIR', 'LibsixelDir', "WIC-$arch")
        Add-MsiRow $installer $database 'Component' @('Codec', $ids[1], 'INSTALLDIR', $attributes, $null, 'CodecDll')
        Add-MsiRow $installer $database 'Component' @('License', $ids[2], 'INSTALLDIR', $attributes, $null, 'LicenseFile')
        Add-MsiRow $installer $database 'Feature' @('WicCodec', $null, 'SIXEL WIC codec', $null, 1, 1, 'INSTALLDIR', 0)
        foreach ($component in @('Codec', 'License')) {
            Add-MsiRow $installer $database 'FeatureComponents' @('WicCodec', $component)
        }
        # FileVersion must describe the actual PE resource, not ProductVersion.
        $fileVersion = Invoke-MsiMethod $installer 'FileVersion' @($Dll, $false)
        Add-MsiRow $installer $database 'File' @('CodecDll', 'Codec', 'wicsixel.dll|libwicsixel.dll', [int](Get-Item -LiteralPath $Dll).Length, $fileVersion, $null, 512, 1)
        Add-MsiRow $installer $database 'File' @('LicenseFile', 'License', 'LICENSE', [int](Get-Item -LiteralPath $License).Length, $null, $null, 512, 2)
        Add-MsiRow $installer $database 'Media' @(1, 2, $null, '#payload.cab', $null, $null)
        $index = 0
        # Declarative registration gives MSI ownership, repair, and rollback.
        # registry.tsv mirrors DllRegisterServer; no self-registration/custom action.
        foreach ($row in (Import-Csv -LiteralPath (Join-Path $PSScriptRoot 'registry.tsv') -Delimiter "`t")) {
            Add-MsiRow $installer $database 'Registry' @("Reg$index", [int]$row.Root, $row.Key, $row.Name, $row.Value, 'Codec')
            $index++
        }
        # Same-version rebuilds replace older packages too. MSI compares only
        # three version fields, so prerelease suffixes do not establish ordering.
        Add-MsiRow $installer $database 'Upgrade' @($ids[0], $null, $msiVersion, $null, 512, $null, 'WIC_OLDER')
        Add-MsiRow $installer $database 'Upgrade' @($ids[0], $msiVersion, $null, $null, 2, $null, 'WIC_NEWER')
        Add-MsiRow $installer $database 'LaunchCondition' @('Installed OR NOT WIC_NEWER', 'A newer libsixel WIC codec is already installed.')
        Add-MsiRow $installer $database 'LaunchCondition' @("Installed OR ($architectureCondition)", "This package requires native $arch Windows.")
        $sequence = [ordered]@{
            FindRelatedProducts = 25; LaunchConditions = 100
            CostInitialize = 800; FileCost = 900; CostFinalize = 1000
            InstallValidate = 1400; InstallInitialize = 1500
            # Removing inside the transaction restores the old package on failure.
            RemoveExistingProducts = 1510; ProcessComponents = 1600
            UnpublishFeatures = 1800; RemoveRegistryValues = 2600
            RemoveFiles = 3500; RemoveFolders = 3600
            InstallFiles = 4000; WriteRegistryValues = 5000
            RegisterUser = 6000; RegisterProduct = 6100
            PublishFeatures = 6300; PublishProduct = 6400; InstallFinalize = 6600
        }
        foreach ($key in $sequence.Keys) {
            Add-MsiRow $installer $database 'InstallExecuteSequence' @($key, $null, $sequence[$key])
        }
        foreach ($key in @('FindRelatedProducts', 'LaunchConditions', 'CostInitialize', 'FileCost', 'CostFinalize')) {
            Add-MsiRow $installer $database 'InstallUISequence' @($key, $null, $sequence[$key])
        }
        Add-MsiRow $installer $database 'InstallUISequence' @('ExecuteAction', $null, 1300)
        foreach ($key in @('CostInitialize', 'FileCost', 'CostFinalize', 'InstallValidate', 'InstallInitialize', 'InstallFiles', 'InstallFinalize')) {
            Add-MsiRow $installer $database 'AdminExecuteSequence' @($key, $null, $sequence[$key])
        }
        Add-MsiRow $installer $database 'AdminExecuteSequence' @('InstallAdminPackage', $null, 3900)
        $record = Invoke-MsiMethod $installer 'CreateRecord' @(1)
        try {
            Invoke-MsiMethod $record 'SetStream' @(1, (Join-Path $temp 'payload.cab'))
            Invoke-MsiSql $database 'INSERT INTO `_Streams` (`Name`, `Data`) VALUES (''payload.cab'', ?)' $record
        } finally { Close-MsiObject $record }
        $summary = $database.GetType().InvokeMember('SummaryInformation', 'GetProperty', $null, $database, @(20))
        try {
            $summaryValues = @{
                1 = 1252; 2 = 'Installation Database'; 3 = $properties.ProductName
                4 = 'libsixel developers'; 7 = "$template;1033"
                9 = [guid]::NewGuid().ToString('B').ToUpperInvariant()
                14 = 500; 15 = 2; 18 = 'libsixel Windows Installer builder'
            }
            foreach ($key in $summaryValues.Keys) {
                Set-MsiProperty $summary 'Property' @($key, $summaryValues[$key])
            }
            Invoke-MsiMethod $summary 'Persist'
        } finally { Close-MsiObject $summary }
        Invoke-MsiMethod $database 'Commit'
        Close-MsiObject $database
        $database = $null
        Copy-Item -LiteralPath (Join-Path $temp 'package.msi') -Destination $Output -Force
        Write-Host "Created $Output ($arch; system DLLs: $($pe.Imports -join ', '))"
    } finally {
        Close-MsiObject $database
        Close-MsiObject $installer
        Remove-Item -LiteralPath $temp -Recurse -Force
    }
}

function Install-WicMsi([string]$Path) {
    # Meson forwards DESTDIR. Staged installs must never alter COM registration.
    if ($env:DESTDIR) {
        Write-Host 'WIC installer staged; installation skipped (DESTDIR).'
        return
    }
    $Path = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "MSI not found: $Path" }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    try {
        $principal = New-Object Security.Principal.WindowsPrincipal($identity)
        $elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    } finally { $identity.Dispose() }
    $quiet = $env:SIXEL_WIC_MSI_QUIET -eq '1'
    if ($quiet -and -not $elevated) {
        throw 'Quiet WIC installation requires an elevated process.'
    }
    $arguments = @('/i', ('"' + $Path + '"'), '/norestart')
    $arguments += if ($quiet) { '/qn' } else { '/qb' }
    if ($env:SIXEL_WIC_MSI_LOG) {
        $arguments += @('/l*v', ('"' + $env:SIXEL_WIC_MSI_LOG + '"'))
    }
    $start = @{FilePath = "$env:SystemRoot\System32\msiexec.exe";
        Wait = $true; PassThru = $true; ArgumentList = $arguments}
    # Hosted CI already runs elevated; avoid an interactive elevation prompt.
    if (-not $elevated) { $start.Verb = 'RunAs' }
    $process = Start-Process @start
    if ($process.ExitCode -eq 3010) {
        Write-Host 'WIC installed; Windows requires a restart to finish replacing files.'
    } elseif ($process.ExitCode -ne 0) {
        throw "Windows Installer failed or was cancelled: $($process.ExitCode)"
    }
}

# Dot-sourcing exposes helpers to tests without producing or installing a package.
if ($MyInvocation.InvocationName -ne '.') {
    if ($Action -eq 'Install') { Install-WicMsi $Output }
    else { New-WicMsi $Dll $Version $License $Output }
}
