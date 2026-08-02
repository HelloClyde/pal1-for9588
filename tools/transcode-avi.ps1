[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$SourcePath,
    [Parameter(Mandatory = $true, Position = 1)]
    [string]$OutputPath,
    [ValidateRange(64, 320)]
    [int]$Width = 288,
    [ValidateRange(40, 240)]
    [int]$Height = 180,
    [ValidateRange(5, 15)]
    [int]$Fps = 12,
    [ValidateRange(0, 64)]
    [int]$QualityError = 14,
    [ValidateRange(0, 64)]
    [int]$SkipError = 7,
    [ValidateSet(11025, 22050)]
    [int]$AudioRate = 11025,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
if (($Width % 4) -ne 0 -or ($Height % 4) -ne 0) {
    throw 'Width 和 Height 必须是 4 的倍数（MS Video 1 使用 4x4 块）。'
}

$source = (Resolve-Path -LiteralPath $SourcePath).Path
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "AVI 源目录不存在：$source"
}
$output = [IO.Path]::GetFullPath($OutputPath)
if ([string]::Equals($source.TrimEnd('\'), $output.TrimEnd('\'),
        [StringComparison]::OrdinalIgnoreCase)) {
    throw '输出目录不能与原始 PAL98 目录相同。'
}

$ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
$ffprobe = (Get-Command ffprobe -ErrorAction Stop).Source
$python = (Get-Command python -ErrorAction Stop).Source
$normalizer = Join-Path $PSScriptRoot 'prepare-pal-avi.py'
$encoder = Join-Path $PSScriptRoot 'encode-msvideo1.py'
New-Item -ItemType Directory -Force -Path $output | Out-Null

$inputs = @{}
Get-ChildItem -LiteralPath $source -File -Filter '*.avi' | ForEach-Object {
    $inputs[$_.Name.ToLowerInvariant()] = $_.FullName
}
$missing = 1..6 | Where-Object { -not $inputs.ContainsKey("$_.avi") }
if ($missing.Count -ne 0) {
    throw ('缺少 PAL98 过场：' + (($missing | ForEach-Object { "$_.AVI" }) -join ', '))
}

$results = @()
foreach ($number in 1..6) {
    $name = "$number.avi"
    $destination = Join-Path $output $name
    $temporary = Join-Path $output ".$number.tmp.avi"
    if ((Test-Path -LiteralPath $destination -PathType Leaf) -and -not $Force) {
        throw "输出已存在：$destination；需要覆盖时加 -Force。"
    }
    if (Test-Path -LiteralPath $temporary) {
        Remove-Item -LiteralPath $temporary -Force
    }

    try {
        & $python -s $encoder $inputs[$name] $temporary `
            --width $Width --height $Height --fps $Fps `
            --audio-rate $AudioRate --quality-error $QualityError `
            --skip-error $SkipError
        if ($LASTEXITCODE -ne 0) { throw "MS Video 1 高质量转码失败：$name" }

        & $python -s $normalizer $temporary
        if ($LASTEXITCODE -ne 0) { throw "AVI 兼容性处理失败：$name" }

        $probe = & $ffprobe -v error `
            -show_entries 'stream=codec_name,width,height,r_frame_rate,sample_rate,channels' `
            -of 'default=noprint_wrappers=1' $temporary
        $probeText = $probe -join "`n"
        if ($LASTEXITCODE -ne 0 -or
            $probeText -notmatch 'codec_name=msvideo1' -or
            $probeText -notmatch 'codec_name=pcm_u8' -or
            $probeText -notmatch "width=$Width" -or
            $probeText -notmatch "height=$Height" -or
            $probeText -notmatch "r_frame_rate=$Fps/1" -or
            $probeText -notmatch "sample_rate=$AudioRate" -or
            $probeText -notmatch 'channels=1') {
            throw "AVI 校验失败：$name"
        }
        & $ffmpeg -hide_banner -loglevel error -i $temporary -f null NUL
        if ($LASTEXITCODE -ne 0) { throw "AVI 完整解码校验失败：$name" }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
        $file = Get-Item -LiteralPath $destination
        $results += [PSCustomObject]@{
            File = $file.Name
            Bytes = $file.Length
            MiB = [Math]::Round($file.Length / 1MB, 2)
        }
    } finally {
        if (Test-Path -LiteralPath $temporary) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

$results | Format-Table -AutoSize
$total = ($results | Measure-Object -Property Bytes -Sum).Sum
Write-Host ("转码完成：{0}，共 {1:N2} MiB" -f $output, ($total / 1MB))
