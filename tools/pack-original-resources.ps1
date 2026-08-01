[CmdletBinding()]
param(
    [string]$DosPath = 'D:\Program Files (x86)\Steam\steamapps\common\PAL\PAL_DOS',
    [string]$Pal98Path = 'D:\Program Files (x86)\Steam\steamapps\common\PAL\PAL98',
    [string]$Output
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $repoRoot 'build'
$staging = Join-Path $buildRoot 'pal-original-staging'
if (-not $Output) { $Output = Join-Path $buildRoot 'PAL-ORIGINAL.PAK' }
$outputPath = [IO.Path]::GetFullPath($Output)
$dos = (Resolve-Path -LiteralPath $DosPath).Path
$pal98 = (Resolve-Path -LiteralPath $Pal98Path).Path
$python = (Get-Command python -ErrorAction Stop).Source

function Reset-BuildDirectory([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith(
        $prefix, [StringComparison]::OrdinalIgnoreCase
    )) {
        throw "拒绝清理 build 之外的目录：$resolved"
    }
    if (Test-Path -LiteralPath $resolved) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
    New-Item -ItemType Directory -Path $resolved -Force | Out-Null
}

$coreFiles = @(
    '0.RPG', 'ABC.MKF', 'BALL.MKF', 'DATA.MKF', 'F.MKF', 'FBP.MKF',
    'FIRE.MKF', 'GOP.MKF', 'MAP.MKF', 'MGO.MKF', 'MIDI.MKF',
    'MUS.MKF', 'PAT.MKF', 'RGM.MKF', 'RNG.MKF', 'SSS.MKF',
    'VOC.MKF', 'M.MSG', 'WORD.DAT', 'WOR16.ASC', 'wor16.FON'
)
$videoFiles = 1..6 | ForEach-Object { "$_.AVI" }

Reset-BuildDirectory $staging
foreach ($name in $coreFiles) {
    $source = Join-Path $dos $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "DOS 原版资源缺失：$source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $staging $name)
}
foreach ($name in $videoFiles) {
    $source = Join-Path $pal98 $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "PAL98 原版视频缺失：$source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $staging $name)
}

& (Join-Path $PSScriptRoot 'check-resources.ps1') $staging
& $python -S (Join-Path $PSScriptRoot 'palpak.py') pack $staging $outputPath
if ($LASTEXITCODE -ne 0) { throw '原始资源包创建失败' }
& $python -S (Join-Path $PSScriptRoot 'palpak.py') verify $outputPath
if ($LASTEXITCODE -ne 0) { throw '原始资源包校验失败' }

$file = Get-Item -LiteralPath $outputPath
$hash = Get-FileHash -LiteralPath $outputPath -Algorithm SHA256
Write-Host "原始资源包：$($file.FullName)"
Write-Host "大小：$($file.Length) bytes"
Write-Host "SHA-256：$($hash.Hash)"
