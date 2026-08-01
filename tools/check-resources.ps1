[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $Path).Path
$required = @(
    'abc.mkf', 'ball.mkf', 'data.mkf', 'f.mkf', 'fbp.mkf',
    'fire.mkf', 'gop.mkf', 'map.mkf', 'mgo.mkf', 'mus.mkf',
    'pat.mkf', 'rgm.mkf', 'rng.mkf', 'sss.mkf', 'word.dat'
)
$missing = @()

foreach ($name in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $name) -PathType Leaf)) {
        $missing += $name
    }
}

if ($missing.Count -ne 0) {
    Write-Error ("资源不完整，缺少：" + ($missing -join ', '))
    exit 1
}

if (-not (Test-Path -LiteralPath (Join-Path $root 'm.msg') -PathType Leaf)) {
    Write-Warning '未找到 m.msg；Windows 95 版可能不需要，DOS 版通常需要。'
}

if (-not (Test-Path -LiteralPath (Join-Path $root 'voc.mkf') -PathType Leaf) -and
    -not (Test-Path -LiteralPath (Join-Path $root 'sounds.mkf') -PathType Leaf)) {
    Write-Error '缺少音效资源：需要 voc.mkf 或 sounds.mkf。'
    exit 1
}

Write-Host "资源检查通过：$root"
