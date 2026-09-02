# guyi-bot 🐾

<p align="center">
  <img src="com.pixelomer.ShijimaQt.png" width="128" height="128" alt="guyi-bot Logo" />
</p>

<p align="center">
  <b>融合拟真物理模拟、好莱坞黑洞文件吞噬、AI Agent 协同生态、在线音乐工坊与环境感知的跨平台智能桌面桌宠机器人</b>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL%20v3-blue.svg" alt="License: GPL v3"></a>
  <a href="https://www.qt.io/"><img src="https://img.shields.io/badge/Qt-6.x-green.svg" alt="Qt 6"></a>
  <a href="https://music.gdstudio.xyz"><img src="https://img.shields.io/badge/Music-GD%E9%9F%B3%E4%B9%90%E5%8F%B0-ff69b4.svg" alt="GD音乐台"></a>
  <img src="https://img.shields.io/badge/Platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey.svg" alt="Platform">
</p>

---

## 📸 界面与动态效果展示 (Visual Tour)

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%">
      <b>🪟 活跃窗口吸附与自主攀爬漫步</b><br/><br/>
      <img src="docs/images/window-interaction.png" alt="窗口吸附与运动" width="100%" />
    </td>
    <td align="center" width="50%">
      <b>📊 桌宠实时属性与状态面板</b><br/><br/>
      <img src="docs/images/main-dashboard.png" alt="桌宠属性面板" width="100%" />
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <b>🎵 在线音乐工坊与滚动歌词（三大模式）</b><br/><br/>
      <img src="docs/images/music-workshop.png" alt="在线音乐工坊" width="100%" />
    </td>
    <td align="center" width="50%">
      <b>⏰ 定时任务管理与番茄钟</b><br/><br/>
      <img src="docs/images/timer-tasks.png" alt="定时任务" width="100%" />
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <b>💬 快捷 AI 提问窗口（⌥Q）</b><br/><br/>
      <img src="docs/images/ask-dialog.png" alt="AI 提问窗口" width="100%" />
    </td>
    <td align="center" width="50%">
      <b>🤖 大模型参数与 Agent 配置</b><br/><br/>
      <img src="docs/images/model-config.png" alt="模型与 Agent 配置" width="100%" />
    </td>
  </tr>
</table>

---

## 📢 致谢与开源声明 (Acknowledgments & License)

