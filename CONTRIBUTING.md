# 参与贡献

感谢参与 SDLPAL for BBK 9588。提交修改前，请确认 `main` 分支可以在不检出
《仙剑奇侠传》商业数据的情况下完成构建和基本启动测试。

## 开发环境

克隆时初始化固定版本的 submodule：

```powershell
git clone --recurse-submodules https://github.com/HelloClyde/pal1-for9588.git
cd pal1-for9588
git submodule update --init --recursive
```

构建与模拟器测试入口分别为：

```powershell
.\tools\build.ps1
.\tools\test-emulator.ps1 -ResetImage
```

涉及真实游戏流程的测试只能使用贡献者自己合法取得的资源。测试方法见
`docs/testing.md`。

## 商业资源隔离

- 不要把原版或 Steam 版的 MKF、AVI、`word.dat`、`m.msg`、存档和 NAND 镜像提交
  到 `main`、PR 分支或 issue 附件。
- 私有 `release` 分支只能保存经仓库所有者授权的单个
  `release-assets/PAL-ORIGINAL.PAK`，必须由 Git LFS 跟踪，且永远不能合并到
  `main`。
- 设备用 `PAL9588.PAK` 和单独转码视频仍是商业数据的派生文件，只能位于被忽略的
  `build/`、私有测试 NAND 或用户设备中。
- 构建产物、工具链和测试日志也不进入版本库。
- 提交前运行 `git status --short` 和 `git diff --cached --stat`，确认暂存区中不存在
  上述文件；维护 `release` 分支时则只允许一个 LFS pointer 资源文件。

仓库从私有改为公开前，必须先删除远端 `release` 分支并确认商业 LFS 对象不再能从
公开仓库访问。

## 代码与 submodule

- 保持 9588 平台适配位于本仓库的 `src/`、`linker/` 与 `tools/`；不要直接修改
  `third_party/sdlpal` submodule 中的历史源码。
- 如确实需要更新 SDK 或 SDLPAL 固定提交，请在提交说明中写明原因、上游提交和
  已完成的回归测试。
- C/C++ 修改应至少通过 `tools/build.ps1`；影响文件、存档、战斗、音频或 AVI 的
  修改还应执行 `docs/testing.md` 中相应的模拟器回归。

## 提交说明

提交应聚焦单一目的，并简要说明用户可见影响和验证结果。公开问题或日志时，请先
移除本机路径、商业数据文件名之外的私人信息，以及任何游戏资源内容。
