# 配置

AgentLauncher 采用配置化驱动。所有 agent 定义都放在单个 `agents.json` 文件中，首次
运行时拷贝到你的用户配置目录：

| 系统 | 路径 |
|---|---|
| Windows | `%LOCALAPPDATA%\AgentLauncher\agents.json` |
| Linux | `~/.config/AgentLauncher/agents.json` |
| macOS | `~/Library/Preferences/AgentLauncher/agents.json` |

应用内置了一份默认配置（来自 `config/default_agents.json`）。

## agent 条目

```json
{
    "id": "kimi-code",
    "name": "Kimi Code",
    "command": "kimi web",
    "webUrl": "http://127.0.0.1:58627",
    "configDir": "%USERPROFILE%/.kimi-code",
    "icon": "qrc:/icons/kimi-code.svg",
    "color": "#FF6B35"
}
```

| 字段 | 用途 |
|---|---|
| `id` | 稳定标识，UI 据此定位 agent |
| `name` | 卡片标题 |
| `command` | **启动**按钮执行的 shell 命令（以独立进程运行） |
| `webUrl` | 每隔几秒做健康检查、点击卡片时在浏览器打开的地址 |
| `configDir` | 配置页「打开」按钮打开的目录；支持 `%VAR%` 展开 |
| `icon` | 图标资源路径（如 `qrc:/icons/...`） |
| `color` | 运行中的高亮颜色（边框 + 背景着色） |

## 启动检测

每个 `webUrl` 每 3 秒做一次 HTTP 探测。**任意** HTTP 响应（哪怕是 `401`/`404`）
都视为服务已起 → 卡片标记为**运行中**。连接被拒或超时则视为**已停止**。

这种方式对所有 agent 都适用，因为它们的 web 服务一启动就暴露 HTTP 端点。

## 默认 agent

| Agent | 启动命令 | Web 地址 | 配置目录 |
|---|---|---|---|
| Kimi Code | `kimi web` | `http://127.0.0.1:58627` | `%USERPROFILE%/.kimi-code` |
| OpenCode | `opencode web --port 4096` | `http://127.0.0.1:4096` | `%USERPROFILE%/.config/opencode` |
| Qwen Code | `qwen serve` | `http://127.0.0.1:4170` | `%USERPROFILE%/.qwen` |

!!! note "OpenCode 默认随机端口"
    OpenCode 每次运行随机选取空闲端口，这会让健康检查和「浏览器打开」不可靠。
    默认配置固定为 `4096`（`--port` 参数与 Web 地址同时固定）。如 `4096` 被占用，
    可在配置页修改。

## 修改端口

进入某 agent 的**配置**页，编辑启动命令，例如：

```
kimi web --port 58628
```

然后把 **Web 地址**也改成对应值（`http://127.0.0.1:58628`）并**保存**。
两者分开存储以保留完全控制权——只需保持同步即可。

## 新增 agent

1. 打开配置目录下的 `agents.json`。
2. 在 `agents` 数组中追加一个填好所有字段的对象。
3. 重启 AgentLauncher（或下次启动时自动加载）。

无需重新编译。
