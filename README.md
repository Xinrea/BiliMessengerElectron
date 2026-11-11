# BiliMessenger

![GitHub Workflow Status (with event)](https://img.shields.io/github/actions/workflow/status/Xinrea/BiliMessengerElectron/release.yml)
![GitHub release (with filter)](https://img.shields.io/github/v/release/Xinrea/BiliMessengerElectron)
![GitHub all releases](https://img.shields.io/github/downloads/Xinrea/BiliMessengerElectron/total)

> Bilibili 舰长私信群发助手（基于 [CUI](https://github.com/yappologistic/CUI) C99 Immediate-Mode UI 框架原生构建）

> [!NOTE]
> 程序下载地址：[https://github.com/Xinrea/BiliMessengerElectron/releases](https://github.com/Xinrea/BiliMessengerElectron/releases)

## 特性

- **原生轻量架构**：使用 C99 + [CUI](https://github.com/yappologistic/CUI) 重构，告别 Chromium/Electron 体积膨胀与内存占用
- **配置无感迁移**：100% 兼容旧版 Electron `config.json` 配置文件（登录凭据、舰长列表、私信模板、兑换码、已发送记录）
- **完整业务能力**：
  - **总览**：直播间状态、主播信息、近 60 天舰长数与粉丝数趋势曲线（`vtbs.moe`）
  - **目标列表**：历史快照日期拉取（`guard.vjoi.cn` + 实时 `topList`）、上舰记录日期范围拉取（跨月自动切分 + 指数退避轮询）、UID/用户名添加用户、去重（保留最高航海等级）、TXT/CSV 导入导出、虚拟列表筛选与多选删除
  - **模板管理**：多模板增删改、变量标签快捷插入（`{level}`、`{name}`、`{code}`、`{date}`、`{month}`、`{lastMonth}`、`{day}`）与实时预览
  - **兑换码管理**：多行批量粘贴导入（自动去重去空行）、虚拟列表多选删除与清空
  - **私信群发**：WBI 签名、`bili_ticket` 凭据注入、64 位 `msg_key` 精确保存、可配置发送间隔、失败自动归入剩余列表、2 分钟内一键撤回
  - **设置**：扫码登录（内置二维码生成与轮询）、Cookie 手动登录、深浅色主题切换、启动自动校验登录状态与 GitHub Release 版本更新提示

## 构建与测试

### 依赖

- CMake >= 3.16
- 支持 C99 的编译器（AppleClang / GCC /Clang / MSVC）
- `libcurl`（macOS / Linux 系统自带；Windows 可通过 `vcpkg install curl` 或 MSYS2 安装）

### 编译步骤

```bash
# 拉取子模块（若克隆时未带 --recursive）
git submodule update --init --recursive

# 配置并编译 Release 版本
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8

# 运行单元测试
ctest --test-dir build --output-on-failure -V
```

### 运行程序

- **macOS**：`./build/BiliMessenger.app/Contents/MacOS/BiliMessenger`（或直接打开 `build/BiliMessenger.app`）
- **Linux / Windows**：`./build/BiliMessenger`（Windows 下为 `BiliMessenger.exe`）
