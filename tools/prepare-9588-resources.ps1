[CmdletBinding()]
param(
    [string]$OriginalPack,
    [string]$Output,
    [string]$ReleaseBranch = 'release',
    [string]$Remote = 'origin'
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $repoRoot 'build'
$unpacked = Join-Path $buildRoot 'pal-original-unpacked'
$compactVideo = Join-Path $buildRoot 'pal98-compact'
$ready = Join-Path $buildRoot 'pal9588-ready'
if (-not $Output) { $Output = Join-Path $buildRoot 'PAL9588.PAK' }
$outputPath = [IO.Path]::GetFullPath($Output)
$python = (Get-Command python -ErrorAction Stop).Source
$releaseWorktree = $null
$worktreeAdded = $false

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

try {
    if (-not $OriginalPack) {
        if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
            throw '自动取得私有 release 资源包需要 Git。'
        }
        & git lfs version | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw '自动取得私有 release 资源包需要 Git LFS。'
        }
        & git fetch $Remote $ReleaseBranch
        if ($LASTEXITCODE -ne 0) { throw "无法取得 $Remote/$ReleaseBranch" }

        $releaseWorktree = Join-Path (
            [IO.Path]::GetTempPath()
        ) ("pal1-9588-release-$PID")
        if (Test-Path -LiteralPath $releaseWorktree) {
            throw "临时 worktree 已存在：$releaseWorktree"
        }
        & git worktree add --detach $releaseWorktree "$Remote/$ReleaseBranch"
        if ($LASTEXITCODE -ne 0) { throw '无法创建 release 临时 worktree' }
        $worktreeAdded = $true
        & git -C $releaseWorktree lfs pull $Remote
        if ($LASTEXITCODE -ne 0) { throw 'Git LFS 私有资源下载失败' }
        $OriginalPack = Join-Path $releaseWorktree 'release-assets\PAL-ORIGINAL.PAK'
    }

    $originalPath = (Resolve-Path -LiteralPath $OriginalPack).Path
    Reset-BuildDirectory $unpacked
    Reset-BuildDirectory $compactVideo
    Reset-BuildDirectory $ready

    & $python -S (Join-Path $PSScriptRoot 'palpak.py') `
        extract $originalPath $unpacked
    if ($LASTEXITCODE -ne 0) { throw '原始资源包展开失败' }
    & (Join-Path $PSScriptRoot 'check-resources.ps1') $unpacked

    & (Join-Path $PSScriptRoot 'transcode-avi.ps1') `
        $unpacked $compactVideo
    if ($LASTEXITCODE -ne 0) { throw 'AVI 离线转换失败' }

    Get-ChildItem -LiteralPath $unpacked -File | Where-Object {
        $_.Extension -ine '.avi'
    } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (
            Join-Path $ready $_.Name
        )
    }
    1..6 | ForEach-Object {
        $video = Join-Path $compactVideo "$_.avi"
        if (-not (Test-Path -LiteralPath $video -PathType Leaf)) {
            throw "转换后视频缺失：$video"
        }
        Copy-Item -LiteralPath $video -Destination (Join-Path $ready "$_.avi")
    }

    & (Join-Path $PSScriptRoot 'check-resources.ps1') $ready
    & $python -S (Join-Path $PSScriptRoot 'palpak.py') pack $ready $outputPath
    if ($LASTEXITCODE -ne 0) { throw '9588 资源包创建失败' }
    & $python -S (Join-Path $PSScriptRoot 'palpak.py') verify $outputPath
    if ($LASTEXITCODE -ne 0) { throw '9588 资源包校验失败' }

    $file = Get-Item -LiteralPath $outputPath
    $hash = Get-FileHash -LiteralPath $outputPath -Algorithm SHA256
    Write-Host "9588 资源包：$($file.FullName)"
    Write-Host "大小：$($file.Length) bytes"
    Write-Host "SHA-256：$($hash.Hash)"
} finally {
    if ($worktreeAdded) {
        & git worktree remove --force $releaseWorktree
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "临时 release worktree 未能自动移除：$releaseWorktree"
        }
    }
}
