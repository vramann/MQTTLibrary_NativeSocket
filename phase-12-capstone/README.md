# Phase 12 — Capstone: Vehicle Gateway

**Goal:** one system that exercises every phase — a "vehicle gateway" that
bridges CAN telemetry, camera + AI perception, and MQTT reporting, built
with your own MQTT library at the center. Scope it to 2–4 weekends.

## The system

```
 [CAN bus / vcan replay] ──► SocketCAN ─┐
                                        ├─► gateway daemon (C, epoll) ──► MQTT (your library) ──► broker ──► dashboard
 [Camera] ─► picamera2/Hailo detections ┘                         │
                                                                  └─► ring-buffer logger (disk)
```

- **CAN side**: real sensors on your HAT bus, or replay a recorded/candump
  log onto `vcan0` — decode selected IDs (DBC via generated C code from
  Phase 8 stretch) into signals.
- **Perception side**: Hailo detection events (person/vehicle) from the
  Phase 10 pipeline arriving as JSON over a local Unix socket.
- **Gateway daemon (the core deliverable, in C)**: single epoll loop owning
  the CAN socket, the Unix socket from the perception process, a timerfd for
  periodic stats, and your MQTT connection. Publishes decoded signals,
  detection events, and health (`pi-health` from Phase 1, now in-process).
  systemd service, `Restart=on-failure`, structured logging to journald.
- **Diagnostics twist (optional, ties to your UDS project)**: expose a UDS
  server-ish endpoint on ISO-TP — respond to ReadDataByIdentifier with live
  gateway stats — or plug in your actual UDS client work against an ECU sim.
- **Uplink (Phase 7 skills)**: the gateway must not care which link carries
  MQTT — run your Phase 7 connectivity supervisor underneath (Ethernet/Wi-Fi
  preferred, cellular failover) and publish uplink state as part of health.
  Optional: SMS "STATUS" command path as an out-of-band diagnostic.
- **Dashboard**: anything subscribing to the broker — Grafana + a bridge,
  Node-RED, or a simple web page; keep this part cheap.

## Milestones
1. Architecture note (1 page): processes, IPC choices, message schemas,
   failure behaviors. Get it reviewed (tutor session) before coding.
2. CAN ingest + decode → MQTT, replayable from a log (testable without the car/bench).
3. Perception events integrated end-to-end (camera → Hailo → Unix socket → MQTT).
4. Robustness pass: broker down, bus-off, camera unplugged, process crash —
   the gateway must degrade and recover; write chaos tests that prove it.
5. Ship: README with wiring photos, systemd units, install script, and a
   demo video/gif. Tag `v1.0`.

## Definition of done
- Survives `stress-ng` + broker restart + CAN error storm without dying.
- Cold boot to fully-reporting in under 30 s with no manual steps.
- You can explain every layer from photon/CAN-frame to MQTT payload.

## After the capstone
- Port the gateway daemon's ideas back into your libraries (reconnect logic,
  epoll patterns, logging discipline).
- Candidate next arcs: Zephyr on a Pico coprocessor, Yocto/Buildroot custom
  image for the gateway, or deepening the UDS client into a real tool.
