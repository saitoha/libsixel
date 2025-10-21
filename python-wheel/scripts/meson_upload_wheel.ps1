[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$wheelhouseDir = Join-Path $repoRoot "python-wheel\wheelhouse"

$repository = if ($env:PYPI_REPO) { $env:PYPI_REPO } else { "pypi" }
$username = if ($env:PYPI_USERNAME) { $env:PYPI_USERNAME } else { "__token__" }
$passwordEnv = if ($env:PYPI_PASSWORD_ENV) { $env:PYPI_PASSWORD_ENV } else { "TWINE_PASSWORD" }

if (-not $wheelhouseDir) {
    throw "wheelhouse directory not set"
}

if (-not (Test-Path $wheelhouseDir)) {
    throw "wheelhouse directory not found: $wheelhouseDir"
}

$password = $env:$passwordEnv
if (-not $password) {
    throw "environment variable $passwordEnv is not defined"
}

$wheel = Get-ChildItem -Path $wheelhouseDir -Filter "*.whl"
if (-not $wheel) {
    throw "no wheels available under $wheelhouseDir"
}

python -m twine upload --repository $repository -u $username -p $password $wheel
