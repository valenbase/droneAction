# 当前环境使用说明

本文档用于当前 DroneAction 开发、部署和测试环境交接。敏感凭据不要写入仓库；远程机器登录密码请通过线下方式保存。

## 1. 环境概览

| 项目 | 当前值 |
| --- | --- |
| GitHub 仓库 | `https://github.com/valenbase/droneAction.git` |
| 本地开发目录 | `/Users/valen/WorkSpace/claudeCodeProjets/chongqing` |
| 远程 Linux 用户 | `tg` |
| 远程 Linux 地址 | `192.168.43.27` |
| 远程部署目录 | `/home/tg/Projects/drone_ws` |
| 固定 AppImage 路径 | `/home/tg/Projects/drone_ws/dist/droneAction.appimage` |
| UDP 控制监听默认端口 | `50050` |
| 网页控制面板默认端口 | `8765` |

## 2. 仓库与同步

本地查看状态：

```bash
cd /Users/valen/WorkSpace/claudeCodeProjets/chongqing
git status --short --branch
git rev-parse --short HEAD
```

远程查看状态：

```bash
ssh tg@192.168.43.27
cd /home/tg/Projects/drone_ws
git status --short --branch
git rev-parse --short HEAD
```

从本地同步源码到远程部署目录时，保留远程构建目录和 AppImage：

```bash
rsync -az --delete \
  --exclude build \
  --exclude build-mavsdk \
  --exclude dist \
  --exclude .appimage-tools \
  /Users/valen/WorkSpace/claudeCodeProjets/chongqing/ \
  tg@192.168.43.27:/home/tg/Projects/drone_ws/
```

不要把生成的 `.AppImage` 或 `.appimage` 文件提交进 Git。

## 3. 启动 AppImage

在远程 Linux 桌面环境中运行：

```bash
cd /home/tg/Projects/drone_ws
./dist/droneAction.appimage
```

启动后先选择运行模式：

- `Simulation`：用于本地仿真测试，默认允许远程命令执行，运动时长上限较长。
- `Real aircraft`：用于真实飞机，默认更保守，高风险动作需要确认，远程命令执行需要在设置中打开。

主界面右上角设置按钮可以配置：

- Vehicle Link
- RTSP
- RK3588 UDP
- Allowed IPs
- 最大速度、最大垂直速度、最大偏航角速度、最大指令时长
- 高风险动作确认
- 远程执行开关

运动类命令需要在主界面 `Safety Gate` 中打开 `Enable RK3588 movement control`。如果没有打开，RK3588 端会收到：

```json
{
  "accepted": false,
  "status": "rejected",
  "reason": "control authorization is disabled"
}
```

## 4. 网页按钮控制面板

按钮面板用于替代手动输入中文命令。它会向 DroneAction 的 UDP 控制端口发送 JSON 指令。

如果控制面板和 AppImage 都在远程 Linux 上运行：

```bash
cd /home/tg/Projects/drone_ws
python3 tools/rk3588_command_panel.py --target-host 127.0.0.1
```

终端会打印：

```text
DroneAction command panel: http://127.0.0.1:8765/
```

在远程机器浏览器打开该地址即可点击控制。

如果需要从其他电脑访问远程 Linux 上的控制面板：

```bash
cd /home/tg/Projects/drone_ws
python3 tools/rk3588_command_panel.py --bind 0.0.0.0 --target-host 127.0.0.1
```

然后在浏览器打开：

```text
http://192.168.43.27:8765/
```

面板支持直接点击：

- 起飞、降落、返航
- 前进、后退、左移、右移、悬停
- 升高、降低、左转、右转

界面上可以设置目标地址、目标端口、ACK 超时、运动时长、速度、垂直速度、偏航角速度和起飞高度。每次点击后，右侧会显示 request 和 ACK。

## 5. 命令行测试工具

发送自然语言测试命令：

```bash
cd /home/tg/Projects/drone_ws
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "前进 1米每秒 2秒"
```

只打印 JSON，不发送：

```bash
python3 tools/rk3588_command_cli.py --dry-run --once "左转 20度/秒 1秒"
```

发送精确 JSON：

```bash
python3 tools/rk3588_command_cli.py --json --once '{"version":1,"request_id":"rk-test","command":"hold","duration_ms":1000}'
```

