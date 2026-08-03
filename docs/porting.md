# 9588 移植说明

## 基线

本项目把上游 SDLPAL 固定在提交
`224cb0a0cc839e0e8ecaa299cd480b02b372b9f7`。该提交仍保留面向
Dingux/JZ4740 等小型 MIPS 设备的 SDL 1.x 时代代码，适合作为 9588
移植基线；上游源码作为只读 submodule 保留，平台差异集中在本仓库。

SDK 也以 submodule 固定，避免依赖开发者机器上的另一份 SDK。

## 运行结构

- `src/runtime`：BDA 入口、启动代码和 MIPS `setjmp/longjmp`。
- `src/libc`：无操作系统 C 运行时所需的最小 libc，以及主包/可选视频包虚拟文件层。
- `src/compat`：SDLPAL 所需的精简 SDL 1.x 兼容层、PCM 格式转换及最小 C++
  字符串支持。
- `src/port`：9588 窗口、按键、路径、游戏/AVI 画面提交、诊断页和显式标记驱动的
  回归入口。
- `linker/bda.ld`：BDA 内存布局及 32 MiB 上限检查。
- `tools`：构建、资源检查、PAL 资源打包/转换、BDA 打包和模拟器测试脚本。

SDLPAL 的逻辑画面为 320×200。本移植将其居中放入 320×240，
转换为 RGB565 后顺时针写入 9588 的 240×320 竖屏帧缓冲；设备本身
逆时针转 90° 横持时即为正向画面。帧缓冲地址由 SDK 从 GUI API
动态获取并校验 KSEG 别名、范围、对齐、尺寸和方向；正常路径用 uncached
32-bit 整帧复制直接写入 LCD 扫描缓冲。无法识别当前固件或提交失败时，
会惰性取得 visible draw context，回退到原有的 10 个 240×32 picture strip。
SDK 的同类真机基准中，整帧提交平均耗时由 picture 路径的 22.189 ms 降到
直接复制的 3.973 ms；该路径是单缓冲连续写入，缩短了撕裂窗口但不提供垂直同步。

AVI 使用另一条原生画面路径。`third_party/sdlpal/aviplay.c` 保持在只读
submodule 中，构建时只重命名其播放入口和画面回调；本仓库的桥接层把解出的
RGB555 帧缩放到 320×200、上下留黑边并旋转成 RGB565。视频帧一次整屏提交，
正常情况同样走 direct framebuffer；提交前后仍泵入 PCM。若视频不存在或
格式不受支持，SDLPAL 调用点按原逻辑回退到 RNG 动画。视频返回时还会清除用于
跳过的按键状态，避免同一次按键紧接着确认标题菜单。桥接层记录实际送显帧数，
解析成功但未产生视频帧时不再报告成功。

设备逆时针转 90° 横持时的按键映射：

| 物理按键 | 游戏动作 |
| --- | --- |
| 右 / 左 / 上 / 下 | 上 / 下 / 左 / 右 |
| Enter | 确认 |
| Esc | 菜单 / 取消 |

实体键来自 SDK `GUI+0x5d4` 的 6-byte input packet，不依赖窗口按键消息。启用
direct framebuffer 后，`SDL_PollEvent` 和 `SDL_Delay` 的运行期热路径不再调用
`bda_gui_event_pump_frame_once()`；只有 direct path 失败并回退 picture renderer，
或者执行 `frame_stop -> frame_release` 关闭序列时才恢复窗口消息泵。这样仍能在退出时
接收 `DRAW_CONTEXT_DETACH`，但不会让慢速窗口队列限制每帧速度。同时长按
Enter + Esc 1.5 秒会产生 SDL quit，作为不依赖窗口消息的原生安全退出手势。

游戏资源、存档和配置统一位于 `A:\应用\数据\PAL\`。固件文件 API 接收
ASCII/GBK byte string，代码中的中文目录显式写为 GBK 转义字节，不依赖源文件编码。

只读商业核心资源集中在 `PAL9588.PAK`，六段 AVI 位于可选的 `PALVIDEO.PAK`。
`src/libc/runtime.c` 的 `access` 和 stdio 包装层按文件名选择容器；存档、配置和日志仍走
普通可写文件。容器不在设备端解包，每个容器最多保留一个共享底层句柄，32 个静态
虚拟 `FILE` 槽支持 MKF、文本和 AVI 交错随机读取。静态槽避免固件小块堆分配在启动
阶段失败，也避免为每个资源各占一个固件句柄。对可选视频包先用目录枚举探测，缺失时
直接返回“视频不可用”，不会用一次失败的 `fopen` 破坏已经打开的主包句柄。详细布局见
`docs/palpak-format.md`。

## 当前边界

已接入完整 SDLPAL 游戏、战斗、脚本、地图、UI、存档与音频核心。PCM
设备固定为 SDK 已验证的 22050 Hz、signed 16-bit、mono；启动时显式设为全幅并在
退出时恢复原固件衰减值。SDLPAL 的 RIX
播放器使用实测更适合本目标的新版 DOSBox DBOPL 核合成音乐，VOC/WAV 音效经过
重采样后与音乐混音。PCM 使用 2048-sample（约 92.9 ms）缓冲，由按键轮询、
整帧提交前后和延时函数共同泵入固件队列，退出时调用公开的
`bda_audio_stop()`。模拟器的片头重解压阶段仍可能产生短暂欠载，真机表现待测。

AVI 播放已接入 SDLPAL 自带的 Microsoft Video 1/PCM AVI 解析和解码代码，不依赖
飞天影音的私有 `player.bin`。主机工具把 PAL98 六段原片离线转换为
保留原始 288×180 分辨率的 12 fps、RGB555 MS Video 1，加 11025 Hz、8-bit、mono
PCM；精简 SDL 兼容层再把音频升采样为设备要求的 22050 Hz signed 16-bit mono。
FFmpeg 只负责解出帧和音轨，`tools/encode-msvideo1.py` 按误差阈值在每个 4×4 块上
选择沿用前帧、单色、双色或分区八色模式。默认参数下六段从 180.06 MiB 降为
31.60 MiB；与旧 FFmpeg MS Video 1 重编码结果相比多 8.28 MiB，但不会增加设备端
解码复杂度，并显著减少纹理和文字边缘的块状损失。输出器直接生成旧 PAL95 顺序
读取器可接受的偶数长度 RIFF 流块，不依赖 `idx1`。11025 Hz 在 12 fps 下产生的
分数音频样本会顺延到下一个交错组，整段最多只在末尾补一个静音样本，避免长片累积
音画偏移。

六个容器都通过离线完整解码校验；当前默认 288×180 profile 已实际显示正常启动
视频，并抽测 3–6 号分别送显 151、28、27、27 帧后退出，四段均返回 `PASS`，音频
传输丢包为 0。存档两次启动读回和
代表性战斗胜利也已跑通。拆分包路径在隔离 NAND 同时部署 `PAL9588.PAK` 和
`PALVIDEO.PAK` 时进入启动 AVI 并播放 22050 Hz PCM；只部署主包时则跳过视频并回退
到 DOS RNG 动画。真机性能、所有片段的音画同步及完整剧情仍需最终回归。
