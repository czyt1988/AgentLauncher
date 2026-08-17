# 强制停止（Force Stop）功能设计

日期: 2026-08-17

## 目标

在每张卡片的右键菜单中新增 "Force Stop" 动作，允许用户停止当前通过 HTTP
健康检查判定为"运行中"的 agent——即使该 agent 不是由本启动器启动的（没有
记录的 PID）。

## 背景

现有 `AgentLauncher::stop(id)` 只能终止本启动器本次会话启动的进程（PID 记录
在 `m_pids` 中）。对非本启动器启动的 agent，`stop()` 会提示 "This agent
wasn't started from the launcher"。用户需要在这些情况下也能强制停止。

## 机制

由于没有 PID，改为基于端口定位：

1. 从 agent 的 `webUrl` 解析端口（如 `http://localhost:3000` → 3000；仅有
   scheme 时 http=80、https=443）。
2. Windows：运行 `cmd /c netstat -ano -p tcp`，逐行按空白拆分为列
   `proto / localAddress / foreignAddress / state / PID`。仅保留 state ==
   `LISTENING` 且 localAddress 以 `:<port>` 结尾的行，取最后一列为 PID。
   这样可避免误匹配 foreignAddress 中恰好出现 `:3000` 的已建立连接，也避免
   `:3000` 误匹配 `:53000`。可能得到多个 PID（IPv4/IPv6、0.0.0.0/127.0.0.1
   各一条），全部收集并去重。
3. 对每个 PID 执行 `taskkill /F /T /PID <pid>`（与现有 `stop()` 一致，杀
   整个进程树）。
4. 非 Windows：`lsof -ti :<port>` 得到 PID 列表，`kill -9` 逐个终止（与
   现有 `#else` 分支风格一致）。
5. 完成后触发一次健康检查（`checkAll`），卡片翻回 Stopped。

## API（C++）

- `Q_INVOKABLE void forceStop(const QString &id)`：执行上述流程。
- 私有辅助 `QList<qint64> findPidsForPort(int port)`：运行 netstat/lsof，
  解析返回 PID 列表（同步执行，netstat 很快）。
- 私有辅助 `int portFromWebUrl(const QString &webUrl) const`：用 QUrl 解析
  端口，无端口时按 scheme 给默认值（http=80/https=443），无法解析返回 -1。
- 失败时（无端口 / netstat 找不到监听 / taskkill 失败）
  `emit launchFailed(id, …)`，复用现有红色闪烁 + 中央错误弹窗。
- 成功终止后 `QTimer::singleShot(500, …, &AgentLauncher::checkAll)`，与
  `stop()` 一致。

## UI（QML）

- `AgentCard.qml` 右键菜单新增 `MenuItem`，文案 "Force Stop"，
  `visible: root.running_p`，置于 Close/Start 之后。
- 点击后先弹出确认框（**已确认：每次都确认**）。确认 Popup 直接放在
  `AgentCard.qml` 内部（自包含，与该卡片的 flashTimer 等局部状态一致），
  样式参照 `main.qml` 中的 `exitConfirmPopup`（Catppuccin Mocha 配色、
  `#f38ba8` 边框表示危险动作）。设 `parent: Overlay.overlay` +
  `anchors.centerIn: parent` 使其相对整个应用窗口居中，而非挤在 260×230
  的卡片内部。
- 确认文案："Force stop %1? This will terminate the process serving %2."
  其中 %1 = agent 名称，%2 = webUrl（含端口，信息充分且无需在 QML 重复
  解析端口）。按钮：Force Stop / Cancel。
- 确认后调用 `launcher.forceStop(root.agentId_p)`，并设置
  `root.stopping = true` 显示 spinner（已有逻辑在 `running_p` 变 false 时
  清除 stopping）。
- 卡片新增 `property string webUrl_p: webUrl` 别名（与现有 `name_p` 等
  别名风格一致），供确认文案使用。

## i18n

所有新字符串用英文 `tr()`/`qsTr()`（源语言英文），中文翻译写入
`translations/agentlauncher_zh_CN.ts`。新增字符串：

- `qsTr("Force Stop")`（菜单项）
- `qsTr("Force Stop")`（确认框标题）
- `qsTr("Force stop %1? This will terminate the process serving %2.")`
- `qsTr("Force Stop")`（确认按钮）
- `qsTr("Cancel")`（取消按钮）
- `tr("Cannot determine port from web URL.")`
- `tr("No process found listening on port %1; the agent may already be stopped.")`
- `tr("Failed to stop process (PID %1).")`（复用现有文案）

## 边界情况

- `webUrl` 为空或无法解析端口 → `launchFailed`：
  "Cannot determine port from web URL."
- netstat 未找到该端口的监听进程 → `launchFailed`：
  "No process found listening on port %1; the agent may already be stopped."
- 多个 PID（IPv4 + IPv6）→ 全部 taskkill，只要有一个成功即视为成功。
- taskkill 全部失败 → `launchFailed` 带退出码/输出。
- agent 实际已停止（健康检查短暂为 true 后变 false）→ forceStop 仍会尝试，
  netstat 找不到监听即提示"may already be stopped"。

## 不在范围内

- 不修改现有 `stop()` / × 按钮行为（保持 PID 追踪方式）。
- 不持久化 force-stop 相关状态。
- 不增加进程嗅探；保持端口 + netstat 方式。
