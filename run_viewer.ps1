param([int]$Port=8765, [string]$Python='')
$ErrorActionPreference='Stop'
$viewerCandidates=@((Join-Path $PSScriptRoot 'build\Release\seamcarve.exe'),(Join-Path $PSScriptRoot 'build\seamcarve.exe'))
$viewerExe=$viewerCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $viewerExe) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Test
    $viewerExe=$viewerCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $viewerExe) { throw 'seamcarve.exe was not found after building the project.' }
if (-not $Python) {
    $Python=Get-Command python -ErrorAction SilentlyContinue | Where-Object { $_.Source -notmatch 'WindowsApps' } | Select-Object -First 1 -ExpandProperty Source
}
if (-not $Python) {
    $candidate=Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $candidate) { $Python=$candidate }
}
if (-not $Python) { throw 'Python 3 is required for the viewer; pass -Python with its executable path.' }
& $Python (Join-Path $PSScriptRoot 'web\server.py') --exe $viewerExe --port $Port
