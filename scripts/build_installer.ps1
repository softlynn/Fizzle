param(
    [string]$InnoCompiler = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)

if (-not (Test-Path $InnoCompiler))
{
    Write-Error "Inno Setup compiler not found at $InnoCompiler"
    exit 1
}

if (-not (Test-Path "build-x64\Fizzle_artefacts\Release\Fizzle.exe"))
{
    Write-Error "Missing build-x64\\Fizzle_artefacts\\Release\\Fizzle.exe. Build release first."
    exit 1
}

& $InnoCompiler "installer\Fizzle.iss"
exit $LASTEXITCODE
