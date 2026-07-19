# Phase 06 — Networking & MQTT

**Goal:** Linux socket programming on the Pi and a working MQTT setup — and,
since you've already built an MQTT library over native sockets
(`MQTTLibrary_NativeSocket`), the centerpiece here is **porting and hardening
your own library on the Pi 5** rather than learning MQTT from scratch.

## Concepts
- Sockets refresher in the Pi context: TCP client/server, `epoll` (you've been
  building toward this since Phase 3 — GPIO fds, timerfds and sockets all
  multiplex in one loop).
- mosquitto as a local broker; QoS levels, retained messages, LWT in practice.
- Service-ifying network daemons with systemd (Phase 1 skills), including
  socket-activation as a stretch.
- Basic TLS with mosquitto (self-signed CA) — optional but realistic.

## Tasks
1. **Environment**: `sudo apt install mosquitto mosquitto-clients`; verify
   pub/sub locally with `mosquitto_pub`/`mosquitto_sub`; open the firewall
   angle: who can reach your broker on the LAN?
2. **Port your MQTT library**: build `MQTTLibrary_NativeSocket` on the Pi
   (native) and via your Phase 2 cross-toolchain. Fix any portability issues
   (endianness assumptions, `-Wall -Wextra` on aarch64, CMake toolchain).
3. **Sensor → MQTT bridge in C**: combine Phase 4's sensor driver with your
   library: publish BME280/MPU6050 readings as JSON to `pi5/sensors/...`
   once per second; subscribe to `pi5/led` to control a GPIO LED. One epoll
   loop, no threads unless you can justify them.
4. **Make it a service**: systemd unit with `Restart=on-failure`; watch it
   survive broker restarts (test your library's reconnect logic — add it if
   missing; this is a genuinely valuable upgrade to your library).
5. **Interoperability check**: run the same flows against your broker with
   `mosquitto_sub` watching — confirm your library's packets behave (correct
   topics, QoS handshakes) — and sniff a session with
   `tcpdump -i lo port 1883` + Wireshark's MQTT dissector.
6. **Python contrast**: same bridge in ~30 lines with `paho-mqtt`; reflect on
   where your C library wins (footprint, control, no GIL) and what features
   paho has that yours lacks — feed that into your library's roadmap.

## Stretch
- TLS: give mosquitto a self-signed CA + server cert; add TLS to your
  client via OpenSSL, or document why you'd wrap with stunnel instead.
- MQTT 5 features tour (properties, shared subscriptions) using mosquitto.
- Bench: messages/sec and latency of your library vs paho on the Pi.

## Checkpoint
- Exactly what happens on the wire for a QoS 1 publish? (You should be able
  to point at it in your tcpdump capture.)
- Why is a single epoll loop often better than thread-per-connection here?
- What does LWT solve for the sensor-bridge use case?
- What did porting to aarch64 flush out of your library, if anything?
