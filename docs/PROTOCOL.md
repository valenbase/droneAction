# RK3588 UDP JSON 控制协议

本文档面向 RK3588 端开发者，描述 RK3588 与 PC 端 DroneAction 应用之间的 UDP JSON 控制协议。RK3588 只需要按本文档发送 JSON 指令并解析 ACK，不需要关心 PC 端内部如何连接飞机。

## 1. 通信方式

- 传输协议：UDP
- 默认 PC 端监听地址：`0.0.0.0`
- 默认 PC 端监听端口：`50050`
- 编码：UTF-8
- 数据格式：单个 UDP datagram 内包含一个 JSON object
- 推荐 datagram 大小：小于 `4096` 字节
- ACK：PC 端收到每条 datagram 后，会向 datagram 的源 IP 和源端口返回一个 UDP JSON ACK。需要人工确认的动作在确认、超时或被替换后还会发送第二个最终 ACK

RK3588 端应独立接收并核对 ACK，持续目标的发送线程不能等待 ACK。当前 ACK 代表“收到并完成安全检查/入队”，不是模式切换或动作完成回执。

## 2. 请求格式

标准请求：

```json
{
  "version": 1,
  "request_id": "rk-000001",
  "session_id": "control-20260910-001",
  "sequence": 1,
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
| `session_id` | string | 持续控制建议 | 一次控制连接的标识；与 `sequence` 同时提供。新会话必须从序号 `1` 开始。 |
| `sequence` | integer | 持续控制建议 | 会话内严格递增的正整数，范围 `1` 到 `9007199254740991`；与 `session_id` 同时提供。 |
| `command` | string | 是 | 控制命令。建议使用英文规范命令，见第 4 节。 |
| `timestamp_ms` | integer | 建议 | RK3588 发送时间，Unix epoch 毫秒。PC 端当前只记录，不参与校验。 |
| `value` | number | 部分命令需要 | 速度、角速度或起飞高度。 |
| `unit` | string | 建议 | `m/s`、`deg/s`、`m`。PC 端当前主要透传到 ACK。 |
| `duration_ms` | integer | 运动命令建议 | 当前目标的最长有效期，单位毫秒。持续运动仍需周期发送更新。 |

`session_id` 和 `sequence` 必须同时出现或者同时省略。为兼容旧控制器，两者都省略时仍可执行，但PC无法识别重复、乱序和旧会话数据，不建议用于真机持续控制。

PC 的首次会话允许从收到的第一个有效序号开始，容忍首包丢失；从已知会话切换到另一个会话时要求首包序号为 `1`。会话记录按发送方 IP 保存，序号检查通过后即消耗该序号，即使后续安全检查拒绝了指令。`timestamp_ms` 尚未用于过期校验，因此序号保护不能识别所有仍按顺序到达的延迟数据；断流计时使用 PC 的单调时钟和最近有效目标的接收时刻。

## 3. ACK 格式

安全检查通过且命令已提交执行队列：

```json
{
  "version": 1,
  "request_id": "rk-000001",
  "session_id": "control-20260910-001",
  "sequence": 1,
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
| `session_id` | string | 请求提供控制会话时原样回填。 |
| `sequence` | integer | 请求提供序号时原样回填。 |
| `command` | string | PC 端识别后的规范命令。解析失败时可能没有该字段。 |
| `accepted` | boolean | 只有 `true` 才表示已通过安全检查并尝试提交执行。 |
| `status` | string | `accepted`、`rejected`、`needs_confirmation`。 |
| `reason` | string | 人类可读原因，RK3588 可记录到日志。 |
| `value` | number | PC 端实际采用的值，可能被限幅。 |
| `unit` | string | 单位。 |
| `duration_ms` | integer | PC 端实际采用的运动时长，可能被限幅。 |
| `execution_status` | string | 已尝试提交执行时出现，失败时也会出现，用于区分拒绝原因。 |
| `execution_reason` | string | 执行提交状态说明。 |

`execution_status` 当前可能值：

| 值 | 含义 |
| --- | --- |
| `queued` | 命令已进入 PC 端执行流程。 |
| `disabled` | 当前运行配置未启用远程命令执行。 |
| `unavailable` | 当前应用构建不支持命令执行。 |
| `not_ready` | 飞机连接或状态还未准备好。 |
| `busy` | 正在安全退出外部控制，暂时不能接受新的动作。 |
| `manual_takeover` | 检测到人工切换模式，必须重新进行运动控制授权。 |

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
| `hold` | `悬停` | 可省略 | 可省略 | 可省略 | 置零并安全退出到位置控制模式 |

运动命令默认值和限幅：

| 类别 | 未提供 `value` 时默认值 | 限幅来源 |
| --- | --- | --- |
| 水平运动 | `0.8 m/s` | PC 端 `max_velocity_mps` |
| 垂直运动 | `0.5 m/s` | PC 端 `max_vertical_mps` |
| 偏航运动 | `20 deg/s` | PC 端 `max_yaw_deg_s` |
| 运动时长 | `1000 ms` | PC 端 `max_command_duration_ms` |

时长下限固定为 `100 ms`。`duration_ms` 只限制当前数据包中的目标可以使用多久，不能代替周期更新。PC 端会在 ACK 中返回实际采用的 `value` 和 `duration_ms`，RK3588 应以 ACK 为准。

PC 端设置文件中的连续控制参数：

| 配置项 | 默认值 | 含义 |
| --- | ---: | --- |
| `command_stale_timeout_ms` | `500` | 未收到有效更新后改发零速度。 |
| `control_exit_timeout_ms` | `2000` | 持续断流后请求退出到位置控制模式。 |
| `control_entry_prestream_ms` | `1200` | 请求进入外部控制前的目标预发送时间。 |
| `mode_confirmation_timeout_ms` | `3000` | 等待进入模式确认的时间。 |
| `mode_exit_confirmation_timeout_ms` | `3000` | 每次等待退出确认的时间；超时后保持零速度并重试。 |

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

PX4 持续控制期间的起飞、降落和返航会返回 `busy`。应先发送 `hold`，并通过遥测确认已退出持续控制，再请求高风险动作。不要把 `hold` 的入队 ACK 当作退出完成通知。

PC 端会保存最新一条待确认指令 15 秒。操作员在界面确认后，PC 会向原发送地址再发一个最终 ACK；成功提交时示例：

```json
{
  "version": 1,
  "request_id": "rk-000003",
  "command": "takeoff",
  "accepted": true,
  "status": "accepted",
  "reason": "operator confirmed high-risk action",
  "execution_status": "queued",
  "execution_reason": "command queued for execution"
}
```

15 秒未确认或被更新的待确认动作替换时，第二个 ACK 为 `accepted=false`。发送端如果已关闭 UDP socket，可能收不到第二个 ACK，因此动作结果还应结合业务状态或遥测判断。

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

### 6.1 运动目标更新

```text
RK3588 -> PC: 每100ms发送一次最新运动目标
RK3588 <- PC: accepted=true, status=accepted, execution_status=queued
RK3588: 停止运动时发送hold，或者持续发送零速度目标
```

建议运动目标更新频率为 `10 Hz`，最低不要低于 `5 Hz`。更新周期应明显小于 PC 配置的 `command_stale_timeout_ms`。

本文的长期会话、两级断流和人工接管锁定目前针对 PX4 后端实现。APM 后端保留原有 Guided 流程，不能假定具备同样的连续控制语义。`Position` 模式仍响应遥控器输入，摇杆居中且定位有效时才保持位置。

### 6.2 ACK 超时

建议 RK3588 端：

- 默认 ACK 超时：`2s`
- 最多重试：`1` 到 `2` 次
- 重试时使用新的 `request_id`，或在业务日志中明确标记为重试
- 对 `takeoff`、`land`、`rtl` 不建议自动重试，除非上层业务明确允许

PC端不会使用 `request_id` 去重。提供 `session_id + sequence` 后，PC会拒绝同一会话中的重复或倒序数据，以及已结束会话中的延迟数据。危险动作仍不应高频重发。

### 6.3 连续控制

持续控制必须周期发送最新目标。下面的数据包应按固定频率重复发送，每次递增 `sequence`，并使用新的 `request_id` 和 `timestamp_ms`：

```json
{
  "version": 1,
  "request_id": "rk-forward-001",
  "session_id": "control-20260910-001",
  "sequence": 1,
  "command": "forward",
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 1000,
  "timestamp_ms": 1780732800000
}
```

如果需要中途停止，发送：

```json
{
  "version": 1,
  "request_id": "rk-hold-001",
  "session_id": "control-20260910-001",
  "sequence": 51,
  "command": "hold",
  "duration_ms": 1000,
  "timestamp_ms": 1780732801000
}
```

PC 端会根据心跳自动选择控制方式。PX4 飞机只建立一个长期控制会话，后续数据包仅更新目标，不会反复切换模式。更新中断达到第一级超时后发送零速度，达到第二级超时后请求位置控制模式；在遥测确认模式切换前仍持续发送零速度。操作员手动切换到其他模式后，控制会话立即停止并锁定，不会自动切回；需要在主界面关闭再重新开启运动授权。

## 7. 示例

### 7.1 前进 1 m/s，持续 2 秒

以下是单个更新包的示例，持续运动必须周期刷新。首次进入控制模式需要预发送及确认时间，测试 CLI 的发送时长包含这段时间；真实控制系统应以模式反馈确定实际运动计时起点。

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
2. 运动目标以固定频率发送，推荐 `10 Hz`；复杂计算不能阻塞控制发送线程。
3. 每次启动控制生成新的 `session_id`，会话内的 `sequence` 从 `1` 开始并严格递增。
4. 每条请求生成唯一 `request_id`，格式可用 `rk-<timestamp>-<seq>`。
5. 异步核对首包及状态变化的 ACK；持续更新仍保持固定发送频率，并记录完整日志。
6. 判断执行提交成功时，同时满足：
   - `accepted == true`
   - `status == "accepted"`
   - `execution_status == "queued"`
7. `value` 和 `duration_ms` 以 ACK 返回值为准，因为 PC 端可能做了安全限幅。
8. 收到 `rejected` 时不要盲目重试，应先根据 `reason` 处理。
9. 收到 `needs_confirmation` 时等待人工确认或上层业务再次触发。
10. 高风险命令不要做高频自动重发。

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
