[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$ResourcePath,
    [string]$EmulatorRoot = $(if ($env:BBK9588_EMULATOR_ROOT) {
        $env:BBK9588_EMULATOR_ROOT
    } else {
        'E:\bbk9588-emulator-v0.1.5'
    }),
    [int]$Port = 8013,
    [string]$VideoPath,
    [switch]$ResetImage,
    [switch]$NoOpenBrowser
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = (Resolve-Path -LiteralPath $ResourcePath).Path
$emulator = (Resolve-Path -LiteralPath $EmulatorRoot).Path
$python = Join-Path $emulator 'python\python.exe'
$nand = Join-Path $emulator 'runtime\bda_test\bbk9588_nand.bin'
$baseUrl = "http://127.0.0.1:$Port"

if (Test-Path -LiteralPath $source -PathType Container) {
    & (Join-Path $PSScriptRoot 'check-resources.ps1') $source
} elseif ($VideoPath) {
    throw '-VideoPath 只能与传统资源目录一起使用；PAL9588.PAK 已包含视频。'
}

$deployParameters = @{
    EmulatorRoot = $emulator
    Port = $Port
    NoAutoLaunch = $true
    NoOpenBrowser = $true
}
if ($ResetImage) { $deployParameters.ResetImage = $true }
& (Join-Path $PSScriptRoot 'test-emulator.ps1') @deployParameters

try {
    Invoke-RestMethod -Method Post -Uri "$baseUrl/api/stop" `
        -TimeoutSec 45 | Out-Null
} catch {
    $body = @{ op = 'force-stop' } | ConvertTo-Json
    Invoke-RestMethod -Method Post -Uri "$baseUrl/api/command" `
        -ContentType 'application/json; charset=utf-8' -Body $body `
        -TimeoutSec 30 | Out-Null
}

$status = Invoke-RestMethod -Uri "$baseUrl/api/status" -TimeoutSec 10
if ($status.running) {
    $body = @{ op = 'force-stop' } | ConvertTo-Json
    Invoke-RestMethod -Method Post -Uri "$baseUrl/api/command" `
        -ContentType 'application/json; charset=utf-8' -Body $body `
        -TimeoutSec 30 | Out-Null
}

$importArguments = @(
    '-s', (Join-Path $PSScriptRoot 'import-resources-emulator.py'),
    $source, '--emulator-root', $emulator, '--nand', $nand
)
if ($VideoPath) {
    $video = (Resolve-Path -LiteralPath $VideoPath).Path
    $importArguments += @('--video-source', $video)
}
& $python @importArguments
if ($LASTEXITCODE -ne 0) { throw '资源导入失败' }

& (Join-Path $PSScriptRoot 'test-emulator.ps1') `
    -EmulatorRoot $emulator -Port $Port -NoOpenBrowser

if (-not $NoOpenBrowser) {
    Start-Process "$baseUrl/"
}
