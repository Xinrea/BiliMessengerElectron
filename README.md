# BiliMessenger

![GitHub Workflow Status (with event)](https://img.shields.io/github/actions/workflow/status/Xinrea/bili-messenger/release.yml)
![GitHub release (with filter)](https://img.shields.io/github/v/release/Xinrea/bili-messenger)
![GitHub all releases](https://img.shields.io/github/downloads/Xinrea/bili-messenger/total)

> Bilibili 舰长私信群发助手（基于 [CUI](https://github.com/yappologistic/CUI) C99 Immediate-Mode UI 框架原生构建）

> [!NOTE]
> 程序下载地址：[https://github.com/Xinrea/bili-messenger/releases](https://github.com/Xinrea/bili-messenger/releases)

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
  - **关于 / 问题反馈**：通过 PowerLive 匿名提交问题描述和可选邮箱，自动附带软件版本、系统类型和诊断日志（可预览或取消）；本地保存草稿、日志快照及回执，重试沿用原编号和附件并遵守限流等待

### 反馈诊断日志

在「关于软件」点击「问题反馈」打开反馈弹窗。关闭弹窗不会清空草稿，也不会取消正在进行的提交；再次打开可继续查看状态和回执。

诊断记录在本次运行期间自动采集，内存中保留最近 256 条应用及请求日志，带时间和 INFO / WARN / ERROR 级别：
- 应用启动／退出、配置加载／保存、文件读写结果、后台任务启动／完成及创建失败。
- 账号检查、扫码登录成功、直播间绑定、名单／上舰记录获取、私信发送与撤回结果、群发结束／兑换码耗尽，以及反馈提交结果。
- HTTP 状态、libcurl 错误码、可用的数字业务错误码、耗时和响应大小。

采集时只允许固定事件名称和数字字段，不记录完整 URL、查询参数、请求／响应正文、服务器错误原文、文件路径、Cookie、令牌、账号标识、私信内容或兑换码。没有直接截取 stdout / stderr 或系统日志，避免把第三方库的未脱敏输出附带出去。
任务错误码 1/2/3 分别表示无效任务、内存分配失败、线程创建失败；文件／配置结果码 1 表示失败；撤回码 120 表示超过两分钟；群发停止码 1 表示兑换码耗尽。业务错误保留原数字码。

反馈默认附带 `diagnostics.log`（UTF-8，最多 64 KiB）。日志预览是独立的可滚动阅读视图，支持复制完整快照和返回表单；提交前也可刷新快照或取消勾选。预览过的快照不会被提交动作悄悄替换。
日志与草稿原子保存，网络失败或重启后的重试仍使用原编号及相同字节。未提交反馈时不会主动上传诊断记录；它不是崩溃转储，也不保留上次运行的完整日志。

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
