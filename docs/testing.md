# 测试记录

测试日期：2026-08-01

## 构建与静态检查

- 使用 SDK 提供的 MIPS 裸机工具链完成全量编译和链接。
- BDA 校验器确认文件头、入口地址和校验和有效。
- 最终 ELF 没有未解析符号。
- 链接脚本检查镜像及运行时数据不超过 32 MiB 可用内存。

## 9588 模拟器无资源冒烟测试

测试环境为 bbk9588-emulator v0.1.5，使用 SDK 脚本生成的独立测试
NAND。程序成功启动，模拟器没有产生崩溃快照，并捕获到完整的
240×320 RGB565 帧。

测试 NAND 未放入商业游戏资源，因此预期显示“缺少游戏资源”诊断页。
这覆盖了以下链路：

- BDA 加载、入口和运行时初始化；
- NAND 路径及资源文件探测；
- 窗口创建和事件轮询；
- 8 位调色板到 RGB565 的转换；
- 320×240 到 240×320 的旋转与画面提交；
- `Esc` 退出路径。

## 合法 Steam DOS 资源测试

使用测试者合法购买并由 Steam 安装的 `PAL_DOS` 资源集。运行：

```powershell
.\tools\test-with-resources.ps1 <PAL_DOS目录> -ResetImage
```

脚本先检查资源，再只把白名单文件导入 SDK 的隔离 NAND
`/应用/数据/PAL`，逐文件复核大小与 SHA-256；商业文件没有进入 Git 工作区。
固件路径使用 GBK byte string 后，资源探测、商标、片头、标题画面和新游戏
首场剧情均成功运行，画面方向已校正，模拟器没有崩溃快照。

## 单资源包回归

主机端先用 `tools/palpak.py` 对当前 `PAL9588.PAK` 的 27 个目录项逐项验证 CRC32，
随后以 `-ResetImage` 重建隔离 NAND，并通过导入脚本确认
`/应用/数据/PAL` 中只写入一个 `PAL9588.PAK`：

```powershell
python -S -m unittest discover -s tests -v
python -S .\tools\palpak.py verify .\build\PAL9588.PAK
.\tools\test-with-resources.ps1 .\build\PAL9588.PAK -ResetImage
```

设备包大小为 51623200 bytes，测试样本 SHA-256 为
`2CB184B45339003E5BD112082C820AD51A8F60BC71A72CABCCBBB8F9874584AE`。
BDA 的 `access()` 能发现包内虚拟文件，SDLPAL 随后交错打开并随机读取多个 MKF 和
AVI。模拟器实际越过资源检查，显示 288×180 启动 AVI；状态页同时记录约
32–34 MIPS、视频帧持续更新和 `play 22050 Hz`，证明图像与音频都来自同一包。
同一 NAND 随后用 `PORTTEST.BATTLE` 跑到 `BATTLE WON`，证明多个长期打开的虚拟
MKF 句柄可交错读取到战斗结算。

包工具测试还覆盖大小写归一化、打包/列表/完整校验/展开往返，以及单字节破坏后的
CRC32 拒绝。散文件兼容路径保留，仅用于故障定位。

## PAL98 AVI 过场

Steam `PAL98` 的六段原始 AVI 合计 188809584 bytes（180.06 MiB）。运行：

```powershell
.\tools\transcode-avi.ps1 <PAL98目录> .\build\pal98-compact
.\tools\test-with-resources.ps1 <PAL_DOS目录> `
    -VideoPath .\build\pal98-compact -ResetImage
