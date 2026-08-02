[CmdletBinding()]
param(
    [switch]$SkipToolchainSetup,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sdkRoot = Join-Path $repoRoot 'sdk'
$sdlpalRoot = Join-Path $repoRoot 'third_party\sdlpal'
$buildRoot = Join-Path $repoRoot 'build'
$objectRoot = Join-Path $buildRoot 'obj'

foreach ($required in @(
    (Join-Path $sdkRoot 'sdk\include\bda_sdk.h'),
    (Join-Path $sdlpalRoot 'main.c')
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw '缺少 submodule；请运行 git submodule update --init --recursive'
    }
}

function Find-ToolPrefix {
    if ($env:BDA_TOOLCHAIN_PREFIX) {
        return $env:BDA_TOOLCHAIN_PREFIX
    }
    $candidates = @(
        (Join-Path $sdkRoot '.toolchain\bin\mipsel-none-elf-'),
        (Get-ChildItem (Join-Path $sdkRoot '.toolchain') -Directory `
            -Filter 'g++-mipsel-none-elf-*' -ErrorAction SilentlyContinue |
            ForEach-Object {
                Join-Path $_.FullName 'bin\mipsel-none-elf-'
            })
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and
            (Test-Path -LiteralPath ($candidate + 'gcc.exe') -PathType Leaf)) {
            return $candidate
        }
    }
    return $null
}

$prefix = Find-ToolPrefix
if (-not $prefix -and -not $SkipToolchainSetup) {
    & (Join-Path $sdkRoot 'scripts\setup_toolchain.ps1')
    if ($LASTEXITCODE -ne 0) {
        throw 'MIPS 工具链安装失败'
    }
    $prefix = Find-ToolPrefix
}
if (-not $prefix) {
    throw '找不到 MIPS 工具链；请去掉 -SkipToolchainSetup 或设置 BDA_TOOLCHAIN_PREFIX'
}

function Resolve-Tool([string]$Name) {
    foreach ($suffix in @('.exe', '')) {
        $candidate = $prefix + $Name + $suffix
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    throw "找不到工具：$($prefix)$Name"
}

function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "命令失败 ($LASTEXITCODE)：$Executable $($Arguments -join ' ')"
    }
}

if ($Clean -and (Test-Path -LiteralPath $buildRoot)) {
    $resolvedBuild = [IO.Path]::GetFullPath($buildRoot)
    $resolvedRepo = [IO.Path]::GetFullPath($repoRoot).TrimEnd('\') + '\'
    if (-not $resolvedBuild.StartsWith(
        $resolvedRepo, [StringComparison]::OrdinalIgnoreCase
    ) -or [IO.Path]::GetFileName($resolvedBuild) -ne 'build') {
        throw "拒绝清理越界目录：$resolvedBuild"
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $objectRoot | Out-Null

$gcc = Resolve-Tool 'gcc'
$gxx = Resolve-Tool 'g++'
$objcopy = Resolve-Tool 'objcopy'
$objdump = Resolve-Tool 'objdump'
$python = (Get-Command python -ErrorAction Stop).Source

$portSources = @(
    'src\runtime\entry.S',
    'src\runtime\setjmp.S',
    'src\runtime\startup.c',
    'src\runtime\cxx_runtime.cpp',
    'src\libc\runtime.c',
    'src\compat\mini_sdl.c',
    'src\port\platform.c',
    'src\port\audio_stub.c',
    'src\port\avi_9588.c',
    'src\port\pal_utils.c',
    'src\port\regression.c',
    'src\port\bda_main.c'
)

$engineNames = @(
    'audio.c', 'aviplay.c',
    'battle.c', 'ending.c', 'fight.c', 'font.c', 'game.c',
    'global.c', 'input.c', 'itemmenu.c', 'magicmenu.c', 'main.c',
    'map.c', 'midi.c', 'palette.c', 'palcfg.c', 'palcommon.c', 'play.c',
    'res.c', 'rngplay.c', 'scene.c', 'script.c', 'text.c',
    'resampler.c', 'sound.c', 'ui.c', 'uibattle.c', 'uigame.c',
    'util.c', 'video.c', 'yj1.c'
)
$adplugCNames = @('fmopl.c')
$audioCppNames = @('rixplay.cpp')
$adplugCppNames = @(
    'binfile.cpp', 'binio.cpp', 'dbemuopl.cpp', 'dbopl.cpp',
    'demuopl.cpp', 'dosbox_opl.cpp', 'emuopl.cpp', 'fprovide.cpp',
    'player.cpp', 'rix.cpp', 'surroundopl.cpp'
)
$sources = @()
foreach ($source in $portSources) {
    $sources += Join-Path $repoRoot $source
}
foreach ($name in $engineNames) {
    $sources += Join-Path $sdlpalRoot $name
}
foreach ($name in $adplugCNames) {
    $sources += Join-Path (Join-Path $sdlpalRoot 'adplug') $name
}
foreach ($name in $audioCppNames) {
    $sources += Join-Path $sdlpalRoot $name
}
foreach ($name in $adplugCppNames) {
    $sources += Join-Path (Join-Path $sdlpalRoot 'adplug') $name
}

$common = @(
    '-EL', '-march=mips32', '-msoft-float',
    '-mno-abicalls', '-G0', '-fno-pic',
    '-O2', '-ffreestanding', '-fno-builtin',
    '-ffunction-sections', '-fdata-sections',
    '-fsigned-char', '-fno-strict-aliasing',
    '-I', (Join-Path $repoRoot 'src\libc\include'),
    '-I', (Join-Path $repoRoot 'src\compat'),
    '-I', (Join-Path $repoRoot 'src\port'),
    '-I', (Join-Path $sdkRoot 'sdk\include'),
    '-I', $sdlpalRoot,
    '-DPAL_CLASSIC',
    '-DPAL_HAS_PLATFORM_SPECIFIC_UTILS=1'
)

$objects = @()
foreach ($source in $sources) {
    $relative = [IO.Path]::GetRelativePath($repoRoot, $source)
    $objectName = ($relative -replace '[\\/:]', '_') -replace '\.(c|cpp|S)$', '.o'
    $object = Join-Path $objectRoot $objectName
    $extension = [IO.Path]::GetExtension($source)
    if ($extension -ceq '.S') {
        $compiler = $gcc
        $language = @('-x', 'assembler-with-cpp')
    } elseif ($extension -ieq '.cpp') {
        $compiler = $gxx
        $language = @(
            '-std=gnu++11', '-fno-exceptions', '-fno-rtti',
            '-fno-threadsafe-statics', '-fno-use-cxa-atexit'
        )
    } else {
        $compiler = $gcc
        $language = @('-std=gnu11')
    }
    if ($source -ieq (Join-Path $sdlpalRoot 'main.c')) {
        $language += '-Dmain=sdlpal_main'
    }
    if ($source -ieq (Join-Path $sdlpalRoot 'aviplay.c')) {
        $language += '-DPAL_PlayAVI=PAL9588_InternalPlayAVI'
        $language += '-DVIDEO_DrawSurfaceToScreen=PAL9588_DrawAVISurface'
    }
    if ($source -ieq (Join-Path $sdlpalRoot 'game.c')) {
        $language += '-DPAL_GameMain=PAL9588_InternalGameMain'
    }
    Write-Host "CC $relative"
    Invoke-Checked $compiler @($common + $language + @(
        '-c', $source, '-o', $object
    ))
    $objects += $object
}

$elf = Join-Path $buildRoot 'Pal1-9588.elf'
$raw = Join-Path $buildRoot 'Pal1-9588.bin'
$map = Join-Path $buildRoot 'Pal1-9588.map'
$dump = Join-Path $buildRoot 'Pal1-9588.dump.txt'
$appTitle = [string]::Concat([char]0x4ED9, [char]0x5251, '1')
$bda = Join-Path $buildRoot ($appTitle + '.bda')
$icon = Join-Path $repoRoot 'assets\pal1-icon.png'
$linker = Join-Path $repoRoot 'linker\bda.ld'

if (-not (Test-Path -LiteralPath $icon -PathType Leaf)) {
    throw "缺少 BDA 图标：$icon"
}

$linkArguments = @(
    '-EL', '-march=mips32', '-msoft-float',
    '-mno-abicalls', '-G0', '-fno-pic',
    '-nostdlib', '-Wl,--build-id=none', '-Wl,--gc-sections',
    "-Wl,-T,$linker", "-Wl,-Map,$map", '-o', $elf
)
Write-Host 'LD Pal1-9588.elf'
Invoke-Checked $gxx ($linkArguments + $objects + @('-lgcc'))
Invoke-Checked $objcopy @('-O', 'binary', $elf, $raw)
& $objdump -d -h $elf | Out-File -LiteralPath $dump -Encoding ascii
if ($LASTEXITCODE -ne 0) { throw 'objdump 失败' }

Invoke-Checked $python @(
    '-s',
    (Join-Path $PSScriptRoot 'pack-prelinked.py'),
    $raw,
    '--sdk', $sdkRoot,
    '--output', $bda,
    '--title', $appTitle,
    '--icon', $icon
)

Write-Host "ELF: $elf"
Write-Host "BDA: $bda"
