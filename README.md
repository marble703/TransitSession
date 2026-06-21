# TransitSession

`TransitSession` 是一个基于 C++20 和 Boost.Asio 的异步通信库，设计目标是分离**异步调度**和**协议读写**，当前提供三类传输能力：

- **serial**：面向串口的字节流会话
- **can**：面向 Linux SocketCAN 的 CAN 帧会话
- **websocket**：面向 WebSocket client 的消息会话


## 特性

- C++20
- 基于 Boost.Asio 的异步收发
- 统一异步调度核心，便于后续扩展

## 目录结构

- `core/`
  - 通用异步调度核心
  - `async_transport_session.hpp` 当前接口
  - `async_session.hpp` 已废弃
- `transport/serial/`
  - 串口配置与串口会话
- `transport/can/`
  - CAN 帧、配置与 SocketCAN 会话
- `transport/websocket/`
  - WebSocket 配置、客户端会话与服务端监听
- `example/`
  - 示例程序
  - benchmark 小工具

## 构建

工程使用 CMake 构建。

```bash
cmake -S . -B build
cmake --build build -j
```

## Serial

当前 Serial 支持字节流式异步收发，读取回调按当前一次 `async_read_some` 收到的数据触发。

示例：

```bash
./build/TransitSession
```

Benchmark：

```bash
./build/TransitSessionSerialBenchmark --device /dev/ttyACM0 --baud 115200 --payload 32 --samples 1000
```

说明：

- 示例默认使用 `/dev/ttyACM0`
- benchmark 需要对端做“收到什么就回什么”的回显

## CAN

当前 CAN 支持 Linux SocketCAN，读取回调按单帧触发，兼容 classic CAN 和 CAN FD。

示例：

```bash
./build/TransitSessionCan
```

Benchmark：

```bash
./build/TransitSessionCanBenchmark --if can0 --id 0x123 --samples 1000
```

说明：

- 示例默认使用 `can0`
- benchmark 依赖 SocketCAN 本地回环，程序内部会打开 `receive_own_messages`

## WebSocket

当前 WebSocket 支持 client 和 server，按一条 WebSocket message 触发一次读取回调。

客户端示例：

```bash
./build/TransitSessionWebSocket --host 127.0.0.1 --port 9002 --target / --message "hello"
```

服务端 echo 示例：

```bash
./build/TransitSessionWebSocketServer --bind 127.0.0.1 --port 9002
```

Benchmark：

```bash
./build/TransitSessionWebSocketBenchmark --payload 32 --samples 1000
./build/TransitSessionWebSocketBenchmark --host 192.168.1.10 --port 9002 --external-server
```

说明：

- 默认 benchmark 会在本机启动一个内置 echo server
- 加 `--external-server` 后只启动 client，连接已有 WebSocket echo 服务
