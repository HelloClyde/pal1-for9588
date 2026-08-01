# SDLPAL for BBK 9588

把开源的 SDLPAL 引擎移植到步步高 9588（JZ4740/MIPS32）学习机。项目是独立
Git 仓库，SDK 和 SDLPAL 都以固定提交的 submodule 引入，方便复现和后续开源。

## 当前状态

- 320×200、8 位调色板画面以原始比例居中显示，并顺时针旋转到 240×320 屏幕；
  模拟器横屏显示方向已用商标画面校验。
- 实体方向键按横屏方向映射；`Enter` 为确认/搜索，`Esc` 为菜单/取消。
- 已接入 SDK 的窗口、事件、文件、计时、内存和 RGB565 图像接口。
- 完整 SDLPAL 游戏核心参与构建。
- 已接入 22050 Hz、16-bit、单声道 PCM：RIX/OPL 背景音乐及 DOS VOC、Win95
  WAV 音效由 SDLPAL 解码、混音后送入 SDK 队列；低算力配置使用 DBOPL 和
  2048-sample 缓冲。
- 已接入原生 AVI 过场：设备端使用 SDLPAL 的开源 Microsoft Video 1 解码器，
  直接把 RGB555 帧旋转提交到 9588，PCM 音轨转换后进入同一音频队列；文件缺失时
  仍自动使用原版 RNG 过场。
- 模拟器已完成存档写入、重新启动后读回、单敌人战斗胜利，以及 1–6 号 AVI
  逐帧播放回归；这些测试入口由 `PORTTEST.*` 标记显式启用，不影响正常启动。

## 取得源码

```powershell
git clone --recurse-submodules https://github.com/HelloClyde/pal1-for9588.git
cd pal1-for9588
git submodule update --init --recursive
```

两个 submodule 都必须停在仓库记录的提交。不要直接使用 SDLPAL 最新主线：本移植以
仍包含 Dingux/JZ4740 支持的最后一个历史版本为兼容基线。

## 游戏资源

本仓库不提供《仙剑奇侠传》的商业数据。请只使用自己合法拥有的 DOS 或 Windows 95
版资源，并复制到模拟器/真机的：

```text
A:\应用\数据\PAL\
```

最低需要：

```text
abc.mkf  ball.mkf  data.mkf  f.mkf    fbp.mkf  fire.mkf
gop.mkf  map.mkf   mgo.mkf   mus.mkf  pat.mkf  rgm.mkf
rng.mkf  sss.mkf   word.dat
```

音效还需要 `voc.mkf` 或 `sounds.mkf`；DOS 版通常需要 `m.msg`。存档也写入
`A:\应用\数据\PAL\`。资源不会被复制进 Git 工作区。

Steam 经典版的 `PAL98` 目录还包含 `1.AVI` 至 `6.AVI`。原文件合计约
180 MiB，可在电脑上用 FFmpeg 离线转换成适合 9588 的低码率版本：

```powershell
.\tools\transcode-avi.ps1 <PAL98目录> .\build\pal98-compact
```

默认保留原始 288×180 分辨率，输出 12 fps、MS Video 1 和 11025 Hz 单声道 PCM，
六段实测合计 23.32 MiB，减少约 87.05%。转换结果仍是标准 AVI，但头部会被规范成
SDLPAL 小型解析器可接受的形式；工具还会把 RIFF 奇数长度流块的既有 padding 计入
块长度，规避旧顺序读取器不跳过 padding 的限制，文件总大小不变。它们属于由用户
自有商业资源产生的派生数据，不应提交或分发；真机使用时把六个文件复制到同一
`PAL` 数据目录即可。

可先在电脑上检查一个资源目录：

```powershell
.\tools\check-resources.ps1 D:\Games\PAL
```

## 构建

需要 PowerShell、Python 3.10+。首次构建会使用 SDK 自带脚本安装并校验 MIPS
工具链：

```powershell
.\tools\build.ps1
```

如果已有工具链，可设置：

```powershell
$env:BDA_TOOLCHAIN_PREFIX = "C:\toolchains\mips\bin\mipsel-none-elf-"
.\tools\build.ps1 -SkipToolchainSetup
```

输出为 `build\Pal1-9588.bda`，并同时生成 ELF、map 和反汇编文件。

## 模拟器测试

默认寻找 `E:\bbk9588-emulator-v0.1.5`，使用 SDK 的隔离测试 NAND，不修改原始
镜像：

```powershell
.\tools\test-emulator.ps1 -ResetImage
```

未放游戏资源时，程序会显示资源缺失诊断页；这也是无需分发商业数据的启动冒烟测试。

需要验证实际游戏流程时，可在模拟器网页的文件管理器中创建 `/应用/数据/PAL` 目录，再导入经
`tools/check-resources.ps1` 检查通过的自有游戏文件。仓库不提供也不接受这些商业资源。

也可以直接把合法资源导入隔离 NAND 并启动：

```powershell
.\tools\test-with-resources.ps1 D:\Games\PAL -ResetImage
```

同时测试离线转码视频：

```powershell
.\tools\test-with-resources.ps1 D:\Games\PAL `
    -VideoPath .\build\pal98-compact -ResetImage
```

更详细的移植结构和实测记录见 [docs/porting.md](docs/porting.md) 与
[docs/testing.md](docs/testing.md)。

生成的 BDA 是 9588 固件直接加载的 MIPS 原生程序；模拟器仅用于自动化验证，
不是发布依赖。模拟器已覆盖存档重启读回、代表性战斗和六段 AVI 完整解码；真机
性能、按键手感、扬声器表现及完整剧情仍需在实体 9588 上复测。

## 许可证与来源

移植代码采用 GPL-3.0-or-later。SDLPAL 的完整许可见
`third_party/sdlpal/gpl.txt`，SDK 许可见 `sdk/LICENSE`。商业游戏数据不属于本项目。

参与开发前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)，尤其是商业资源隔离与
submodule 固定版本要求。
