# 测试记录

测试日期：2026-08-02；资源拆包回归更新：2026-08-03

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
- 窗口创建、直接按键轮询和关闭阶段事件处理；
- 8 位调色板到 RGB565 的转换；
- 320×240 到 240×320 的旋转与 direct framebuffer 整帧提交；
- 固件布局不匹配或提交失败时保留 picture renderer 回退分支；
- Enter + Esc 长按安全退出路径。

## Direct framebuffer 回归

SDK submodule 更新到 `ac4558a` 后，使用带 QEMU GDB 诊断的完整资源 NAND 启动。
运行时 `g_direct_framebuffer=1`，SDK 返回的 uncached 扫描缓冲为
`0xa1f82000`，描述符为 240×320、stride 480、`rotate_180=1`。修正直接模式不应
依赖旧 `g_draw`/`g_draw_object` 的提交前置条件后，模拟器 frame chardev 持续收到
新帧；普通 `/screen.png` 与同一时刻直接读取扫描缓冲生成的 PNG 哈希一致，间隔
两秒采集的帧哈希不同。

用 `Esc` 跳过片头后实际进入游戏场景，确认 320×240 到 240×320 的旋转送显链路。
2026-08-03 根据真机反馈将最终映射从顺时针改为逆时针 90°：逻辑坐标 `(x, y)`
映射到物理坐标 `(y, 319-x)`，并同步反转实体方向键映射。编译期四角断言覆盖
`(0,0) -> (0,319)` 和 `(319,239) -> (239,0)`。模拟器重新部署 BDA 后抓取原始
240×320 帧；该帧顺时针旋转回 320×240 时人物和对话文字均正向且无镜像，确认
帧缓冲内容为目标的逆时针 90°。状态页同时记录 PCM
`playing=true`、`muted=false`、22050 Hz、
transport dropped packet 为 0，且没有崩溃快照。最终 BDA 大小为 2418344 bytes，
SHA-256 为
`560CE2427076877C54A110A77CF6FE76DC6D982374425BF94A524251E92B7CAF`。

## 运行期窗口消息泵优化

direct framebuffer 模式的实体键本来已经通过 SDK input packet 直接读取，但旧版
仍在每次 `SDL_PollEvent()` 以及 `SDL_Delay()` 的约 1 ms 循环内最多泵 8 条窗口
消息。优化版在游戏运行期完全跳过窗口队列；picture renderer 回退路径仍正常泵送，
关闭时则在 `frame_stop -> frame_release` 之后最多处理 128 条消息等待 detach。

在同一完整 NAND、同一启动 AVI 和同样 20 秒采样窗口内，以 QEMU frame chardev
计数比较：

| 版本 | 帧数 | 时间 | 实测 FPS |
| --- | ---: | ---: | ---: |
| 运行期窗口消息泵 | 103 | 20.095 s | 5.126 |
| direct input、无运行期消息泵 | 232 | 20.062 s | 11.564 |

优化后为原来的 2.256 倍，达到 12 FPS 源视频帧率的 96.4%。两次测试都保持
22050 Hz PCM 播放，transport dropped packet 为 0，且没有崩溃快照。模拟器还验证
了同时长按 Enter + Esc 1.5 秒会走 SDL quit、停止并释放 frame，随后正常返回系统
启动器；窗口消息只在这一关闭阶段恢复。该结果用于比较软件路径开销，不等同于真机
最终 FPS，仍需实体 9588 复测。

## 触摸点击寻路回归

2026-08-03 在全新隔离 NAND 中一次部署当前 BDA、`PAL9588.PAK` 和
`PALVIDEO.PAK`，从 PAL98 AVI 进入新游戏并推进到客栈自由行动场景。模拟器通过原始
触摸通道发送 down/up，端口层成功按逆时针横屏方向反算坐标；触摸没有被误判为 Esc，
也没有打开系统菜单或退出程序。点击可调查位置后，角色自动接近并显示游戏内调查文本，
验证了寻路、地图/事件物件碰撞和到达后 `PAL_Search()` 交互链路。

测试期间 direct framebuffer 持续送显，地图场景瞬时采样约 10–13 FPS，PCM
`playing=true`，且无崩溃快照。寻路只在点击时执行一次有界 BFS，后续每帧仅核对并
提交下一步；该模拟器数值用于回归，不代表真机最终性能。当前 BDA 为 2,421,896 bytes，
SHA-256 为
`530AEDB96A09543A92765C47D9594A3A5E32FAEDFA3AB0B8DB78A11DC08426D7`。