```

当前默认 profile 保留原始分辨率，六段文件合计 24454440 bytes（23.32 MiB），缩小
87.05%。工具逐个确认 MS Video 1、288×180、12 fps、11025 Hz、8-bit mono PCM，并把 FFmpeg 写入
的 PCM stream handler 和 1 MiB suggested buffer 规范为原 PAL95 小型读取器使用的
形式。回归中发现 SDLPAL 的旧顺序读取器不会跳过 RIFF 奇数长度块后的一个 padding
byte；准备工具现把既有 padding 计入流块和 `idx1` 长度，不增加文件大小。修正后六个
`movi` 区域分别包含 206、736、414、160、1835、1757 个视频帧，且不再有奇数长度
音视频块，FFprobe 仍能读出原时长和帧数。

正常玩家路径实际显示了 `1.avi` 和 `2.avi`。对两段分别长按 `Esc` 后松手，第一段只
跳到第二段，第二段返回后稳定停留在“新的故事 / 旧的回忆”标题菜单，没有把同一次
按键带入菜单。桥接层现在会在播放前后等待动作键释放，同时清空平台锁存、精简 SDL
事件状态和 SDLPAL 键位状态。

低存储 160×100 profile 曾由回归入口无人为跳过地顺序播放 `3.avi` 至 `6.avi`，
设备端记录的结果为：

```text
VIDEO START
VIDEO 3 FRAMES 414 KEYS 0 PASS
VIDEO 4 FRAMES 160 KEYS 0 PASS
VIDEO 5 FRAMES 1835 KEYS 0 PASS
VIDEO 6 FRAMES 1757 KEYS 0 PASS
VIDEO PASS
```

四段均达到容器声明的完整帧数。最终累计音频传输 42850 packet，dropped packet 为
0。另一次人工跳过回归在 3、5、6 号分别显示 216、127、158 帧后退出，4 号自然播放
完整 160 帧；长按跳过 3 号不会继续跳过刚开始的 4 号。

改为默认 288×180 profile 后，模拟器实际显示了正常启动用的 1 号片段，并对 3–6
号分别解码送显 157、72、90、87 帧后跳过；四段均返回 `PASS`，累计音频传输 4548
packet，dropped packet 为 0，且无崩溃快照。分辨率提高没有改变容器帧数或 RIFF
兼容性处理。

此前从模拟器 PCM WebSocket 抽取开场 120 个 packet、105884 个 sample：48.85%
sample 非零，peak 1993，RMS 474.84，确认 AVI 音轨已进入设备 PCM，而不是只发送
静音块。

QEMU 本次约为 32–33 M guest instructions/s，解码和 RGB555 缩放会使 AVI 的墙钟
播放时间明显慢于标称时长；该现象只用于发现热点，不作为 9588 真机帧率结论。

六个转码容器现已通过静态格式检查；低存储 profile 完成了模拟器完整帧数回归，
当前默认原分辨率 profile 完成了六段实际解码抽测。结局剧情的脚本触发条件和实体机
音画同步仍需在真机完整流程中复测。

## 存档重启读回

`PORTTEST.SAVE` 使用两次独立启动验证 2 号存档：第一次从合法的 1 号初始档读取
场景、金钱和主角 HP，调用 SDLPAL 原生 `PAL_SaveGame` 写出 183488-byte `2.rpg`，
保存次数头为 `0x1234`，并另存 12-byte 预期快照；第二次启动不再写档，只调用
`PAL_InitGameData(2)` 并逐项比对。结果为：

```text
SAVE START
SAVE PASS
RELOAD START
RELOAD PASS
```

两次启动间 `2.rpg` 的 SHA-256 均为
`BE9ECFFF59AEF05E71B58E938ABC7A6E64A7F4F1C89D09A70E7E2BB151958F18`。

## 战斗流程

`PORTTEST.BATTLE` 从同一合法初始档装载调色板、场景和角色资源，进入一个无特殊
脚本的单低级敌人队伍。测试模式只在战斗主循环进入 `OnGoing` 后注入一次 SDLPAL
“自动攻击”动作，实际跑过敌我动画、伤害、音乐、音效、胜利结算和资源清理：

```text
BATTLE START
BATTLE READY
BATTLE WON
```

## 可重复回归入口

在隔离测试 NAND 的 `/应用/数据/PAL` 放置任意一个空标记即可启用对应入口：

- `PORTTEST.SAVE`：两次启动完成写档和重启读回；
- `PORTTEST.BATTLE`：运行单敌人自动攻击战斗；
- `PORTTEST.VIDEO`：跳过启动用 1、2 号片段，完整顺序播放 3–6 号；
- `PORTTEST.LOG`：测试过程和结果日志；存档测试另用 `PORTTEST.EXPECT` 保存预期值。

这些标记仅供隔离 NAND/真机测试使用，普通玩家不要创建；测试结束应删除标记。商业
资源不进入 `main` 或测试日志；仓库所有者的原版备份仅存在于受限的私有 `release`
分支。

## 音乐与音效

- SDK PCM：22050 Hz、signed 16-bit、mono，2048 samples/block；
- RIX 音乐：新版 DOSBox DBOPL 实时合成；
- 音效：`VOC.MKF` 由 SDLPAL 的 VOC/WAV 播放器解码、重采样并与音乐混音；
- 模拟器音频状态为 `playing=true`，传输丢包为 0；
- 一段稳态观测窗口输出 532243 个 sample，其中 17408 个 underrun，约 3.3%；
- 片头重解压曾出现约 0.91 秒的最大续 DMA 间隔，因此模拟器中仍可能听到一次
  短暂断续。

以上证明同一份原生 MIPS BDA 在完整固件模拟器中已跑通资源读取、存档重启读回、
代表性战斗、音乐音效和六段 AVI。实体 9588 上的性能、按键手感、扬声器音量、AVI
音画同步及完整剧情仍需真机回归。
