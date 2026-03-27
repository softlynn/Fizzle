param(
    [string]$InnoCompiler = ""
)

if (-not $InnoCompiler)
{
    $innoCandidates = @(
        (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Path,
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    ) | Where-Object { $_ -and (Test-Path $_) }

    $InnoCompiler = $innoCandidates | Select-Object -First 1
}

if (-not (Test-Path $InnoCompiler))
{
    Write-Error "Inno Setup compiler not found. Install Inno Setup or pass -InnoCompiler."
    exit 1
}

if (-not (Test-Path "build-x64\Fizzle_artefacts\Release\Fizzle.exe"))
{
    Write-Error "Missing build-x64\\Fizzle_artefacts\\Release\\Fizzle.exe. Build release first."
    exit 1
}

& $InnoCompiler "installer\Fizzle.iss"
exit $LASTEXITCODE
