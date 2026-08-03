# SDLPAL for BBK 9588

把开源 SDLPAL 原生移植到步步高 9588（JZ4740/MIPS32）学习机。SDK 和
SDLPAL 都以固定提交的 submodule 引入；发布产物是固件直接执行的 BDA，模拟器只用于
开发和回归测试。

![PAL1 for BBK 9588 实机画面预览](docs/images/preview-collage.png)

上图来自同一份 BDA 在 bbk9588-emulator v0.1.5 中实际运行的六个场景：AVI
过场、标题菜单、两段剧情对话、自由行动地图和战斗。画面按正常横屏方向校正，
仅裁掉模拟器外框、等比缩放后拼接，没有重绘游戏内容。

## 快速开始

以下流程适用于有该私有仓库和私有 `release` 分支访问权的开发者。需要 Git、Git
LFS、PowerShell、Python 3.10+、FFmpeg，以及默认位于
`E:\bbk9588-emulator-v0.1.5` 的模拟器。

```powershell
git lfs install
git clone --recurse-submodules git@github.com:HelloClyde/pal1-for9588.git
cd pal1-for9588

# 从私有 release 分支取原版资源，生成主包和可选视频包
.\tools\prepare-9588-resources.ps1

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

## 当前状态

- 完整 SDLPAL 游戏、地图、脚本、UI、战斗和存档核心参与构建。
- 320×200、8 位调色板画面以原始比例居中显示；横屏画面逆时针转 90°
  写入 240×320 竖屏帧缓冲，AVI 使用同一方向。
- 每帧通过 SDK 验证的 direct framebuffer API 一次复制到 LCD 扫描缓冲；
  SDK 拒绝当前固件布局或提交失败时，自动回退到原有 picture 渲染路径。
- direct framebuffer 模式下实体键直接读取 SDK input packet，运行期不再进入慢速
  窗口消息队列；同时长按 Enter + Esc 1.5 秒可安全退出并在关闭阶段处理 detach。
- 已接入 22050 Hz、16-bit、单声道 PCM：RIX/OPL 音乐和 DOS VOC/Win95 WAV
  音效由 SDLPAL 解码、混音后送入 SDK 队列。
- 已接入 SDLPAL 开源 Microsoft Video 1 解码器；六段 PAL98 AVI 在电脑端由质量可控
  的 4×4 块编码器保留原始 288×180 分辨率离线转码，设备端直接播放视频和 PCM
  音轨，不承担 H.264/MPEG-4 软解负担。
- BDA 从 `PAL9588.PAK` 随机读取 MKF、文本和字体，并按需从可选的
  `PALVIDEO.PAK` 读取 AVI；两个包都不会先展开到 NAND。
- 模拟器已验证主包启动、可选 AVI 与音频播放、存档重启读回、自动战斗胜利和六段视频
  回归。真机性能、按键手感、扬声器表现及完整剧情仍需实体 9588 复测。

## 资源工作流

### 私有原版包

`main` 和正常开发分支不包含商业资源。用户合法购买的原版数据只保存在同一私有
GitHub 仓库的 `release` 分支中：

```text
release-assets/PAL-ORIGINAL.PAK
```

该文件由 Git LFS 管理，是 DOS 版游戏数据和 PAL98 六段原始 AVI 合成的一个包。
`release` 分支不得合并进 `main`，仓库改为公开前必须先删除该分支和对应 LFS 对象；
资源版权不因存入私有仓库而改变。

若没有 `release` 分支访问权，可以直接从自己安装的 Steam 经典版创建本地原版包：

```powershell
.\tools\pack-original-resources.ps1 `
    -DosPath 'D:\Program Files (x86)\Steam\steamapps\common\PAL\PAL_DOS' `
    -Pal98Path 'D:\Program Files (x86)\Steam\steamapps\common\PAL\PAL98'

.\tools\prepare-9588-resources.ps1 `
    -OriginalPack .\build\PAL-ORIGINAL.PAK
```

所有 staging 目录和资源输出都位于被 Git 忽略的 `build\` 下。

### 设备资源包

`prepare-9588-resources.ps1` 会执行以下操作：

1. 从私有 `release` 分支通过 Git LFS 取得 `PAL-ORIGINAL.PAK`，或使用
   `-OriginalPack` 指定本地包；
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

### GitHub Actions

`Build BDA` workflow 会在 `main` 推送和手动触发时，
使用固定的 submodule 提交构建 `仙剑1.bda`，并将 BDA 与 SHA-256 校验文件保存为
30 天的 `pal1-bda` Artifact。手动运行时勾选 `build_resources`，还会从私有
`release` 分支通过 Git LFS 取得 `PAL-ORIGINAL.PAK`，生成 30 天的
`pal1-resources` Artifact，其中包含 `PAL9588.PAK`、`PALVIDEO.PAK` 和各自的
SHA-256 校验文件。

推送 `v*` 标签时会自动执行以上两条构建链，创建或更新同名 GitHub Release，并
上传 BDA、两个资源包和三个独立校验文件。普通 Pull Request 不触发该 workflow；
资源 job 也不会缓存任何原始或转换后的游戏数据。

GitHub 会移除 Release 附件文件名中的中文字符，因此发布时使用可稳定下载的
`PAL1-9588-<版本>.bda`，并将附件显示标签设为 `仙剑1.bda`；BDA 内部应用名称和
本地构建产物仍为 `仙剑1` / `仙剑1.bda`。

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
许可证，只能由拥有合法副本且获准访问私有仓库的人使用。

参与开发前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。
