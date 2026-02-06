$ErrorActionPreference = "Stop"

$alreadyInstalled = $false
try {
    $soundDevices = Get-CimInstance Win32_SoundDevice -ErrorAction Stop
    foreach ($dev in $soundDevices) {
        if ($null -ne $dev.Name -and ($dev.Name -match "CABLE Input" -or $dev.Name -match "VB-Audio Virtual Cable")) {
            $alreadyInstalled = $true
            break
        }
    }
} catch {
    $alreadyInstalled = $false
}

if ($alreadyInstalled) {
    exit 0
}

$zip = Join-Path $env:TEMP "vbcable.zip"
$dir = Join-Path $env:TEMP "vbcable"

Invoke-WebRequest -UseBasicParsing -Uri "https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip" -OutFile $zip

if (Test-Path $dir)
{
    Remove-Item -Recurse -Force $dir
}

Expand-Archive -Path $zip -DestinationPath $dir -Force

$setup = Join-Path $dir "VBCABLE_Setup_x64.exe"
if (Test-Path $setup)
{
    Start-Process -FilePath $setup -ArgumentList "/S" -WindowStyle Hidden -Wait
}
