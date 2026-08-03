# PAL9588.PAK / PALVIDEO.PAK 格式

`PAL9588.PAK` 和可选的 `PALVIDEO.PAK` 使用同一种为 9588 低内存、低算力文件接口
设计的只读随机访问容器。主包保存游戏核心资源，视频包只保存 `1.avi`–`6.avi`。
容器不做二次压缩：MKF 本身已有压缩，AVI 已在电脑端转码；未压缩容器可避免设备端
解包、临时文件和额外内存，同时允许 SDLPAL 任意 `fseek`。

所有整数均为 little-endian `uint32_t`，整个文件不得超过 `0x7fffffff` bytes。

## 文件头

固定 32 bytes，等价于 Python `struct` 格式 `<8s6I`：

| Offset | Size | 字段 | v1 要求 |
| ---: | ---: | --- | --- |
| `0x00` | 8 | magic | `PAL9588\0` |
| `0x08` | 4 | version | `1` |
| `0x0c` | 4 | entry count | `1..256` |
| `0x10` | 4 | directory offset | `32` |
| `0x14` | 4 | entry size | `64` |
| `0x18` | 4 | data offset | 目录末尾向上对齐到 16 bytes |
| `0x1c` | 4 | archive size | 必须等于物理文件大小 |

## 目录项

文件头后紧跟 `entry count` 个固定 64-byte 目录项，等价于 `<48s4I`：

| Offset | Size | 字段 | v1 要求 |
| ---: | ---: | --- | --- |
| `0x00` | 48 | name | 小写 ASCII basename，NUL padding，最长 47 bytes |
| `0x30` | 4 | data offset | 16-byte 对齐且位于数据区内 |
| `0x34` | 4 | data size | 条目真实长度，不含 padding |
| `0x38` | 4 | CRC32 | 标准 zlib/IEEE CRC32 |
| `0x3c` | 4 | flags | v1 必须为 `0` |

目录名不含路径，并按忽略大小写的文件名排序。重复文件名、目录、非 ASCII 名称和未知
flags 都会被主机工具拒绝。每个数据条目起点按 16 bytes 对齐，末尾 padding 填零。

## 完整性模型

- `tools/palpak.py pack` 在写入前计算每个源文件 CRC32，并通过同目录临时文件原子替换
  输出；
- `verify` 和 `extract` 会检查头、目录、边界及每个条目的 CRC32；
- BDA 运行时检查 magic、版本、目录结构、物理大小、偏移和边界，然后直接读取条目；
- 为控制每次打开的 CPU 和 I/O 成本，设备端不重复计算整条目 CRC，因此部署前必须
  运行主机端 `verify`。

## BDA 文件语义

运行时分别为主包和已安装的视频包保留一个共享底层句柄，并为最多 32 个同时打开的
虚拟文件保存独立的 `base`、`size` 和 `position`。每次读操作先把所属包的共享句柄
定位到 `base + position`，因此多个 MKF/AVI 可以交错读取。

`1.avi`–`6.avi` 只查询 `PALVIDEO.PAK`，其余只读资源查询 `PAL9588.PAK`；包内条目
不能写入，也不能越过条目边界读取。视频包是可选项：运行时先用目录枚举判断其是否
存在，缺失时直接向 SDLPAL 报告视频不可用，不执行会干扰已有主包句柄的失败打开，
调用点随后回退到 DOS RNG 动画。写入和追加永远使用普通 NAND 文件。