常用示例：

```bash
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "起飞 2.5米"
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "前进 1米每秒 5秒"
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "悬停"
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "降落"
```

协议细节见：

```text
/home/tg/Projects/drone_ws/docs/PROTOCOL.md
```

## 6. 构建与测试

普通构建：

```bash
cd /home/tg/Projects/drone_ws
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

带远程命令执行支持的构建：

```bash
cd /home/tg/Projects/drone_ws
cmake -S . -B build-mavsdk -G Ninja -DCMAKE_BUILD_TYPE=Release -DDRONEACTION_ENABLE_MAVSDK=ON
cmake --build build-mavsdk
ctest --test-dir build-mavsdk --output-on-failure
```

当前远程机器已经保留两个构建目录：

- `/home/tg/Projects/drone_ws/build`
- `/home/tg/Projects/drone_ws/build-mavsdk`

通常修改代码后直接执行：

```bash
cd /home/tg/Projects/drone_ws
cmake --build build-mavsdk
ctest --test-dir build-mavsdk --output-on-failure
```

## 7. 打包 AppImage

使用带远程命令执行支持的构建目录打包：

```bash
cd /home/tg/Projects/drone_ws
BUILD_DIR=/home/tg/Projects/drone_ws/build-mavsdk scripts/package-appimage.sh
```

打包脚本行为：

- 删除 `dist/` 下旧的 `.AppImage` 和 `.appimage`
- 安装当前构建产物到 `AppDir`
- 使用 linuxdeploy 和 appimagetool 重新生成固定文件名
- 输出文件固定为 `dist/droneAction.appimage`

检查结果：

```bash
ls -lh /home/tg/Projects/drone_ws/dist
```

## 8. GitHub Actions

CI 文件：

```text
.github/workflows/build-appimage.yml
```

触发方式：

- push
- pull request
- workflow dispatch
- `v*` tag

普通 push / PR 会上传 workflow artifact。推送 tag，例如：

```bash
git tag v0.1.0
git push origin v0.1.0
```

会创建或更新 GitHub Release，并附加 `droneAction.appimage`。

## 9. 常见问题

### 9.1 CLI 或网页面板 ACK 超时

先确认 AppImage 已启动，并且 UDP 端口配置是 `0.0.0.0:50050` 或目标地址可达。

如果 AppImage 在同一台机器上：

```bash
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "悬停"
```

如果 RK3588 或测试电脑在另一台机器上，`--host` 应使用 PC 端 IP，例如：

```bash
python3 tools/rk3588_command_cli.py --host 192.168.43.27 --once "悬停"
```

### 9.2 返回 `control authorization is disabled`

主界面 `Safety Gate` 没有开启运动授权。勾选 `Enable RK3588 movement control` 后再发送运动命令。

### 9.3 返回 `aircraft telemetry is offline`

飞机状态没有在线。先确认运行模式、链路配置和状态显示。

### 9.4 返回 `needs_confirmation`

起飞、降落、返航等高风险动作需要 PC 端人工确认。RK3588 端不要高频自动重发。

### 9.5 运动时间不符合发送值

PC 端会对 `duration_ms` 做安全限幅。实际采用值会在 ACK 的 `duration_ms` 中返回，RK3588 端应以 ACK 为准。

### 9.6 AppImage 不是最新

重新打包：

```bash
cd /home/tg/Projects/drone_ws
BUILD_DIR=/home/tg/Projects/drone_ws/build-mavsdk scripts/package-appimage.sh
```

确认文件时间：

```bash
ls -lh /home/tg/Projects/drone_ws/dist/droneAction.appimage
```

## 10. 推荐交接检查清单

1. `git status --short --branch` 本地和远程都干净。
2. 本地和远程 `git rev-parse --short HEAD` 一致。
3. 远程 `build-mavsdk` 构建和测试通过。
4. `/home/tg/Projects/drone_ws/dist/droneAction.appimage` 存在且时间正确。
5. AppImage 启动后选择正确运行模式。
6. 主界面打开运动授权。
7. 网页面板能打开并收到 ACK。
8. `docs/PROTOCOL.md` 已交给 RK3588 开发人员。
