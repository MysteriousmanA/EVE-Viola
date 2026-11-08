# Qt / MSVC 构建故障记录：C1902

本文记录 2026-11-07 使用 `scripts/build_qt.ps1` 打包时实际遇到的错误和验证过的处理步骤。默认构建入口仍是在项目根目录运行：

```powershell
.\scripts\build_qt.ps1
```

脚本通过 `vswhere.exe` 查找 MSVC x64 环境，使用 Qt 自带的 CMake 配置 `qt-release`，编译后调用 `deploy_qt.ps1`。打包脚本最后会在不依赖 Qt SDK 的 `PATH` 下启动 `dist-cpp\EVE-Viola\EVE-Viola.exe` 做烟测。因此“编译成功”和“打包成功”是两个不同的检查点。

## 1. C1902：程序数据库管理器不匹配

### 当时的现象

执行打包命令后，CMake 停在编译器自检，尚未编译项目源码：

```text
-- Detecting CXX compiler ABI info - failed
-- Check for working CXX compiler: .../cl.exe - broken
... /Zi /Ob0 /Od /RTC1 -MDd ... /FS -c testCXXCompiler.cxx
fatal error C1902: 程序数据库管理器不匹配；请检查安装
```

`--fresh` 会重新运行 CMake 的编译器检查；**即使选择的是 Release 预设，这一步仍可能使用带 `/Zi` 的 Debug 编译参数**。因此不能只查看 Release 的 `/O2` 参数来判断这次失败。

### 做过的定位

1. 通过开发者环境检查 `cl /Bv`、`where cl`、`where mspdb140.dll`，看到当前 `cl.exe` 和其目录下的 `mspdb140.dll` 版本一致。`C1902` 的提示本身不足以断定整个 VS 安装损坏。
2. 用最小 C++ 文件单独编译：`/Zi` 可以通过；加入 `/FS` 后复现 `C1902`，并会等待较长时间；再加入 `/Z7` 后通过。
3. 把 `TEMP`、`TMP` 改到项目 `build` 目录后，原来的 `/Zi` 加 `/FS` 组合仍然失败，因此这次不是单纯的临时目录权限问题。

在本机复现时，可从 **VS x64 Native Tools 命令提示符**进入一个临时目录，创建 `probe.cpp`，内容为 `int main() { return 0; }`，然后依次运行：

```bat
cl /nologo /Zi /c probe.cpp
cl /nologo /Zi /FS /c probe.cpp
cl /nologo /Zi /Z7 /FS /c probe.cpp
```

`/Z7` 将调试信息写入目标文件，避开这次触发错误的 `/Zi` 程序数据库管理路径。以上是本机验证结果；其他机器若复现失败，仍应检查 VS 组件、`cl` 与 PDB 工具实际加载路径，以及权限或安全软件对编译器进程的影响。

### 已采用的修复

`scripts/build_qt.ps1` 在 CMake 配置时指定：

```text
-DCMAKE_CXX_FLAGS_DEBUG="/Z7 /Ob0 /Od /RTC1"
```

这样 CMake 的 Debug 编译器自检不再使用 `/Zi`；正常 Release 编译仍使用其原有 Release 参数。实际重新执行项目打包脚本后，编译器自检通过。

## 2. 编译通过，但发布版启动烟测崩溃

定位 C1902 时曾临时设置全局环境变量 `_CL_=/Z7`，使所有 `cl.exe` 调用都追加 `/Z7`。这一尝试让配置和编译通过，但随后 `deploy_qt.ps1` 的烟测报告：

```text
The packaged Windows startup test failed with exit code -1073741819
```

该退出码是 `0xC0000005`（访问冲突）。单独用 `offscreen` 启动当时的构建产物还观察到 `-1073740791`（`0xC0000409`）。这些退出码说明**编译通过不代表程序可启动**；它们本身不能证明 C1902 的根因，也不能直接归因于某一行应用代码。

后来去掉全局 `_CL_`，只保留上一节的配置阶段修复，并在 MSVC x64 开发者环境中执行一次干净重建：

```bat
cmake --build --preset qt-release --clean-first
```

接着重新运行项目打包脚本，相关测试和打包烟测均通过。这里有一个关键细节：`cmake --preset qt-release --fresh` **只重新配置 CMake，不会删除旧的 `.obj`**；当时下一次构建曾显示 `ninja: no work to do`。若尝试过 `_CL_`、编译标志或其他全局工具链参数，应先干净重建，再依据启动结果判断问题是否仍在。

不要把 `_CL_=/Z7` 作为常驻系统环境变量或项目标准修复。它会作用于所有编译命令，范围超过本次需要绕开的 CMake Debug 自检。上述烟测崩溃与全局参数或残留对象文件之间的确切因果关系未单独证明；干净重建是已验证有效的恢复步骤。

## 3. 其他同时遇到的环境问题

| 现象 | 处理思路 |
| --- | --- |
| 手写的 `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` 找不到 | 本机 VS 实际位于 `D:\Programming\Microsoft_VisualStudio`。不要推测安装路径；使用 `build_qt.ps1`，它通过 `vswhere.exe` 定位工具链。 |
| Qt 测试中的 `QSaveFile` 报临时目录访问被拒绝 | 当时测试运行环境限制了系统临时目录。仅在该环境中把 `TEMP` 和 `TMP` 指向可写的项目 `build` 目录后重跑；不要因此修改配置保存逻辑。 |
| GUI 测试找不到 Qt 平台插件，或无桌面测试无法启动 | 离屏测试时设置 `QT_QPA_PLATFORM=offscreen`，并让 `PATH` 包含 Qt `bin`、`QT_QPA_PLATFORM_PLUGIN_PATH` 指向 Qt `plugins\platforms`。正式打包烟测使用 `windows` 平台插件，由 `deploy_qt.ps1` 处理。 |

## 复发时的检查顺序

1. 从项目根目录运行开头所示的打包命令，确定失败发生在 **CMake 自检、项目编译、Qt 部署，还是最终启动烟测**。
2. 如果是 `C1902`，记录完整 `cl.exe` 命令行，尤其是 `/Zi`、`/Z7`、`/FS`；用最小文件复现，再检查 `cl /Bv` 和 `where mspdb140.dll`。
3. 确认脚本仍指定 `CMAKE_CXX_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1`；不要先对所有编译命令设置 `_CL_`。
4. 如果修改过工具链环境或编译标志，在 VS x64 开发者环境中执行 `cmake --build --preset qt-release --clean-first`，随后重新运行项目打包脚本。
5. 以脚本末尾的 `Packaged Windows startup test passed without the Qt SDK on PATH.` 为发布成功标志。若烟测失败，记录退出码，并检查 `dist-cpp\EVE-Viola\logs`；日志中没有新条目时，也要考虑崩溃发生在日志初始化之前。
