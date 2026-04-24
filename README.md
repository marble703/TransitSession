# TransitSession

`TransitSession` 是一个基于 C++20 和 Boost.Asio 的异步通信库，设计目标是分离**异步调度**和**协议读写**，当前提供两类传输能力：

- **serial**：面向串口的字节流会话
- **can**：面向 Linux SocketCAN 的 CAN 帧会话


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
- `example/`
  - 示例程序

## 构建

工程使用 CMake 构建。

```bash
cmake -S . -B build
cmake --build build -j
```