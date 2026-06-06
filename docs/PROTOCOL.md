# RK3588 UDP JSON 控制协议

本文档面向 RK3588 端开发者，描述 RK3588 与 PC 端 DroneAction 应用之间的 UDP JSON 控制协议。RK3588 只需要按本文档发送 JSON 指令并解析 ACK，不需要关心 PC 端内部如何连接飞机。

## 1. 通信方式

- 传输协议：UDP
- 默认 PC 端监听地址：`0.0.0.0`
- 默认 PC 端监听端口：`50050`
- 编码：UTF-8
- 数据格式：单个 UDP datagram 内包含一个 JSON object
- 推荐 datagram 大小：小于 `4096` 字节
- ACK：PC 端收到每条 datagram 后，会向 datagram 的源 IP 和源端口返回一个 UDP JSON ACK

RK3588 端建议每条指令都等待 ACK。当前 ACK 代表“收到并完成安全检查/入队”，不是动作完成回执。

## 2. 请求格式

标准请求：

```json
{
  "version": 1,
  "request_id": "rk-000001",
  "command": "forward",
  "timestamp_ms": 1780732800000,
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 2000
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `version` | integer | 是 | 协议版本。当前固定为 `1`。 |
| `request_id` | string | 强烈建议 | RK3588 生成的请求 ID，用于日志和 ACK 对应。建议全局唯一。 |
| `command` | string | 是 | 控制命令。建议使用英文规范命令，见第 4 节。 |
| `timestamp_ms` | integer | 建议 | RK3588 发送时间，Unix epoch 毫秒。PC 端当前只记录，不参与校验。 |
| `value` | number | 部分命令需要 | 速度、角速度或起飞高度。 |
| `unit` | string | 建议 | `m/s`、`deg/s`、`m`。PC 端当前主要透传到 ACK。 |
| `duration_ms` | integer | 运动命令建议 | 运动持续时间，单位毫秒。PC 端会做上下限限制。 |

额外字段当前会被忽略，可以预留给后续扩展。

## 3. ACK 格式

安全检查通过且命令已提交执行队列：

```json
{
  "version": 1,
  "request_id": "rk-000001",
  "command": "forward",
  "accepted": true,
  "status": "accepted",
  "reason": "command passed safety gate",
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 2000,
  "execution_status": "queued",
  "execution_reason": "command queued for execution"
}
```

被拒绝：

```json
{
  "version": 1,
  "request_id": "rk-000002",
  "command": "forward",
  "accepted": false,
  "status": "rejected",
  "reason": "control authorization is disabled",
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 2000
}
```

需要 PC 端人工确认：

```json
{
  "version": 1,
  "request_id": "rk-000003",
  "command": "takeoff",
  "accepted": false,
  "status": "needs_confirmation",
  "reason": "high-risk action requires UI confirmation"
}
```

ACK 字段说明：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `version` | integer | 协议版本，当前为 `1`。 |
| `request_id` | string | 从请求中回填。解析失败时可能没有该字段。 |
| `command` | string | PC 端识别后的规范命令。解析失败时可能没有该字段。 |
| `accepted` | boolean | 只有 `true` 才表示已通过安全检查并尝试提交执行。 |
| `status` | string | `accepted`、`rejected`、`needs_confirmation`。 |
| `reason` | string | 人类可读原因，RK3588 可记录到日志。 |
| `value` | number | PC 端实际采用的值，可能被限幅。 |
| `unit` | string | 单位。 |
| `duration_ms` | integer | PC 端实际采用的运动时长，可能被限幅。 |
| `execution_status` | string | 只有 `accepted=true` 时出现，表示执行提交状态。 |
| `execution_reason` | string | 执行提交状态说明。 |

`execution_status` 当前可能值：

| 值 | 含义 |
| --- | --- |
| `queued` | 命令已进入 PC 端执行流程。 |
| `disabled` | 当前运行配置未启用远程命令执行。 |
| `unavailable` | 当前应用构建不支持命令执行。 |
| `not_ready` | 飞机连接或状态还未准备好。 |

注意：如果 `accepted=true` 但 `execution_status` 不是 `queued`，RK3588 端应认为该动作没有实际执行。

## 4. 命令列表

RK3588 端建议统一发送英文规范命令。中文别名仅用于人工测试兼容。

### 4.1 运动命令

运动命令需要 PC 端 UI 已开启运动授权，并且飞机状态在线。

| 命令 | 中文别名 | `value` | `unit` | `duration_ms` | 行为 |
| --- | --- | --- | --- | --- | --- |
| `forward` | `前进` | 速度，正数 | `m/s` | 建议 | 机体系前进 |
| `backward` | `后退` | 速度，正数 | `m/s` | 建议 | 机体系后退 |
| `left` | `左移` | 速度，正数 | `m/s` | 建议 | 机体系左移 |
| `right` | `右移` | 速度，正数 | `m/s` | 建议 | 机体系右移 |
| `up` | `升高` | 速度，正数 | `m/s` | 建议 | 垂直上升 |
| `down` | `降低` | 速度，正数 | `m/s` | 建议 | 垂直下降 |
| `yaw_left` | `左转` | 角速度，正数 | `deg/s` | 建议 | 向左偏航 |
| `yaw_right` | `右转` | 角速度，正数 | `deg/s` | 建议 | 向右偏航 |
| `hold` | `悬停` | 可省略 | 可省略 | 可省略 | 发送零运动控制 |

运动命令默认值和限幅：

| 类别 | 未提供 `value` 时默认值 | 限幅来源 |
| --- | --- | --- |
| 水平运动 | `0.8 m/s` | PC 端 `max_velocity_mps` |
| 垂直运动 | `0.5 m/s` | PC 端 `max_vertical_mps` |
| 偏航运动 | `20 deg/s` | PC 端 `max_yaw_deg_s` |
| 运动时长 | `1000 ms` | PC 端 `max_command_duration_ms` |

时长下限固定为 `100 ms`。PC 端会在 ACK 中返回实际采用的 `value` 和 `duration_ms`，RK3588 应以 ACK 为准。

### 4.2 高风险命令

| 命令 | 中文别名 | `value` | `unit` | 行为 |
| --- | --- | --- | --- | --- |
| `takeoff` | `起飞` | 可选，起飞高度 | `m` | 起飞 |
| `land` | `降落` | 不需要 | 可省略 | 降落 |
| `rtl` | `返航` | 不需要 | 可省略 | 返回起点 |

高风险命令在真机配置下默认需要 PC 端 UI 人工确认，ACK 会返回：

```json
{
  "accepted": false,
  "status": "needs_confirmation",
  "reason": "high-risk action requires UI confirmation"
}
```

RK3588 端收到 `needs_confirmation` 时，不应自动重发同一指令刷屏，应等待人工确认或业务侧重新触发。

### 4.3 预留命令

以下命令当前能被识别，但 v1 未绑定任务，都会被拒绝：

| 命令 | 说明 |
| --- | --- |
| `plan_a` / `A计划` | 预留任务 A |
| `plan_b` / `B计划` | 预留任务 B |
| `0` 到 `9` | 预留数字任务 |

拒绝原因：

```json
{
  "status": "rejected",
  "reason": "mission command is not bound in v1"
}
```

## 5. 错误与拒绝原因

常见 `reason`：

| reason | 触发条件 | RK3588 建议处理 |
| --- | --- | --- |
| `invalid JSON command` | datagram 不是合法 JSON object | 修复编码/格式。 |
| `unsupported command version` | `version` 不是 `1` | 降级或更新协议。 |
| `missing command` | 缺少 `command` | 修复请求构造。 |
| `unknown command: ...` | 命令不在枚举内 | 修复命令名称。 |
| `sender IP is not allowed` | PC 端设置了 IP 白名单且当前源 IP 不在名单内 | 检查网络和 PC 端配置。 |
| `control authorization is disabled` | PC 端 UI 未开启运动授权 | 提示操作员开启授权。 |
| `aircraft telemetry is offline` | 飞机状态离线 | 等待连接恢复后再发。 |
| `high-risk action requires UI confirmation` | 高风险命令需要人工确认 | 等待人工确认，不要高频重试。 |
| `mission command is not bound in v1` | 任务类命令尚未绑定 | 暂不要使用该命令。 |

解析失败时，ACK 可能只有：

```json
{
  "version": 1,
  "accepted": false,
  "status": "rejected",
  "reason": "invalid JSON command"
}
```

因此 RK3588 端不能假设 ACK 一定包含 `request_id` 和 `command`。

## 6. 推荐时序

### 6.1 单条运动指令

```text
RK3588 -> PC: command=forward, value=1.0, duration_ms=2000
RK3588 <- PC: accepted=true, status=accepted, execution_status=queued
RK3588: 记录 ACK，等待下一次业务触发
```

### 6.2 ACK 超时

建议 RK3588 端：

- 默认 ACK 超时：`2s`
- 最多重试：`1` 到 `2` 次
- 重试时使用新的 `request_id`，或在业务日志中明确标记为重试
- 对 `takeoff`、`land`、`rtl` 不建议自动重试，除非上层业务明确允许

当前 PC 端没有基于 `request_id` 的去重机制，所以 RK3588 不应对同一危险动作做高频重发。

### 6.3 连续控制

如果需要持续移动，不建议每 `100ms` 高频发送短命令。更推荐发送一条带明确 `duration_ms` 的运动命令，例如：

```json
{
  "version": 1,
  "request_id": "rk-forward-001",
  "command": "forward",
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 5000,
  "timestamp_ms": 1780732800000
}
```

如果需要中途停止，发送：

```json
{
  "version": 1,
  "request_id": "rk-hold-001",
  "command": "hold",
  "duration_ms": 1000,
  "timestamp_ms": 1780732801000
}
```

## 7. 示例

### 7.1 前进 1 m/s，持续 2 秒

Request:

```json
{
  "version": 1,
  "request_id": "rk-forward-001",
  "command": "forward",
  "timestamp_ms": 1780732800000,
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 2000
}
```

ACK:

```json
{
  "version": 1,
  "request_id": "rk-forward-001",
  "command": "forward",
  "accepted": true,
  "status": "accepted",
  "reason": "command passed safety gate",
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 2000,
  "execution_status": "queued",
  "execution_reason": "command queued for execution"
}
```

### 7.2 左转 20 deg/s，持续 1 秒

```json
{
  "version": 1,
  "request_id": "rk-yaw-001",
  "command": "yaw_left",
  "timestamp_ms": 1780732800000,
  "value": 20,
  "unit": "deg/s",
  "duration_ms": 1000
}
```

### 7.3 起飞到 2.5 m

```json
{
  "version": 1,
  "request_id": "rk-takeoff-001",
  "command": "takeoff",
  "timestamp_ms": 1780732800000,
  "value": 2.5,
  "unit": "m"
}
```

### 7.4 降落

```json
{
  "version": 1,
  "request_id": "rk-land-001",
  "command": "land",
  "timestamp_ms": 1780732800000
}
```

## 8. RK3588 端实现建议

1. 使用英文规范命令，不使用中文别名作为设备间协议字段。
2. 每条请求生成唯一 `request_id`，格式可用 `rk-<timestamp>-<seq>`。
3. 每条 UDP 请求等待 ACK，并把 request/ACK 全量记录到日志。
4. 判断执行提交成功时，同时满足：
   - `accepted == true`
   - `status == "accepted"`
   - `execution_status == "queued"`
5. `value` 和 `duration_ms` 以 ACK 返回值为准，因为 PC 端可能做了安全限幅。
6. 收到 `rejected` 时不要盲目重试，应先根据 `reason` 处理。
7. 收到 `needs_confirmation` 时等待人工确认或上层业务再次触发。
8. 高风险命令不要做高频自动重发。

## 9. 测试工具

命令行发送：

```bash
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "前进 1米每秒 2秒"
```

打印 JSON 但不发送：

```bash
python3 tools/rk3588_command_cli.py --dry-run --once "左转 20度/秒 1秒"
```

发送精确 JSON：

```bash
python3 tools/rk3588_command_cli.py --json --once '{"version":1,"request_id":"rk-test","command":"hold","duration_ms":1000}'
```

浏览器按钮面板：

```bash
python3 tools/rk3588_command_panel.py --target-host 127.0.0.1
```

如果从其他电脑访问 RK3588 或 PC 上的面板：

```bash
python3 tools/rk3588_command_panel.py --bind 0.0.0.0 --target-host 127.0.0.1
```
