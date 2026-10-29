param([ValidateSet('Release','Debug')][string]$Configuration='Release', [switch]$Test)
$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$buildRoot=Join-Path $projectRoot 'build'
$pythonRuntime=Get-Command python -ErrorAction SilentlyContinue | Where-Object { $_.Source -notmatch 'WindowsApps' } | Select-Object -First 1 -ExpandProperty Source
if (-not $pythonRuntime) {
    $bundledPython=Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $bundledPython) { $pythonRuntime=$bundledPython }
}
$configureArgs=@('-S',$projectRoot,'-B',$buildRoot,"-DCMAKE_BUILD_TYPE=$Configuration")
if ($pythonRuntime) { $configureArgs+="-DPython3_EXECUTABLE=$pythonRuntime" }
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
$generatorLine=Select-String -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt') -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$'
$generatorName=if ($generatorLine) { $generatorLine.Matches[0].Groups[1].Value } else { '' }
if ($generatorName -like 'Visual Studio *') {
    & cmake --build $buildRoot --config $Configuration --parallel 4 -- /nodeReuse:false
} else {
    & cmake --build $buildRoot --config $Configuration --parallel 4
}
if ($LASTEXITCODE -ne 0) { throw 'C++ build failed.' }
if ($Test) {
    & ctest --test-dir $buildRoot -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
$executable=@((Join-Path $buildRoot "$Configuration\seamcarve.exe"),(Join-Path $buildRoot 'seamcarve.exe')) |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $executable) { throw 'The build completed but seamcarve.exe was not found.' }
Write-Host "Executable: $executable"
