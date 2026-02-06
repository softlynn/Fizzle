param(
    [string]$BuildDir = "build-x64",
    [ValidateSet("Debug","Release")]
    [string]$Config = "Release"
)

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Path
if (-not $cmake) { $cmake = "C:\Program Files\CMake\bin\cmake.exe" }
if (-not (Test-Path $cmake))
{
    Write-Error "cmake not found. Install CMake or add it to PATH."
    exit 1
}

& $cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $BuildDir --config $Config --target Fizzle
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $BuildDir --config $Config --target FizzleTests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --test-dir $BuildDir -C $Config --output-on-failure
exit $LASTEXITCODE
