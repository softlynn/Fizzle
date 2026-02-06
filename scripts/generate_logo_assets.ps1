param(
    [string]$SourceImage = "C:\Users\Alex2\Downloads\ChatGPT Image Feb 6, 2026, 01_55_10 AM.png",
    [string]$OutDir = "resources/icon",
    [double]$Zoom = 1.40
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $SourceImage)) {
    throw "Source image not found: $SourceImage"
}

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$pngPath = Join-Path $OutDir "app.png"
$icoPath = Join-Path $OutDir "app.ico"

$source = [System.Drawing.Image]::FromFile($SourceImage)
$canvasSize = 1024
$bmp = New-Object System.Drawing.Bitmap($canvasSize, $canvasSize, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$g.Clear([System.Drawing.Color]::Transparent)

$scale = [Math]::Min($canvasSize / $source.Width, $canvasSize / $source.Height) * $Zoom
$drawW = [int]([Math]::Round($source.Width * $scale))
$drawH = [int]([Math]::Round($source.Height * $scale))
$drawX = [int](($canvasSize - $drawW) / 2)
$drawY = [int](($canvasSize - $drawH) / 2)
$destRect = [System.Drawing.Rectangle]::new($drawX, $drawY, $drawW, $drawH)
$g.DrawImage($source, $destRect)
$bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = @()
foreach ($s in $sizes) {
    $scaled = New-Object System.Drawing.Bitmap($bmp, $s, $s)
    $ms = New-Object System.IO.MemoryStream
    $scaled.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $images += [pscustomobject]@{
        Size = $s
        Bytes = $ms.ToArray()
    }
    $ms.Dispose()
    $scaled.Dispose()
}

$fs = [System.IO.File]::Open($icoPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
$bw = New-Object System.IO.BinaryWriter($fs)
$bw.Write([UInt16]0)
$bw.Write([UInt16]1)
$bw.Write([UInt16]$images.Count)

$offset = 6 + (16 * $images.Count)
foreach ($img in $images) {
    $dimByte = if ($img.Size -ge 256) { [Byte]0 } else { [Byte]$img.Size }
    $bw.Write($dimByte)
    $bw.Write($dimByte)
    $bw.Write([Byte]0)
    $bw.Write([Byte]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]$img.Bytes.Length)
    $bw.Write([UInt32]$offset)
    $offset += $img.Bytes.Length
}

foreach ($img in $images) {
    $bw.Write($img.Bytes)
}

$bw.Flush()
$bw.Dispose()
$fs.Dispose()
$g.Dispose()
$bmp.Dispose()
$source.Dispose()

Write-Host "Generated:"
Write-Host " - $pngPath"
Write-Host " - $icoPath"
