param(
  [Parameter(Mandatory = $false)]
  [ValidateSet('x64', 'amd64_arm64')]
  [string]$Arch = 'x64'
)

$ErrorActionPreference = 'Stop'

function Convert-SetOutputToMap {
  param(
    [string[]]$Lines
  )

  $map = [System.Collections.Generic.Dictionary[string,string]]::new([System.StringComparer]::OrdinalIgnoreCase)

  foreach ($line in $Lines) {
    if ([string]::IsNullOrWhiteSpace($line)) {
      continue
    }
    if ($line.StartsWith('=')) {
      continue
    }
    if (-not $line.Contains('=')) {
      continue
    }

    $name, $value = $line -split '=', 2
    if ([string]::IsNullOrWhiteSpace($name)) {
      continue
    }

    $map[$name] = $value
  }

  return $map
}

function Convert-PathListToUnique {
  param(
    [string]$Value
  )

  $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
  $items = [System.Collections.Generic.List[string]]::new()

  foreach ($item in ($Value -split ';')) {
    if ($item -eq '') {
      continue
    }
    if ($seen.Add($item)) {
      [void]$items.Add($item)
    }
  }

  return [string]::Join(';', $items)
}

function Write-GitHubEnvironment {
  param(
    [string]$Name,
    [string]$Value
  )

  if ([string]::IsNullOrWhiteSpace($env:GITHUB_ENV)) {
    throw 'GITHUB_ENV is not set.'
  }

  "$Name=$Value" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}

$programFilesX86 = ${env:ProgramFiles(x86)}
if ([string]::IsNullOrWhiteSpace($programFilesX86)) {
  throw 'ProgramFiles(x86) is not set.'
}

$vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
  throw "vswhere.exe was not found: $vswhere"
}

$installationPathRaw = & $vswhere -latest -products '*' -property installationPath | Select-Object -First 1
if ($LASTEXITCODE -ne 0) {
  throw 'vswhere.exe failed while locating Visual Studio.'
}
$installationPath = ''
if ($installationPathRaw) {
  $installationPath = $installationPathRaw.ToString().Trim()
}
if ([string]::IsNullOrWhiteSpace($installationPath)) {
  throw 'vswhere.exe did not return a Visual Studio installation path.'
}

$vcvarsall = Join-Path $installationPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path $vcvarsall)) {
  throw "vcvarsall.bat was not found: $vcvarsall"
}

$beforeMarker = '__LIBSIXEL_VCVARS_BEFORE__'
$afterMarker = '__LIBSIXEL_VCVARS_AFTER__'

# Capture the plain command prompt environment before and after vcvarsall.bat.
# The markers let us ignore the Visual Studio banner while keeping errors visible.
$cmdLine = "set && echo $beforeMarker && call `"$vcvarsall`" $Arch && echo $afterMarker && set"
$output = @(& $env:COMSPEC /d /s /c $cmdLine 2>&1 | ForEach-Object { $_.ToString() })
$exitCode = $LASTEXITCODE

foreach ($line in $output) {
  if ($line -match '^\[ERROR.*\]') {
    throw "vcvarsall.bat rejected the requested environment: $line"
  }
}
if ($exitCode -ne 0) {
  throw "vcvarsall.bat failed with exit code $exitCode."
}

$beforeIndex = [array]::IndexOf($output, $beforeMarker)
$afterIndex = [array]::IndexOf($output, $afterMarker)
if ($beforeIndex -lt 0 -or $afterIndex -lt 0 -or $afterIndex -le $beforeIndex) {
  throw 'Could not parse vcvarsall.bat environment output.'
}

$beforeLines = @($output[0..($beforeIndex - 1)])
$afterLines = @($output[($afterIndex + 1)..($output.Length - 1)])
$before = Convert-SetOutputToMap -Lines $beforeLines
$after = Convert-SetOutputToMap -Lines $afterLines
$pathLike = @('Path', 'PATH', 'INCLUDE', 'LIB', 'LIBPATH')

# Export only the environment changes produced by vcvarsall.bat.  This mirrors
# the action it replaces without depending on third-party JavaScript.
foreach ($entry in $after.GetEnumerator() | Sort-Object Name) {
  $name = $entry.Key
  $value = $entry.Value

  if ($pathLike -contains $name) {
    $value = Convert-PathListToUnique -Value $value
  }

  if ((-not $before.ContainsKey($name)) -or $before[$name] -ne $value) {
    Write-Host "Setting $name"
    Write-GitHubEnvironment -Name $name -Value $value
  }
}

Write-Host "Configured MSVC developer environment for $Arch."
