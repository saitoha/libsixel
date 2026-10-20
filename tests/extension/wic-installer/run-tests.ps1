# SPDX-License-Identifier: MIT
# Amortize PowerShell/COM startup while reporting each file as one TAP result.
param([string]$Dll, [string]$Package, [string]$Test = '*',
    [ValidateSet('Package', 'Installed')][string]$Mode = 'Package',
    [string]$Probe, [string]$LifetimeProbe)
$ErrorActionPreference = 'Stop'
if ($Mode -eq 'Installed' -and ($env:DESTDIR -or $env:SIXEL_INSTALLCHECK_DESTDIR)) {
    Write-Output '1..0 # SKIP staged MSI has not installed a system codec'
    exit 0
}
$setupTimer = [Diagnostics.Stopwatch]::StartNew()
# Reuse one native process; COM setup and teardown stay inside each case.
function Invoke-WicLifetime($Context, [string]$Target, [string]$Phase, [int]$FailAt) {
    if (-not $Context.InstalledDll) { throw 'No installed WIC component path.' }
    $process = $Context.LifetimeProcess
    if ($null -eq $process) { throw 'Lifetime probe process is unavailable.' }
    $process.StandardInput.WriteLine("$Target $Phase $FailAt")
    # Process.StandardInput autoflushes each line. Keep the protocol literal:
    # regex captures and per-line Write-Output command dispatch add first-call
    # cost without contributing to the native observation or its diagnostics.
    while ($null -ne ($line = $process.StandardOutput.ReadLine())) {
        if ($line -eq 'RESULT 0') { return }
        if ($line -like 'RESULT *') {
            throw "WIC lifetime observation failed: $Target/$Phase ($FailAt): $line."
        }
        "# $line"
    }
    throw 'Lifetime probe terminated before reporting a result.'
}
$payload = (Resolve-Path -LiteralPath $Dll).ProviderPath
$packagePath = (Resolve-Path -LiteralPath $Package).ProviderPath
. "$PSScriptRoot/../../../wic/installer.ps1"
. "$PSScriptRoot/msi-query.ps1"
$category = if ($Mode -eq 'Installed') { 'wic-installed' } else { 'wic-installer' }
$suiteName = if ($Mode -eq 'Installed') { 'wic-installed' } else { 'wic-msi' }
$tests = @(Get-ChildItem -LiteralPath "$PSScriptRoot/../$category" -Filter '????_*.ps1' |
    Where-Object { $_.BaseName -like $Test } | Sort-Object Name)