- 本项目 **`guyi-bot`** 基于开源项目 [Shijima-Qt](https://github.com/pixelomer/Shijima-Qt)（作者：[@pixelomer](https://github.com/pixelomer)）进行二次开发与深度定制扩展。
- 感谢原作者的优秀工作与开源精神。本项目遵循 **GNU General Public License v3.0 (GPLv3)** 开源发布。
- 本项目是在原 Shijima-Qt 基础框架上扩展了 AI Agent 协议适配、好莱坞星际黑洞文件吞噬、在线音乐工坊、定时任务番茄钟、状态胶囊、物理抛飞增强及桌面快捷交互。

---

## 🌟 核心特性概览

### 1. 🐾 拟真物理、窗口智能吸附与动态行为树
* **🪟 实时窗口感知与吸附（Window Server）**：
  * 接入底层 Window Server，实时感知外部应用（VS Code / Chrome / 访达 / 终端等）窗口位置；
  * 拖动桌宠至窗口顶栏或上沿放手，即刻**毫秒级磁吸落在窗口顶部漫步/坐下**；
  * 地面闲逛时会主动发现头顶窗口，**纵身起跳跃上窗口**或抓墙攀爬！
* **物理漫步与边界攀爬**：支持在屏幕边缘、底面、天花板攀爬、跳跃漫步，支持自由下落重力加速度；
* **物理抛掷交互**：鼠标左键抓起桌宠后快速甩出，可触发真实验算的自由抛飞与墙面反弹；
* **4 阶动态情绪与疲惫状态机**：
  * **⚡ 体力系统**：高耗能动作消耗体力，休息或睡眠时缓慢回血；体力耗尽原地休整；
  * **🌟 心情与亲密度**：开心、平静、无聊、烦躁 4 阶情绪循环；常伴互动积累亲密度。

### 2. 🪐 《星际穿越》卡冈图雅黑洞文件吞噬与大跳连环飞踢
* **电影级黑洞视效**：白金光子环、炽热横向金色吸积盘与 32 颗环绕吸入的光子流粒子，480x300 超宽无截断画布；
* **大跳奔赴与连环飞踢**：
  * 待处理文件出现时，桌宠在半空中空中飞跃大跳赶赴现场；
  * 到位后切换为**连环飞踢腿动作**，一脚一脚将文件猛力踹向黑洞事件视界，最终终极飞踢暴扣吞噬！
* **安全移入回收站**：跨平台对接 macOS（FSEvents 零轮询）与 Windows 回收站，支持随时还原。

### 3. 🤖 AI Agent 智能协同
* **前台开发感知**：实时感知前台应用切换，智能吐槽或鼓励；
* **Agent 任务协同**：深度适配 aipy-pro 与本地 Agent 开放接口，在任务执行进度与完成时做出动作反馈；
* **划词与快捷提问（⌥Q / ⌥T）**：随时选中文本唤起迷你快捷工具栏与 AI 提问卡片。

### 4. 🎵 在线音乐工坊
* **多音源搜索与播放**：支持全网多音源聚合搜索、无损高品质试听与外链降级容灾；
* **三大智能播放模式**：💖 熟悉模式（偏好与收藏推荐）、🚀 探索模式（全网雷达新歌发现）、🎲 随机模式；
* **滚动歌词与智能续播**：动态同步高亮当前歌词行，列表少于 3 首时异步自动推荐续播；
* **本地收藏库**：内置轻量 SQLite 歌曲库，一键收藏与持久化存储；
* **快捷热键**：按下 `⌥M`（Option + M）秒级呼出/收起音乐工坊卡片；
* 💡 **音源数据支持**：音乐数据及接口技术来自 **[GD音乐台 (music.gdstudio.xyz)](https://music.gdstudio.xyz)**。

### 5. ⏰ 定时任务与番茄钟
* 支持单次定时倒计时、循环 Cron 提醒与闹钟设定；
* 伴随式状态胶囊浮条显示剩余时间与当前倒计时状态。

---

## 🏗️ 项目架构（模块化设计）

```text
guyi-bot/
├── src/
│   ├── main.cc                                # 应用程序统一主入口
│   ├── core/                                  # 核心资产加载、XML 解析、默认形象、音频管理、CLI
│   ├── pet/                                   # 桌宠实体、行为树、情绪状态、记忆、事件总线、黑洞处置序列
│   ├── agent/                                 # AI Agent 服务、aipy-pro 适配器、RESTful HTTP API
│   ├── music/                                 # 音乐工坊、GD音乐台 API 客户端、SQLite 收藏库、播放控制
│   ├── timer/                                 # 定时任务、倒计时调度与管理界面
│   ├── system/                                # 系统感知（应用激活、休眠唤醒、废纸篓监听）与全局热键
│   └── ui/                                    # 紧凑气泡、黑洞视效、状态胶囊条、经验角标、划词工具栏与各功能弹窗
├── Platform/                                  # macOS / Linux / Windows 底层窗口系统绑定
├── DefaultMascot/                             # 内置默认桌宠动画帧与行为配置
├── docs/images/                               # 项目演示与功能界面截图
└── Makefile / common.mk                       # 跨平台构建系统
```

---

## ⌨️ 常用快捷键与交互

| 快捷键 / 操作 | 功能描述 |
| :--- | :--- |
| `⌥M` (Option + M) | 呼出 / 隐藏 **音乐工坊** 播放器 |
| `⌥Q` (Option + Q) | 呼出 **AI 快捷提问窗口** |
| `⌥T` (Option + T) | 呼出 **快捷翻译工具** |
| **鼠标左键拖拽** | 抓起桌宠并放置在屏幕任意位置或窗口顶部 |
| **快速甩掷** | 触发空中抛飞与物理碰撞反弹 |
| **鼠标右键** | 呼出桌宠动作交互、状态面板、黑洞吞噬与系统设置 |
| **拖拽文件至桌宠** | 触发黑洞吞噬与大跳连环飞踢文件清理 |

---

## 🛠️ 编译与运行构建

### 环境要求
* **C++ 编译器**：支持 C++17 标准（Clang / GCC / MSVC）
* **Qt 框架**：Qt 6.x（Widgets, Gui, Core, Multimedia, Concurrent, Network）
* **系统依赖**：`libarchive`、`sqlite3`

### 1. macOS 环境构建

```bash
# 1. 安装依赖（通过 Homebrew）
brew install qt@6 libarchive sqlite

# 2. 全量多核编译
make -j$(sysctl -n hw.ncpu)

# 3. 打包生成独立的 macOS 应用程序包 (.app)
make macapp

# 产物位于：
# publish/macOS/release/guyi-bot.app
```

### 2. Linux 环境构建

```bash
make CONFIG=release
```

---

## 📄 开源许可与致谢

* **项目名称**：guyi-bot
* **基础项目源自**：[Shijima-Qt](https://github.com/pixelomer/Shijima-Qt) (Copyright © 2023-2025 pixelomer)
* **二次开发作者**：guyi (Copyright © 2026 guyi)
* **音乐服务出处**：本软件在线音乐功能由 **[GD音乐台 (music.gdstudio.xyz)](https://music.gdstudio.xyz)** 提供数据与 API 技术支持。
* **开源协议**：本项目依据 [GNU General Public License v3.0 (GPLv3)](LICENSE) 协议开源发布。
