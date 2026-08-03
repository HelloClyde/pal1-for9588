# SDLPAL for BBK 9588

把开源 SDLPAL 原生移植到步步高 9588（JZ4740/MIPS32）学习机。SDK 和
SDLPAL 都以固定提交的 submodule 引入；发布产物是固件直接执行的 BDA，模拟器只用于
开发和回归测试。

![PAL1 for BBK 9588 实机画面预览](docs/images/preview-collage.png)

上图来自同一份 BDA 在 bbk9588-emulator v0.1.5 中实际运行的六个场景：AVI
过场、标题菜单、两段剧情对话、自由行动地图和战斗。画面按正常横屏方向校正，
仅裁掉模拟器外框、等比缩放后拼接，没有重绘游戏内容。

## 快速开始

以下流程适用于拥有合法《仙剑奇侠传》经典版资源的用户。需要 Git、PowerShell、
Python 3.10+ 和 FFmpeg；只有运行模拟器测试时才需要
`bbk9588-emulator v0.1.5`。

```powershell
git clone --recurse-submodules https://github.com/HelloClyde/pal1-for9588.git
cd pal1-for9588

# Steam 经典版安装目录中应同时包含 PAL_DOS 和 PAL98
$palRoot = 'D:\Program Files (x86)\Steam\steamapps\common\PAL'

# 从自己的原版文件生成 9588 主资源包和可选视频包
.\tools\pack-original-resources.ps1 `
    -DosPath "$palRoot\PAL_DOS" -Pal98Path "$palRoot\PAL98"
.\tools\prepare-9588-resources.ps1 `
    -OriginalPack .\build\PAL-ORIGINAL.PAK

# 构建原生 MIPS BDA，并把主包 + 视频包放入隔离 NAND 启动
.\tools\build.ps1
.\tools\test-with-resources.ps1 .\build\PAL9588.PAK `
    -VideoPackage .\build\PALVIDEO.PAK -ResetImage
```

输出文件：

- `build\仙剑1.bda`：9588 原生程序；
- `build\PAL9588.PAK`：设备端必需的游戏主资源包；
- `build\PALVIDEO.PAK`：包含六段 AVI 的可选视频资源包。

真机安装时，将 `仙剑1.bda` 放到 `A:\应用\程序\`，将 `PAL9588.PAK` 放到
`A:\应用\数据\PAL\`。需要 PAL98 视频时再把 `PALVIDEO.PAK` 放到同一目录；不安装、
删除或读取失败都会被安全忽略，游戏改用 SDLPAL 原有的 DOS RNG 片头与结局动画。
存档、配置和诊断日志仍以普通可写文件保存在该目录。

## 操作

- 实体方向键移动，Enter 调查/确认，Esc 打开菜单/返回；Enter + Esc 长按 1.5 秒退出。
- 在自由行动地图上轻触目标位置会自动绕开地图障碍和阻挡物前往最近可达点；轻触可调查
  的 NPC 或物件，会在走近后自动面向并调查。
- 自动行走期间按任意实体方向键、Enter 或 Esc 会立即取消；战斗、菜单、对话和切换场景
  期间不会启动寻路。滑动不视为点击，以免误触。

## 当前状态

- 完整 SDLPAL 游戏、地图、脚本、UI、战斗和存档核心参与构建。
- 320×200、8 位调色板画面以原始比例居中显示；横屏画面逆时针转 90°
  写入 240×320 竖屏帧缓冲，AVI 使用同一方向。
- 每帧通过 SDK 验证的 direct framebuffer API 一次复制到 LCD 扫描缓冲；
  SDK 拒绝当前固件布局或提交失败时，自动回退到原有 picture 渲染路径。
- direct framebuffer 模式下实体键直接读取 SDK input packet，运行期不再进入慢速
  窗口消息队列；同时长按 Enter + Esc 1.5 秒可安全退出并在关闭阶段处理 detach。
- 触摸同样从 SDK 原始事件流读取，不恢复窗口消息泵；坐标按逆时针横屏方向反算，地图
  点击使用 SDLPAL 原生地图与事件物件碰撞判断执行轻量自动寻路。
- 已接入 22050 Hz、16-bit、单声道 PCM：RIX/OPL 音乐和 DOS VOC/Win95 WAV
  音效由 SDLPAL 解码、混音后送入 SDK 队列。
- 已接入 SDLPAL 开源 Microsoft Video 1 解码器；六段 PAL98 AVI 在电脑端由质量可控
  的 4×4 块编码器保留原始 288×180 分辨率离线转码，设备端直接播放视频和 PCM
  音轨，不承担 H.264/MPEG-4 软解负担。
- BDA 从 `PAL9588.PAK` 随机读取 MKF、文本和字体，并按需从可选的
  `PALVIDEO.PAK` 读取 AVI；两个包都不会先展开到 NAND。
- 模拟器已验证主包启动、可选 AVI 与音频播放、存档重启读回、自动战斗胜利和六段视频
  回归。真机性能、按键手感、扬声器表现及完整剧情仍需实体 9588 复测。

## 从原版仙剑 98 制作资源包

Steam《仙剑奇侠传》经典版安装目录通常同时包含 `PAL_DOS` 和 `PAL98`：前者提供
游戏数据、音乐与音效，后者提供六段 98 版 AVI。其他合法版本也可以使用，只要这
两个目录中的文件完整。将下面的 `$palRoot` 改成自己的安装位置：

```powershell
$palRoot = 'D:\Program Files (x86)\Steam\steamapps\common\PAL'

