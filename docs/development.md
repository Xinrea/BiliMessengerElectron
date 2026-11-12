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

### 应用图标

`assets/icons/box.svg` 是 logo 的矢量源文件，README 直接引用它。修改后使用 Python 3 和 `rsvg-convert`（macOS：`brew install librsvg`；Debian / Ubuntu：`apt install librsvg2-bin`）重新生成应用图标：

```bash
python3 assets/icons/generate.py
```

脚本生成 `box.png`、Windows 使用的多尺寸 `icon.ico` 和 macOS 使用的多尺寸 `icon.icns`。这些产物随源码提交，正常编译不需要额外安装图标生成工具。

### 测试与打包

测试默认开启，可通过 `-DBM_BUILD_TESTS=OFF` 关闭。测试覆盖核心逻辑、反馈持久化、反馈界面及 HTTP 提交等行为，具体目标见 [CMakeLists.txt](../CMakeLists.txt)。

仓库的 [Release 工作流](../.github/workflows/release.yml) 构建并测试 macOS 和 Windows 版本：

| 平台 | 发布文件 | 说明 |
| --- | --- | --- |
| macOS | `BiliMessenger-macOS.zip` | 应用包 |
| Windows x64 | `BiliMessenger-Windows-x64.exe` | 免安装单文件 |
| Windows x64 | `BiliMessenger-Windows-x64-Setup.exe` | 当前用户安装程序，无需管理员权限，支持快捷方式和卸载 |

推送 tag 时上传到 GitHub Release；手动触发时不创建 Release，可从 Actions 的 artifacts 下载构建产物。Linux 不在当前发布工作流中。

发布 tag 使用 SemVer 格式（可带 `v` 前缀），例如 `v2.0.1`。带预发布后缀的 tag（如 `v2.1.0-beta.1`、`v2.1.0-rc.1`）会自动标记为 GitHub prerelease；`+` 后的构建元数据不影响该判断。两个平台共用同一判断结果。客户端通过 GitHub `/releases/latest` 获取正式版，并与 CMake 提供的 `BM_VERSION` 做语义化版本比较；不会提示升级到更低或相同版本，构建元数据不参与版本优先级比较。

Windows 发布构建不同于上面的默认动态链接开发构建：使用 `curl[core,ssl]:x64-windows-static`、静态 CUI 和静态 MSVC 运行库。curl 的 `ssl` feature 在 Windows 上启用 Schannel，HTTPS 使用 Windows 证书存储，不需要附带 OpenSSL DLL 或 CA 文件。两种发布形式使用同一份应用 exe，不打包测试程序、静态库或构建目录中的其他文件。

本地复现 Windows 发布打包（PowerShell；需要 Inno Setup 6.3+）：

```powershell
vcpkg install "curl[core,ssl]:x64-windows-static"
cmake -S . -B build-release -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' `
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW `
  -DCUI_BUILD_SHARED=OFF -DBM_BUILD_TESTS=ON
cmake --build build-release --config Release -j4
ctest --test-dir build-release -C Release --output-on-failure --no-tests=error -V

New-Item -ItemType Directory -Path dist -Force | Out-Null
Copy-Item build-release/Release/BiliMessenger.exe dist/BiliMessenger-Windows-x64.exe
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" `
  "/DSourceDir=$pwd\build-release\Release" "/DOutputDir=$pwd\dist" build-release/BiliMessenger.iss
```

安装脚本由 CMake 从 `assets/windows-installer.iss.in` 生成，版本号取自 `project(... VERSION ...)`。安装位置默认为 `%LOCALAPPDATA%\Programs\BiliMessenger`；卸载保留 `%APPDATA%\bilimessenger` 中的用户数据。工作流还会静默安装并校验安装后的 exe 与免安装文件哈希一致、卸载程序存在。
