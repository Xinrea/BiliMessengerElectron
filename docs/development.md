# 构建与测试

[← 返回 README](../README.md)

当前源码为 C99 + [CUI](https://github.com/yappologistic/CUI) 原生应用，使用 CMake 构建。

## 依赖

- CMake >= 3.16。
- 支持 C99 的编译器（AppleClang / GCC / Clang / MSVC）。
- `libcurl` 开发库：macOS 可使用系统 SDK；Linux 需安装发行版对应的开发包；Windows 可使用 vcpkg。
- CUI 的平台依赖：参见 [CUI 文档](https://github.com/yappologistic/CUI)。
- Python 3（可选）：CMake 检测到解释器时会注册反馈 HTTP 测试。

## 获取源码

```bash
git clone --recurse-submodules https://github.com/Xinrea/bili-messenger.git
cd bili-messenger
```

已有仓库但尚未初始化子模块时，先执行：

```bash
git submodule update --init --recursive
```

## macOS / Linux

```bash
# 配置并编译 Release 版本
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

# 运行单元测试
ctest --test-dir build --output-on-failure -V
```

运行程序：

- **macOS**：打开 `build/BiliMessenger.app`，或执行 `./build/BiliMessenger.app/Contents/MacOS/BiliMessenger`。
- **Linux**：执行 `./build/BiliMessenger`。

## Windows

以下 PowerShell 示例使用 Visual Studio 的多配置生成器和 vcpkg。请先安装 C/C++ 构建工具与 vcpkg，并将 `VCPKG_INSTALLATION_ROOT` 设为本机 vcpkg 安装目录。

```powershell
vcpkg install curl:x64-windows
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release -j8
ctest --test-dir build -C Release --output-on-failure -V

.\build\Release\BiliMessenger.exe
```

使用单配置生成器时，可执行文件通常位于 `build/BiliMessenger.exe`，而不是 `build/Release/`。

## 测试与发布

测试默认开启，可通过 `-DBM_BUILD_TESTS=OFF` 关闭。测试覆盖核心逻辑、反馈持久化、反馈界面及 HTTP 提交等行为，具体目标见 [CMakeLists.txt](../CMakeLists.txt)。

仓库的 [Release 工作流](../.github/workflows/release.yml) 构建并测试 macOS 和 Windows 版本，分别打包为 `BiliMessenger-macOS.zip` 与 `BiliMessenger-Windows-x64.zip`。Linux 不在当前发布工作流中。
