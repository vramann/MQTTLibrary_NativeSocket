# Raspberry Pi 5 — Learning Curriculum

A phased, hands-on curriculum for mastering the Raspberry Pi 5: from Linux and
C/C++ fundamentals through GPIO, bus protocols, CAN, camera, and the AI HAT,
ending in a capstone that integrates everything.

**Learner profile:** experienced embedded C developer, new to the Pi 5 platform.
**Language order:** C/C++ first, then Python and shell where they earn their place.

## Hardware covered

| Item | Used from phase |
|---|---|
| Raspberry Pi 5 (4/8 GB), 27 W USB-C PSU, SD card / NVMe | Phase 0 |
| Basic GPIO kit (LEDs, buttons, breadboard, resistors) | Phase 3 |
| I2C/SPI sensors (e.g. BME280, MPU6050, MCP3008, small display) | Phase 4–5 |
| Cellular/GSM modem (SIM7600/A7670 HAT or USB LTE dongle) + data SIM | Phase 7 (Part C) |
| CAN HAT (MCP2515 or MCP2518FD based) | Phase 8 |
| Camera Module 3 | Phase 9 |
| AI HAT / AI Kit (Hailo-8 / Hailo-8L) | Phase 10 |

Wi-Fi and Bluetooth/BLE (Phase 7 Parts A and B) use the Pi 5's onboard radio —
no extra hardware needed.

## Phase map

| Phase | Topic | Outcome |
|---|---|---|
| [00](phase-00-setup/) | Board bring-up & setup | Headless Pi 5 you can SSH into; understand what makes the Pi 5 different (BCM2712 + RP1) |
| [01](phase-01-linux-fundamentals/) | Linux for embedded work | Fluent with systemd, /sys, /proc, udev, device tree, permissions |
| [02](phase-02-build-toolchain/) | C/C++ toolchain on & for the Pi | Native + cross-compile workflow with CMake, gdb remote debugging |
| [03](phase-03-gpio/) | GPIO | Digital I/O in C (libgpiod v2), edge events, then gpiozero in Python |
| [04](phase-04-bus-protocols/) | UART, I2C, SPI | Talk to real sensors from C (ioctl-level) and Python |
| [05](phase-05-advanced-io-timing/) | PWM, timing, real-time | Hardware PWM, servos, ADC, latency measurement, PREEMPT_RT basics |
| [06](phase-06-networking-mqtt/) | Networking & MQTT | Sockets in C, mosquitto, port/reuse of your own MQTT library on the Pi |
| [07](phase-07-wireless/) | Wireless: Wi-Fi, BT/BLE, cellular | nmcli/AP mode, BlueZ + GATT, ModemManager/AT, uplink failover supervisor |
| [08](phase-08-can-bus/) | CAN bus | SocketCAN in C, can-utils, python-can, ISO-TP and a taste of UDS |
| [09](phase-09-camera/) | Camera | rpicam-apps, libcamera C++, picamera2, OpenCV capture pipeline |
| [10](phase-10-ai-hat/) | AI HAT (Hailo) | On-device inference: detection/pose pipelines, custom model flow |
| [11](phase-11-kernel-devicetree/) | Kernel & device tree | Write a device tree overlay and a small kernel module |
| [12](phase-12-capstone/) | Capstone | CAN + camera + AI + wireless uplink + MQTT "vehicle gateway" project |

Dependency-wise: 0 → 1 → 2 → 3 → 4 are sequential. After 4, phases 5–10 can be
reordered to match hardware availability (Phase 7's cellular part can wait for
the modem; its Wi-Fi/BT parts need no extra hardware). Phase 11 is
optional-but-recommended before the capstone. Track your progress in [PROGRESS.md](PROGRESS.md).

## How to use this repo as a course

1. Work through one phase at a time; each phase README has **Concepts → Tasks →
   Checkpoint → References**.
2. Commit your solution code into the phase's `code/` directory (phase 03 ships
   a starter example showing the expected layout: C source + Makefile).
3. Don't skip checkpoints — they are the questions a tutor would ask before
   letting you move on. Bring answers/doubts back to your tutoring sessions.
4. Python and shell tasks intentionally come *after* the C tasks in each phase:
   first understand the kernel interface, then appreciate the convenience layer.

## Background docs

- [docs/pi5-architecture-notes.md](docs/pi5-architecture-notes.md) — what's new
  in the Pi 5 (BCM2712, the RP1 south bridge, and the gotchas that break Pi 4
  habits and older tutorials).
- [docs/resources.md](docs/resources.md) — curated references per topic.
