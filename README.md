<div align="center">

# Anomaly 插件模板

**一个开箱即用的 Anomaly 插件脚手架 —— 逐一演示全部 UI 控件**

快速上手 · 项目结构 · 改造指南 · 开源许可

![platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6) ![language](https://img.shields.io/badge/C%2B%2B-20-00599C) ![abi](https://img.shields.io/badge/Anomaly%20ABI-v1-2bb5a0) ![license](https://img.shields.io/badge/license-AGPL--3.0--only-blue)

</div>

本仓库是 Anomaly 插件的**模板工程**。它包含一个可直接编译、验证和加载的 ABI v1 示例插件 **UI Gallery（控件画廊）**，在一个窗口里演示 `anomaly.ui` 服务提供的各种交互控件。点击 GitHub 的 **Use this template** 创建自己的仓库，替换示例元数据并删掉不需要的控件即可开始开发。

> [!NOTE]
> 模板中的占位标识（`com.example.ui-gallery`、`Your Name` 等）需要你替换成自己的信息。见下方[「改造成你自己的插件」](#-改造成你自己的插件)。

## ⚠️ 免责声明

本模板仅用于 Anomaly 插件开发的学习与脚手架用途。你基于本模板开发、分发插件所产生的一切后果由你自行承担。示例插件不修改游戏、不访问网络，只在宿主窗口内绘制 UI。

## ✨ 功能一览

示例插件在单个窗口里演示以下控件。所有字段访问都按 `struct_size` 做运行期能力探测，因此可以兼容 ABI major 1 下的 UI 服务表尾部扩展；宿主未提供的字段会被跳过：

- 🔤 **基础** —— `text` 文本、`button` 按钮、`checkbox` 复选框、复选框模拟的单选组
- 🔢 **数值与颜色** —— `slider_float` 滑块、`input_uint32` 整数输入、`input_double` 双精度输入、`color_edit4` 颜色选择
- ⌨️ **输入 / 过滤 / 表格** —— `input_text` 文本输入、`filter_match` 模糊过滤、`begin_table` 多列表格
- 🧱 **布局容器** —— `separator` 分隔线、`begin_child` 可滚动子区域
- 🪟 **弹窗与菜单** —— `open_popup` / `begin_popup_modal` 模态框、`begin_menu` 子菜单
- 🩺 **元信息** —— `frame_state` 帧状态、`developer_mode_enabled` 开发者模式、`get_window_size` 窗口尺寸、`on_update` 帧计数
- 🌐 **本地化** —— 通过可选的 `anomaly.localization` 服务翻译界面文案，缺失时回退英文

> [!TIP]
> ESP 世界空间绘制（`draw_entity_bbox` 等）不是窗口控件，需要相机与活动会话，故不在本画廊范围内；可参考主仓库 `plugins/EntityESP`。

## ABI v1 约定

- DLL 只导出 `AnomalyPluginEntryV1`，并填写 `AnomalyPluginDescriptorV1`。
- 插件通过 `AnomalyHostApiV1::query_service` 查询服务，不依赖 Runtime 源码或 C++ ABI。
- `anomaly.ui` 和可选的 `anomaly.localization` 当前都请求 service version 1。
- `on_load` / `on_start` / `on_stop` / `on_unload` 运行在 Lifecycle 域，`on_update` 在 Game 域，`on_draw` 在 Render 域。
- 服务可能暂时不可用；必需服务不可用时拒绝启动，可选服务不可用时回退。

## 🚀 快速上手

### 前置条件

- Windows x64 + Visual Studio 2022（MSVC，含 C++20）
- CMake ≥ 3.22
- 一份**已安装的 Anomaly SDK**（`AnomalySDK` CMake 包）

先在 Anomaly 主仓库配置标准构建树并安装 SDK：

```powershell
cmake --preset windows-vs2022
cmake --install .build\windows-vs2022 --config RelWithDebInfo `
  --prefix C:\AnomalySDK --component SDK
```

安装目录形如：

```
<sdk-prefix>/
  include/anomaly/sdk/...
  lib/cmake/AnomalySDK/AnomalySDKConfig.cmake
```

### 配置与编译

本仓库自带 Visual Studio 2022 x64 preset。配置时把 `AnomalySDK_DIR` 指向已安装 SDK 的 CMake 包目录：

```powershell
cmake --preset default `
  -DAnomalySDK_DIR=C:/AnomalySDK/lib/cmake/AnomalySDK
cmake --build --preset default
```

编译完成后，一个完整的插件包位于：

```
build/package/UiGallery/
  plugin.dll
  manifest.json
  locales/zh-CN.json
```

MSVC 还可能在该目录生成 `.pdb`、`.lib` 和 `.exp` 开发产物；Runtime 加载只需要上面列出的文件。

### 验证

使用与目标 Runtime 同版本的工具检查 Manifest、入口和生命周期：

```powershell
anomaly-plugin validate .\build\package\UiGallery
anomaly-test-host --plugin .\build\package\UiGallery --reload 10 --ticks 60
```

`anomaly-test-host` 不需要游戏进程，适合先验证加载、tick、停止和热重载。它不能代替依赖真实 NTE 会话的功能测试。

### 部署

把运行时文件复制到 `Anomaly\plugins\UiGallery\`，确保 `manifest.json` 和 `plugin.dll` 直接位于该目录根部。开发中的文件变化会由 Runtime 的插件监控器触发热重载。

## 🗂️ 项目结构

```
.
├── CMakeLists.txt        # 消费 AnomalySDK，调用 anomaly_add_plugin
├── CMakePresets.json     # MSVC x64 预设
├── manifest.json         # 插件清单（schemaVersion 2）
├── pluginmaster.json     # 第三方插件源条目
├── src/
│   └── plugin.cpp        # 控件画廊：全部演示代码集中于此
├── locales/
│   └── zh-CN.json        # 简体中文文案
├── LICENSE               # AGPL-3.0-only
└── README.md
```

## 🧩 改造成你自己的插件

1. **改标识**：`manifest.json` 与 `src/plugin.cpp` 里同步修改
   - `id`（反向域名，如 `com.yourname.myplugin`）、`name`、`author`、`version`
2. **改清单**：按需增删 `services` / `capabilities`；只声明你真正会用到的服务
3. **写逻辑**：在 `Draw()` 里保留需要的控件段落，删掉其余；需要每帧逻辑就用 `Update()`
4. **加本地化**：按需增加 `locales/en-US.json`；文件内键名与 `Tr()` 调用一致
5. **改包名**：`CMakeLists.txt` 里的 `PACKAGE_NAME` / 输出目录
6. **改发布信息**：同步更新 `pluginmaster.json` 的 ID、版本、仓库地址和下载地址

> [!CAUTION]
> `manifest.json` 的 `id` 必须与 `AnomalyPluginEntryV1` 返回的描述符一致，否则宿主会拒绝加载。名称、作者和版本也应保持一致；非可选服务必须声明对应 capability。

## 配置 pluginmaster.json

`pluginmaster.json` 是提供给 Anomaly 插件源的索引，根节点必须是 JSON 数组。通常每个插件 ID 只保留当前发布版本。将模板改成自己的插件时，可以从下面的配置开始：

```json
[
  {
    "Name": "My Plugin",
    "InternalName": "com.yourname.my-plugin",
    "Version": "1.0.0",
    "Author": "Your Name",
    "Punchline": "一句话说明插件用途。",
    "Description": "说明插件提供的功能、使用方式和已知限制。",
    "Games": ["nte"],
    "ApiMajor": 1,
    "Tags": ["ui", "utility"],
    "AcceptsFeedback": true,
    "RepoUrl": "https://github.com/OWNER/REPOSITORY",
    "IconUrl": "https://raw.githubusercontent.com/OWNER/REPOSITORY/main/assets/icon.png",
    "DownloadLinkInstall": "https://github.com/OWNER/REPOSITORY/releases/download/v1.0.0/MyPlugin.zip",
    "DownloadLinkUpdate": "https://github.com/OWNER/REPOSITORY/releases/download/v1.0.0/MyPlugin.zip"
  }
]
```

### 必填字段

| 字段 | 配置要求 |
| --- | --- |
| `Name` | 插件列表中显示的名称，不能为空。 |
| `InternalName` | 必须与 `manifest.json` 的 `id` 完全相同。 |
| `Version` | 必须是有效 SemVer，并与 Manifest 的 `version` 完全相同。 |
| `DownloadLinkInstall` | 可直接下载 ZIP 文件的 HTTPS 地址，不能填写 GitHub Release 页面地址。 |

### 建议字段

| 字段 | 配置要求 |
| --- | --- |
| `Author` | 作者或维护者名称。 |
| `Punchline` | 列表页的一句话简介。 |
| `Description` | 功能、用法和限制的完整说明。 |
| `Games` | 当前填写 `["nte"]`，并与 Manifest 的 `games` 保持一致。 |
| `ApiMajor` | ABI v1 插件填写 `1`，并与 Manifest 的 `api.major` 保持一致。 |
| `Tags` | 用于分类和检索的短标签。 |
| `RepoUrl` | 源码仓库或项目主页。 |
| `IconUrl` | 可公开直接访问的 HTTPS 图片地址；没有图标时删除该字段。 |
| `AcceptsFeedback` | 是否接受用户反馈。 |
| `DownloadLinkUpdate` | 可省略；保留时通常与 `DownloadLinkInstall` 相同。 |

下载 ZIP 的根目录必须直接包含 `manifest.json` 和 `plugin.dll`。`InternalName`、`Version`、`Games`、`ApiMajor` 或入口文件不一致时，安装器会拒绝该包。先上传版本化且不可变的 ZIP，再更新 `pluginmaster.json`，避免索引指向尚不存在的文件。

公开发布后，插件源地址通常是可直接读取该文件的 HTTPS URL，例如：

```text
https://raw.githubusercontent.com/OWNER/REPOSITORY/main/pluginmaster.json
```

## 发布 v1.0.0

仓库中的示例 Manifest、描述符和 `pluginmaster.json` 当前版本均为 `1.0.0`。发布前应：

1. 用 `anomaly-plugin validate` 和 `anomaly-test-host` 验证 Release 构建。
2. 只保留 `manifest.json`、`plugin.dll`、`locales/` 和实际运行时依赖，再用 `anomaly-plugin pack` 生成 `package.sha256`。
3. 压缩**包目录内的内容**，确保 ZIP 根部直接包含 `manifest.json`，不要额外嵌套一层目录。
4. 上传不可变的版本资源后，最后更新 `pluginmaster.json` 的版本与下载地址。

完整流程见 Anomaly 主仓库的 `docs/developer-guide/plugin-distribution.md`。

## 📚 参考

- **插件开发指南**：Anomaly 主仓库 `docs/developer-guide/plugin-development.md`
- **ABI v1 API 参考**：Anomaly 主仓库 `docs/api-reference/`
- **官方插件示例**：Anomaly 主仓库 `examples/`
- **UI 控件全集**：SDK 头文件 `include/anomaly/sdk/services/ui.h`（`AnomalyUiServiceV1`）
- **生命周期与描述符**：SDK 头文件 `include/anomaly/sdk/plugin.h`
- **轻量 C++ 封装**：SDK 头文件 `include/anomaly/sdk/cpp.hpp`（`Host::Query`、`UiWindow`）

## 🧾 开源许可

本模板以 [AGPL-3.0-only](LICENSE) 发布，与 Anomaly 生态保持一致。你可以在遵循许可证的前提下自由 fork、修改与分发。