## 合法 Steam DOS 资源测试

使用测试者合法购买并由 Steam 安装的 `PAL_DOS` 资源集。运行：

```powershell
.\tools\test-with-resources.ps1 <PAL_DOS目录> -ResetImage
```

脚本先检查资源，再只把白名单文件导入 SDK 的隔离 NAND
`/应用/数据/PAL`，逐文件复核大小与 SHA-256；商业文件没有进入 Git 工作区。
固件路径使用 GBK byte string 后，资源探测、商标、片头、标题画面和新游戏
首场剧情均成功运行，画面方向已校正，模拟器没有崩溃快照。

## 主包与可选视频包回归

当前设备资源拆成 21 项核心资源的 `PAL9588.PAK`，以及只含 `1.avi`–`6.avi` 的
可选 `PALVIDEO.PAK`。主机端分别逐项验证 CRC32，导入脚本也拒绝主包混入 AVI、
视频包缺项或包含额外成员：

```powershell
python -S -m unittest discover -s tests -v
python -S .\tools\palpak.py verify .\build\PAL9588.PAK
python -S .\tools\palpak.py verify .\build\PALVIDEO.PAK
.\tools\test-with-resources.ps1 .\build\PAL9588.PAK `
    -VideoPackage .\build\PALVIDEO.PAK -ResetImage
```

当前产物为：

| 文件 | 条目 | bytes | SHA-256 |
| --- | ---: | ---: | --- |
| `PAL9588.PAK` | 21 | 27168336 | `18AF405D5EE7E0F31148A4BC6733BBA6D2CF141BA1B7F1E2FB49EC52E4DC3E30` |
| `PALVIDEO.PAK` | 6 | 33134352 | `3BAB5AFA2FDC1DFB2CA1DE3C60A8ECDFE414ED683AC511D034028E50D57E8248` |

BDA 的 `access()`/stdio 层按 `1.avi`–`6.avi` 文件名选择视频包，其他资源选择主包。
两个包同时部署后，模拟器画面实际显示了 288×180 SOFTSTAR 启动 AVI；采样时收到
115 个 LCD 帧和 465 个音频 packet，PCM 为 22050 Hz、`playing=true`、
`muted=false`，transport dropped packet 为 0，且没有崩溃快照。

随后用 `-ResetImage` 重建 NAND，只部署 `PAL9588.PAK`：

```powershell
.\tools\test-with-resources.ps1 .\build\PAL9588.PAK -ResetImage
```

导入结果确认数据目录不存在 `PALVIDEO.PAK`。程序没有因可选包打开失败而破坏主包
句柄，而是显示 DOS 水墨 RNG 片头；采样时累计 471 个 LCD 帧和 1771 个音频 packet，
PCM 仍为播放且未静音，dropped packet 为 0，没有崩溃快照。这验证了视频包可以不装
或单独删除，完整游戏核心仍可启动。

包工具测试还覆盖大小写归一化、打包/列表/完整校验/展开往返，以及单字节破坏后的
CRC32 拒绝。散文件兼容路径保留，仅用于故障定位。

## PAL98 AVI 过场

Steam `PAL98` 的六段原始 AVI 合计 188809584 bytes（180.06 MiB）。运行：

```powershell
.\tools\transcode-avi.ps1 <PAL98目录> .\build\pal98-compact
.\tools\test-with-resources.ps1 <PAL_DOS目录> `
    -VideoPath .\build\pal98-compact -ResetImage
```

当前默认 profile 保留原始分辨率，六段文件合计 33133898 bytes（31.60 MiB），缩小
82.45%。FFmpeg 解出 12 fps RGB555 帧和 11025 Hz、8-bit mono PCM；
`tools/encode-msvideo1.py` 再以默认 quality error 14、skip error 7 选择 MS Video 1
的跳过、单色、双色和分区八色块。与旧 FFmpeg 重编码器的 23.32 MiB 输出相比增加
8.28 MiB，但片头抽样 SSIM 从 0.929 提升到 0.941，地球纹理与 SOFTSTAR 字样边缘
明显更接近原片。设备端解码代码和复杂度不变。

