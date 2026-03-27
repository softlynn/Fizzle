param(
    [string]$BuildDir = "build-x64",
    [ValidateSet("Debug","Release")]
    [string]$Config = "Release",
    [string]$Generator = ""
)

$cmakeCandidates = @(
    (Get-Command cmake -ErrorAction SilentlyContinue).Path,
    "C:\Program Files\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) | Where-Object { $_ -and (Test-Path $_) }

$cmake = $cmakeCandidates | Select-Object -First 1
if (-not $cmake)
{
    Write-Error "cmake not found. Install CMake or add it to PATH."
    exit 1
}

$ctest = Join-Path (Split-Path $cmake -Parent) "ctest.exe"
if (-not (Test-Path $ctest)) { $ctest = "ctest" }

if (-not $Generator)
{
    $cmakeHelp = & $cmake --help
    if ($cmakeHelp -match "Visual Studio 18 2026")
    {
        $Generator = "Visual Studio 18 2026"
    }
    elseif ($cmakeHelp -match "Visual Studio 17 2022")
    {
        $Generator = "Visual Studio 17 2022"
    }
    else
    {
        Write-Error "No supported Visual Studio generator found via cmake --help."
        exit 1
    }
}

& $cmake --fresh -S . -B $BuildDir -G $Generator -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $BuildDir --config $Config --target Fizzle
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $BuildDir --config $Config --target FizzleTests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $ctest --test-dir $BuildDir -C $Config --output-on-failure
exit $LASTEXITCODE
