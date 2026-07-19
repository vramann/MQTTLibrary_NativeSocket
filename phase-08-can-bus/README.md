# Phase 08 — CAN Bus with SocketCAN

**Goal:** bring up your CAN HAT on the Pi 5, get fluent with SocketCAN in C
and the can-utils/python-can tooling, and climb the stack to ISO-TP — the
on-ramp to your UDS (ISO 14229) diagnostics interest.

**Hardware:** MCP2515-based HAT (classic CAN) or MCP2518FD HAT (CAN FD —
preferred if you're buying). Ideally two nodes: second Pi, USB-CAN adapter,
or an MCU board with CAN, so you have a real bus with 120 Ω termination at
both ends.

## Bring-up (device tree overlay — Phase 1 skills pay off)
- MCP2515: `dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25`
  (match oscillator/INT pin to *your* HAT's silkscreen/docs).
- MCP2518FD: `dtoverlay=mcp251xfd,spi0-0,interrupt=25`.
- Verify: `dmesg | grep -i can`, `ip link` shows `can0`.
- Up: `sudo ip link set can0 up type can bitrate 500000` (add
  `dbitrate ... fd on` for CAN FD). Loopback/listen-only modes exist for
  solo testing: `type can bitrate 500000 loopback on`.

## Concepts
- Why CAN-as-network-interface (SocketCAN) instead of a serial device: frames,
  filters, multiple applications sharing one bus.
- `struct can_frame` / `canfd_frame`, raw sockets (`PF_CAN`, `CAN_RAW`),
  kernel-side filtering (`CAN_RAW_FILTER`).
- Bus health: error frames, bus-off, `ip -details -statistics link show can0`.
- Layered protocols: ISO-TP (ISO 15765-2) segmentation via the kernel
  `can-isotp` module; where UDS sits on top.

## Tasks
1. **Tooling first** (`sudo apt install can-utils`): `cansend`, `candump`,
   `cangen` + `canbusload`; use `vcan` (virtual CAN) before touching hardware:
   `modprobe vcan && ip link add dev vcan0 type vcan && ip link set vcan0 up`.
2. **Raw socket C program**: open `CAN_RAW` socket, bind to `can0`, send a
   frame, receive with filters set so you only see IDs you care about.
   Run it against `candump` on vcan first, then real hardware.
3. **Two-node conversation**: periodic "sensor" frames from node A (reuse
   Phase 4 sensor data), consumer on node B; add a simple request/response
   ID pair. Integrate the CAN fd into your epoll loop (it's just an fd).
4. **Error handling**: enable error frames (`CAN_RAW_ERR_FILTER`), then
   deliberately break the bus (pull a wire, wrong bitrate) and observe/handle
   bus-off + restart (`ip link set can0 type can restart-ms 100`).
5. **ISO-TP**: `modprobe can-isotp`; use `isotpsend`/`isotprecv` to move a
   multi-frame payload; then a C program using `CAN_ISOTP` sockets. Send a
   UDS-shaped request (e.g. `0x22` ReadDataByIdentifier) and fake the
   response from the peer — groundwork for your UDS client project.
6. **Python pass**: `python-can` for a quick bus monitor/logger script
   (blf/asc logging); note when you'd reach for it vs C.

## Stretch
- Decode real traffic with a DBC file (`cantools` in Python); generate C
  encode/decode structs from the DBC (`cantools generate_c_source`).
- CAN FD throughput experiments (up to 64-byte payloads) if hardware allows.
- MQTT-CAN gateway preview: forward selected CAN IDs to MQTT with your
  library — a slice of the capstone.

## Checkpoint
- Why do interface state and bitrate live in `ip link` and not in your
  program?
- What does the kernel do for you between the MCP2518FD interrupt line and
  your `read()` returning a frame?
- When node A's requests get no response: walk your debugging order
  (termination? bitrate? ACK? filters?) — justify the sequence.
- Where does ISO-TP end and UDS begin?
