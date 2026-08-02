[CmdletBinding()]
param(
    [string]$EmulatorRoot = $(if ($env:BBK9588_EMULATOR_ROOT) {
        $env:BBK9588_EMULATOR_ROOT
    } else {
        'E:\bbk9588-emulator-v0.1.5'
    }),
    [int]$Port = 8013,
    [switch]$ResetImage,
    [switch]$NoAutoLaunch,
    [switch]$NoOpenBrowser
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bdaName = [string]::Concat([char]0x4ED9, [char]0x5251, '1.bda')
$bda = Join-Path $repoRoot (Join-Path 'build' $bdaName)
$helper = Join-Path $repoRoot 'sdk\scripts\test_bda_in_emulator.ps1'

if (-not (Test-Path -LiteralPath $bda -PathType Leaf)) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) {
        throw '构建失败'
    }
}

$helperParameters = @{
    Bda = $bda
    EmulatorRoot = $EmulatorRoot
    Port = $Port
    NoOpenBrowser = $true
}
if ($ResetImage) { $helperParameters.ResetImage = $true }
if ($NoAutoLaunch) { $helperParameters.NoAutoLaunch = $true }

& $helper @helperParameters
if ($LASTEXITCODE -ne 0) {
    throw '模拟器部署失败'
}

if (-not $NoOpenBrowser) {
    Start-Process "http://127.0.0.1:$Port/"
}
