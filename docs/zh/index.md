# AgentLauncher

一个用 **Qt6/QML + C++** 开发的桌面小工具，从同一个卡片网格快速启动多个 AI 编码
agent 的 **Web 端**（Kimi Code、OpenCode、Qwen Code），并一键打开它们的配置目录。
所有内容**配置化驱动**——新增 agent 只需编辑一个 JSON 文件，无需改代码。

## 为什么需要

每个 AI 编码 agent 都有自己的命令行工具和启动本地 web 服务的方式，默认端口不同、
配置目录布局也不同，逐个记住命令很繁琐。AgentLauncher 给你一个统一入口，启动任意
一个并直接跳到它的 web 界面。

## 功能

- 首页以**卡片网格**展示所有已配置的 agent。
- **启动**按钮启动 agent 的 web 服务（以独立进程运行）。
- **配置**按钮进入该 agent 的设置页（编辑命令 / web 地址、打开配置目录）。
- 通过 HTTP 健康检查检测**已启动状态**；运行中的卡片用 agent 专属颜色做边框高亮 + 背景着色。
- 点击**已启动**的卡片即用默认浏览器打开其 web 端。
- **完全配置化**，统一写在 `agents.json`。

## 快速开始

```bash
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/msvc2019_64"
cmake --build build
./build/AgentLauncher
```

添加你自己的 agent 见[配置](configuration.md)，构建细节见[开发](development.md)。