.\tools\pack-original-resources.ps1 `
    -DosPath "$palRoot\PAL_DOS" `
    -Pal98Path "$palRoot\PAL98"

.\tools\prepare-9588-resources.ps1 `
    -OriginalPack .\build\PAL-ORIGINAL.PAK
```

第一条命令只在被 Git 忽略的 `build\` 目录生成转换用中间文件；第二条命令直接
生成设备需要的两个资源包。脚本不会联网下载、上传或提交任何原版游戏文件。

`prepare-9588-resources.ps1` 会执行以下操作：

1. 校验用户从自己游戏目录生成的本地中间包；
2. 校验原版资源，使用 FFmpeg 解出 RGB555 帧和 PCM，再由本仓库的高质量 MS Video 1
   块编码器将 1–6 号 AVI 转为 288×180、12 fps、11025 Hz、8-bit mono PCM；
3. 把核心资源打成必需的 `build\PAL9588.PAK`，把六段转码视频打成可选的
   `build\PALVIDEO.PAK`，并分别逐项校验 CRC32。

当前主包包含 21 个条目（含新游戏初始模板 `0.RPG`），约 25.91 MiB；视频包包含
6 个条目，约 31.60 MiB。PAK 采用未压缩、16-byte 对齐的目录格式，换取低内存和
真正的随机读取；BDA 的
`access`/`fopen`/`fread`/`fseek`/`ftell`
会把两个包内的条目作为普通只读文件暴露给未修改的 SDLPAL。视频包不存在时不会
尝试打开不存在的文件，避免影响已打开的主包句柄；写入始终落到普通 NAND 文件。
格式见 [docs/palpak-format.md](docs/palpak-format.md)。

可单独使用打包工具：

```powershell
python -S .\tools\palpak.py list .\build\PAL9588.PAK
python -S .\tools\palpak.py verify .\build\PAL9588.PAK
python -S .\tools\palpak.py verify .\build\PALVIDEO.PAK
python -S .\tools\palpak.py extract .\build\PAL9588.PAK .\build\unpacked
```

## 构建

首次构建会使用 SDK 脚本安装并校验 MIPS 工具链：

```powershell
.\tools\build.ps1
```

已有工具链时可设置：

```powershell
$env:BDA_TOOLCHAIN_PREFIX = 'C:\toolchains\mips\bin\mipsel-none-elf-'
.\tools\build.ps1 -SkipToolchainSetup
```

输出同时包含 BDA、ELF、map 和反汇编文件。两个 submodule 必须停在仓库记录的提交；
不要直接改用 SDLPAL 最新主线。

## 模拟器测试

只有 BDA、没有游戏资源的启动冒烟测试：

```powershell
.\tools\test-emulator.ps1 -ResetImage
```

使用主包和可选视频包测试：

```powershell
.\tools\test-with-resources.ps1 .\build\PAL9588.PAK `
    -VideoPackage .\build\PALVIDEO.PAK -ResetImage
```

验证不安装视频包时的 DOS 动画回退路径：

```powershell
.\tools\test-with-resources.ps1 .\build\PAL9588.PAK -ResetImage
```

脚本仍兼容传统散文件目录，便于诊断和资源比对：

```powershell
.\tools\test-with-resources.ps1 D:\Games\PAL -ResetImage
```

更详细的结构和实测记录见 [docs/porting.md](docs/porting.md) 与
[docs/testing.md](docs/testing.md)。

## 许可证与来源

移植代码采用 GPL-3.0-or-later。SDLPAL 完整许可见
`third_party/sdlpal/gpl.txt`，SDK 许可见 `sdk/LICENSE`。商业游戏资源不属于本项目
许可证，只能由拥有合法副本的用户自行提取和使用。

参与开发前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。