if ($tests.Count -eq 0) { throw "No WIC tests match: $Test" }
$installer = $database = $lifetimeProcess = $null
$lifetimeStartupMs = $null
$failed = $false
try {
    $pe = Get-WicPeInfo $payload
    $processArch = @{x86 = 'x86'; AMD64 = 'x64'; ARM64 = 'arm64'}[$env:PROCESSOR_ARCHITECTURE]
    # Use one matching process for the entire suite, never one per observation.
    if ($pe.Architecture -eq 'x86' -and $processArch -ne 'x86') {
        $relay = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
            '-File', $PSCommandPath, '-Dll', $payload, '-Package', $packagePath,
            '-Test', $Test, '-Mode', $Mode)
        if ($Probe) { $relay += @('-Probe', $Probe) }
        if ($LifetimeProbe) { $relay += @('-LifetimeProbe', $LifetimeProbe) }
        & "$env:SystemRoot\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" @relay
        exit $LASTEXITCODE
    }
    if ($pe.Architecture -eq $processArch -and
        $tests.Name -contains '0013_registration.ps1') {
        . "$PSScriptRoot/registration-capture.ps1"
    }
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $database = Invoke-MsiMethod $installer 'OpenDatabase' @($packagePath, 0)
    $context = [pscustomobject]@{
        Payload = $payload
        FileSize = (Get-Item -LiteralPath $payload).Length
        Architecture = $pe.Architecture
        Database = $database
    }
    if ($Mode -eq 'Installed') {
        # Query the exact MSI product and its registered component key path.
        # A previous manual registration must not make installcheck pass.
        $product = Read-MsiField $database 'SELECT `Value` FROM `Property` WHERE `Property` = ''ProductCode'''
        $component = Read-MsiField $database 'SELECT `ComponentId` FROM `Component` WHERE `KeyPath` = ''CodecDll'''
        $state = $installer.GetType().InvokeMember('ProductState', 'GetProperty', $null, $installer, @($product))
        $installedDll = ''
        if ($state -eq 5) {
            $installedDll = $installer.GetType().InvokeMember('ComponentPath', 'GetProperty', $null, $installer, @($product, $component))
        }
        $context | Add-Member -NotePropertyMembers @{
            ProductState = $state
            InstalledDll = $installedDll
            Probe = (Resolve-Path -LiteralPath $Probe).ProviderPath
            LifetimeProcess = $null
        }
    }
    if ($Mode -eq 'Installed' -and $context.InstalledDll -and
        @($tests | Where-Object { [int]$_.Name.Substring(0, 4) -ge 5 }).Count -gt 0) {
        $nativeStartup = [Diagnostics.Stopwatch]::StartNew()
        $startInfo = New-Object Diagnostics.ProcessStartInfo
        $startInfo.FileName = (Resolve-Path -LiteralPath $LifetimeProbe).ProviderPath
        $startInfo.Arguments = '--server'
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardInput = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.EnvironmentVariables['LIBSIXEL_WIC_INSTALLED_DLL'] = $context.InstalledDll
        $lifetimeProcess = [Diagnostics.Process]::Start($startInfo)
        if ($lifetimeProcess.StandardOutput.ReadLine() -ne 'READY') {
            throw 'Lifetime probe did not start.'
        }
        $nativeStartup.Stop()
        $lifetimeStartupMs = $nativeStartup.Elapsed.TotalMilliseconds
        $context.LifetimeProcess = $lifetimeProcess
    }
    # Parse each independent test once; avoid per-file PowerShell discovery.
    $cases = foreach ($file in $tests) {
        [pscustomobject]@{
            Name = $file.Name
            BaseName = $file.BaseName
            Body = [scriptblock]::Create([IO.File]::ReadAllText($file.FullName))
        }
    }
    $context | Add-Member -NotePropertyName SourceRoot -NotePropertyValue "$PSScriptRoot/../../.."
    $setupTimer.Stop()
    Write-Output "1..$($tests.Count)"
    $culture = [Globalization.CultureInfo]::InvariantCulture
    Write-Output ('# test-time: ' + $setupTimer.Elapsed.TotalMilliseconds.ToString('F3', $culture) +
        " ms SETUP $suiteName/setup")
    if ($null -ne $lifetimeStartupMs) {
        # This breakdown overlaps setup; it is not hidden or charged twice.
        Write-Output ('# native process startup: ' +
            $lifetimeStartupMs.ToString('F3', $culture) + ' ms (included in SETUP)')
    }
    $number = 0
    foreach ($case in $cases) {
        $number++
        if ($case.Name -eq '0013_registration.ps1' -and $pe.Architecture -ne $processArch) {
            Write-Output "ok $number - $($case.BaseName) # SKIP requires $($pe.Architecture) PowerShell"
            continue
        }
        $timer = [Diagnostics.Stopwatch]::StartNew()
        try {
            # Child scope prevents one test's locals from affecting another test.
            & $case.Body -Context $context
            $timer.Stop()
            $result = 'PASS'
            Write-Output "ok $number - $($case.BaseName)"
        } catch {
            $timer.Stop()
            $failed = $true
            $result = 'FAIL'
            Write-Output "not ok $number - $($case.BaseName)"
            Write-Output ('# ' + ($_.Exception.Message -replace '[\r\n]+', ' '))
        }
        Write-Output ('# test-time: ' + $timer.Elapsed.TotalMilliseconds.ToString('F3', $culture) +
            " ms $result extension/$category/$($case.Name)")
    }
} finally {
    if ($lifetimeProcess) {
        $lifetimeProcess.StandardInput.Close()
        $lifetimeProcess.WaitForExit()
        if ($lifetimeProcess.ExitCode -ne 0) {
            $failed = $true
            Write-Output '# Lifetime probe process failed during shutdown.'
        }
        $lifetimeProcess.Dispose()
    }
    Close-MsiObject $database
    Close-MsiObject $installer
}
# This is the harness status; individual observations are reported in TAP above.
if ($failed) { exit 1 }