工具逐个确认 MS Video 1、288×180、12 fps、11025 Hz、8-bit mono PCM，并完整解码
每个输出。六个 `movi` 区域分别包含 206、736、414、160、1835、1757 个视频帧；
所有流块均为偶数长度，最大 suggested buffer 为 24544 bytes，低于播放器的 64 KiB
上限，容器不依赖 `idx1`。音频交错器把不能整除帧率的单个样本顺延到下一组，避免
为每组补齐造成累计时长漂移。

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

高质量 288×180 profile 在独立视频包路径实际显示了正常启动用的 1 号片段。标记回归对
3–6 号分别解码送显 151、28、27、27 帧后退出，设备端日志为：

```text
VIDEO START
VIDEO 3 FRAMES 151 KEYS 1 PASS
VIDEO 4 FRAMES 28 KEYS 1 PASS
VIDEO 5 FRAMES 27 KEYS 1 PASS
VIDEO 6 FRAMES 27 KEYS 1 PASS
VIDEO PASS
```

四段均无崩溃，PCM transport dropped packet 为 0。回归后已删除测试标记和日志，
并用模拟器自带工具重建测试 NAND 的页外 ECC 后重新校验两个 PAK 完整。

此前从模拟器 PCM WebSocket 抽取开场 120 个 packet、105884 个 sample：48.85%
sample 非零，peak 1993，RMS 474.84，确认 AVI 音轨已进入设备 PCM，而不是只发送
静音块。

QEMU 本次约为 32–33 M guest instructions/s，解码和 RGB555 缩放会使 AVI 的墙钟
播放时间明显慢于标称时长；该现象只用于发现热点，不作为 9588 真机帧率结论。

六个转码容器现已通过静态格式和完整解码检查；当前默认原分辨率 profile 完成了
六段实际解码抽测。结局剧情的脚本触发条件和实体机音画同步仍需在真机完整流程中
复测。

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

`PORTTEST.BATTLE` 从确定的新游戏状态装载调色板、场景和角色资源，并显式使用
2 号战场与初始档的 37 号战斗音乐，进入一个无特殊脚本的单低级敌人队伍。显式设置
战场很重要：FBP 的 0、1 号图分别是角色状态页和装备页背景，并不是战斗场景；2 号
是原版脚本实际使用的第一个战场。测试模式只在战斗主循环进入 `OnGoing` 后注入一次
SDLPAL“自动攻击”动作，实际跑过敌我动画、伤害、音乐、音效、胜利结算和资源清理：

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
- `PORTTEST.SOUND`：播放酒剑仙事件使用的 93 号夜间更鼓并记录循环最大耗时、
  按键和触摸输入计数；
- `PORTTEST.LOG`：测试过程和结果日志；存档测试另用 `PORTTEST.EXPECT` 保存预期值。

这些标记仅供隔离 NAND/真机测试使用，普通玩家不要创建；测试结束应删除标记。商业
资源不进入 `main` 或测试日志；仓库所有者的原版备份仅存在于受限的私有 `release`
分支。

## 音乐与音效

- SDK PCM：22050 Hz、signed 16-bit、mono，2048 samples/block；
- RIX 音乐：新版 DOSBox DBOPL 实时合成；
- 音效：`VOC.MKF` 由 SDLPAL 的 VOC/WAV 播放器解码、重采样并与音乐混音；
- 2026-08-08 使用 `PORTTEST.SOUND` 播放酒剑仙事件连续调用的 93 号 6000 Hz 更鼓；
  5 秒内完成 4987 次循环，单次最大耗时 5 ms，同时接收 2 次按键和 4 次触摸，日志为
  `SOUND INPUT PASS`；
- 模拟器音频状态为 `playing=true`，传输丢包为 0；
- 一段稳态观测窗口输出 532243 个 sample，其中 17408 个 underrun，约 3.3%；
- 片头重解压曾出现约 0.91 秒的最大续 DMA 间隔，因此模拟器中仍可能听到一次
  短暂断续。

以上证明同一份原生 MIPS BDA 在完整固件模拟器中已跑通资源读取、存档重启读回、
代表性战斗、音乐音效和六段 AVI。实体 9588 上的性能、按键手感、扬声器音量、AVI
音画同步及完整剧情仍需真机回归。
