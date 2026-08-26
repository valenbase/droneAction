# APM/ArduCopter 使用说明

DroneAction 会从飞控心跳自动识别 ArduPilot。遥测显示与原有界面共用，运动指令则自动切换到 ArduCopter 的 Guided 控制流程，不需要 RK3588 修改 UDP JSON 协议。

当前运动控制面向 ArduCopter 多旋翼。固定翼、车和船可以显示遥测，但不会执行本页描述的多旋翼速度控制。

## 1. Ubuntu 主机

| 项目 | 当前值 |
| --- | --- |
| 主机 | `valen@192.168.0.12` |
| 工程 | `/home/valen/Projects/drone_ws` |
| AppImage | `/home/valen/Projects/drone_ws/dist/droneAction.appimage` |
| ArduPilot 源码 | `/home/valen/WorkSpace/drone/ardupilot` |
| RK3588 指令端口 | `50050/UDP` |

## 2. 启动 ArduCopter SITL

首次构建：

```bash
cd /home/valen/WorkSpace/drone/ardupilot
./waf configure --board sitl
./waf copter -j2
```

启动无界面 SITL：

```bash
cd /home/valen/WorkSpace/drone/ardupilot
Tools/autotest/sim_vehicle.py \
  -v ArduCopter \
  -f quad \
  -N \
  --no-mavproxy \
  --use-dir=/tmp/droneaction-apm-sitl
```

该方式默认在 `TCP 5760` 等待地面端连接。

## 3. AppImage 仿真配置

```bash
cd /home/valen/Projects/drone_ws
./dist/droneAction.appimage
```

1. 启动时选择 `Simulation`。
2. 打开右上角设置。
3. 将 `Vehicle Link` 改为 `tcp://127.0.0.1:5760`。
4. 保持 `Enable command execution` 开启，保存设置。
5. 点击主界面 `Start`。
6. 状态应从离线变为在线，模式应显示 `Stabilized`、`Guided control`、`Land` 等实际模式。
7. 发送平移、升降或偏航指令前，开启主界面的运动授权复选框。

仿真刚启动时需要等待 GPS 和状态估计就绪。过早起飞会被飞控拒绝，事件日志会显示飞控返回的具体原因，等待状态稳定后再重新发送一次。

## 4. 真机配置

1. 启动时选择 `Real aircraft`。
2. 在设置中填写 G12 实际共享出来的飞控链路地址。当前默认值为 `tcp://192.168.43.1:5760`。
3. 如果 G12 以 UDP 主动发送到电脑，使用监听形式，例如 `udp://:14550`，端口按 G12 配置填写。
4. 确认遥测在线且模式、姿态、GPS、高度等数据持续更新。
5. 在设置中显式开启命令执行；需要运动时，再开启主界面的运动授权。
6. 起飞、降落、返航默认要求人工确认。收到指令后，主界面确认按钮会显示待执行动作，15 秒内点击才会提交。

第一次真机测试应卸桨进行链路、模式切换和拒绝场景验证。装桨测试时应保留遥控器模式切换和紧急接管能力。

## 5. 指令执行流程

运动指令的闭环流程：

```text
收到 UDP JSON 指令
  -> 检查遥测在线和操作员运动授权
  -> 检查当前模式
  -> 已在 Guided：直接进入目标发送
  -> 不在 Guided：请求切换并等待心跳确认
  -> 以 10 Hz 持续发送机体系速度目标
  -> 达到 duration_ms 后发送零速度
```

如果操作员在动作期间从遥控器切走 Guided，应用检测到心跳模式变化后会停止本次动作，不会自动抢回模式。新的运动指令到来时，旧动作会先被取消。

起飞流程：

```text
确认 Guided
  -> 请求解锁
  -> 等待心跳确认已解锁
  -> 发送起飞高度
  -> 检查飞控应答
```

降落和返航会直接发送相应飞行指令并检查飞控应答。

## 6. RK3588 测试

JSON 协议与原版本相同：

```bash
cd /home/valen/Projects/drone_ws
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "起飞 3米"
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "前进 1米每秒 2秒"
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "降落"
```

按钮面板：

```bash
python3 tools/rk3588_command_panel.py --target-host 127.0.0.1
```

完整字段和 ACK 规则见 `docs/PROTOCOL.md`。

## 7. 开发联调探针

该程序直接调用 AppImage 使用的同一个遥测和控制服务，自动执行起飞、前进和降落。只用于 SITL，不会打包进 AppImage。

```bash
cd /home/valen/Projects/drone_ws
build/tests/apm_integration_probe tcp://127.0.0.1:5760
```

成功时输出 `RESULT PASS`，并打印测试中检测到的最大地速。
