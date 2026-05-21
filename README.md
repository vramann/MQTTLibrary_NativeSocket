# MQTT Native Socket Library

A production-grade MQTT 3.1.1 C++ library built on **native POSIX TCP sockets** and **OpenSSL** — no external MQTT library dependency.

## Why native sockets?

| | [MQTTLibrary_Paho](../mqtt-library) | This project |
|---|---|---|
| Transport | Eclipse Paho C/C++ | POSIX TCP + OpenSSL |
| External MQTT dep | `paho.mqtt.c` + `paho.mqtt.cpp` | None |
| TLS | Paho SSL options | Direct `SSL_CTX` / `SSL_connect` |
| Protocol framing | Paho | Hand-rolled MQTT 3.1.1 codec |
| Binary size | Larger (Paho static libs) | Smaller |
| Portability | Linux / macOS | Any POSIX system |

Both projects expose **identical public APIs** (`IPublisher`, `ISubscriber`, `Message`, `ConnectionConfig`, etc.), so they are drop-in replaceable.

## Features

- **MQTT 3.1.1** packet codec — CONNECT, CONNACK, PUBLISH, PUBACK, SUBSCRIBE, SUBACK, UNSUBSCRIBE, UNSUBACK, PINGREQ/RESP, DISCONNECT
- **QoS 0 and 1** (QoS 2 can be added on top of the packet layer)
- **TLS 1.2/1.3** via OpenSSL — CA verification, mutual TLS, hostname verification
- **Automatic reconnect** with exponential back-off and jitter
- **Keepalive** — PINGREQ sent at `keepalive/2` intervals; socket recv timeout keeps the loop responsive
- **MQTT wildcard subscriptions** — `+` and `#` topic filter matching
- **Async publish** with optional `PublishAckCallback`
- **Structured logging** via spdlog
- **Metrics** — atomic counters for published, received, dropped, reconnect, latency EMA
- **YAML / JSON config** via yaml-cpp / nlohmann-json (FetchContent, zero install required)
- **Unit tests** (GTest) covering the full packet codec
- **Docker** dev environment with Mosquitto broker

## Directory layout

```
mqtt-native-socket/
├── include/mqtt/
│   ├── core/            Message, Error, ConnectionConfig, ConnectionState
│   ├── transport/       TlsConfig, SocketTransport (POSIX + OpenSSL)
│   ├── protocol/        MqttPacket — MQTT 3.1.1 encoder / decoder
│   ├── logging/         Logger (spdlog wrapper)
│   ├── monitoring/      Metrics
│   ├── config/          ConfigLoader (YAML + JSON)
│   ├── publisher/       IPublisher, MqttPublisher
│   └── subscriber/      ISubscriber, MqttSubscriber
├── src/                 Implementation
├── tests/unit/          GTest unit tests
├── examples/            single_publisher, single_subscriber
├── docker/              Dockerfile + Mosquitto docker-compose
├── configs/             default.yaml
└── scripts/             build.sh, install_deps.sh
```

## Quick start

```bash
# 1. Install system deps (cmake, git, libssl-dev)
./scripts/install_deps.sh

# 2. Build (all C++ deps fetched automatically)
./scripts/build.sh          # debug build + tests

# 3. Run with a local broker
docker-compose -f docker/docker-compose.yml up -d mosquitto
./build/debug/examples/single_publisher configs/default.yaml
./build/debug/examples/single_subscriber configs/default.yaml
```

## Minimal usage

```cpp
#include "mqtt/mqtt.hpp"

mqtt::ConnectionConfig cfg;
cfg.brokerUrls = {"tcp://localhost:1883"};
cfg.clientId   = "my-device";

mqtt::MqttPublisher pub(cfg);
pub.connect();
pub.publish(mqtt::Message::fromString("vehicle/speed", "120", 1));
pub.disconnect();
```

## Build options

| Option | Default | Description |
|--------|---------|-------------|
| `MQTT_BUILD_TESTS` | `ON` | Build GTest unit tests |
| `MQTT_BUILD_EXAMPLES` | `ON` | Build example binaries |
| `MQTT_ENABLE_ASAN` | `OFF` | AddressSanitizer |
| `MQTT_ENABLE_TSAN` | `OFF` | ThreadSanitizer |
| `MQTT_ENABLE_UBSAN` | `OFF` | UndefinedBehaviorSanitizer |

```bash
cmake --preset asan && cmake --build build/asan
```

## Dependencies

| Library | How obtained | Purpose |
|---------|-------------|---------|
| OpenSSL | System package | TLS transport |
| spdlog v1.14.1 | FetchContent | Logging |
| nlohmann/json v3.11.3 | FetchContent | JSON config |
| yaml-cpp 0.8.0 | FetchContent | YAML config |
| GoogleTest v1.14.0 | FetchContent (tests only) | Unit testing |

## Implementation notes

### `SocketTransport`
- Non-blocking `connect()` with `poll()` timeout
- `SO_RCVTIMEO = 500 ms` on the socket so the receive loop can check a stop flag without blocking indefinitely
- OpenSSL non-blocking handshake with `poll()` between `SSL_ERROR_WANT_READ/WRITE` retries

### `MqttPacket`
- Stateless encoder functions return `std::vector<uint8_t>`
- `readPacket()` reads fixed header + variable-length remaining-length + body from the transport
- Separate `parse*()` functions decode CONNACK, PUBLISH, SUBACK payloads

### Publisher receive loop
- Dedicated `std::jthread` reads incoming packets
- PUBACK resolves a `std::promise<ErrorCode>` keyed by packet ID — synchronous `publish()` waits on the corresponding `std::future`
- PINGRESP is logged and discarded

### Subscriber receive loop
- Incoming PUBLISH is dispatched to the matching topic callback (wildcard-aware)
- QoS 1: PUBACK sent immediately before callback to avoid redelivery on slow handlers
- SUBACK / UNSUBACK resolve pending promises so `subscribe()` / `unsubscribe()` can block-wait with a timeout
