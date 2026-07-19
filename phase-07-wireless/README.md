# Phase 07 — Wireless Connectivity: Wi-Fi, Bluetooth, Cellular/GSM

**Goal:** master the Pi 5's wireless links as an engineer, not a settings-app
user: Wi-Fi (onboard, incl. access-point mode), Bluetooth Classic + BLE
(onboard, via BlueZ), and a cellular/GSM uplink (add-on HAT or USB modem) —
ending with a connectivity manager that fails over between links. This phase
feeds the capstone's uplink design.

**Hardware:** onboard Wi-Fi/BT is enough for parts A and B. Part C needs a
cellular modem: a SIM7600/A7670-series HAT or a USB LTE dongle (e.g.
Quectel EC25-based) plus a data SIM. "GSM" here means cellular data/SMS
generally — buy LTE (Cat-1/Cat-4) hardware, not 2G-only: 2G/3G networks are
shut down in many countries.

## Concepts
- The Linux wireless stack: firmware + driver → netlink (nl80211) →
  `wpa_supplicant` → **NetworkManager** as the orchestrator on Pi OS
  (`nmcli` is your CLI). rfkill for radio on/off.
- Wi-Fi station vs AP mode; what the onboard radio can/can't do concurrently.
- Bluetooth stack: controller (HCI) vs host (**BlueZ**); Classic (SPP/RFCOMM)
  vs **BLE** (GAP advertising, GATT services/characteristics); BlueZ's D-Bus
  API as the modern programming surface; `btmon` for on-air truth.
- Cellular: AT commands over a serial port (Phase 4 termios skills reused
  verbatim!) vs **ModemManager** (`mmcli`) doing it properly; QMI/MBIM vs PPP;
  SMS; why NAT'd carrier networks make *outbound* connections (MQTT!) the
  right architecture.
- Link health: RSSI/quality metrics, watchdogs, and route priorities
  (NetworkManager metrics) for failover.

## Part A — Wi-Fi
1. **nmcli fluency** (shell): list/scan (`nmcli dev wifi`), connect, inspect
   (`nmcli -f ALL dev wifi`), saved profiles, autoconnect priorities. Watch
   signal: `iw dev wlan0 link` and `/proc/net/wireless`.
2. **Headless provisioning story**: break your Wi-Fi config on purpose and
   recover via Ethernet/UART console (Phase 0 debug UART earns its keep).
   Document your recovery runbook.
3. **AP mode**: turn the Pi into a hotspot (`nmcli dev wifi hotspot ...`),
   connect your laptop, run your Phase 6 MQTT broker over it — a field-
   provisioning pattern (device boots as AP, you configure it, it joins your
   network).
4. **Programmatic view in C**: query link state the way tools do — read
   RSSI via `SIOCGIWSTATS`/nl80211 (use `libnl` or shell out and parse; try
   nl80211 via libnl for the real thing) and publish Wi-Fi health to MQTT
   with your library.

## Part B — Bluetooth (BlueZ)
1. **bluetoothctl fluency**: power, scan, pair, trust, connect; meanwhile run
   `btmon` in another tmux pane and watch the HCI traffic — always debug
   with btmon open.
2. **BLE scanning**: observe advertisements (phone, earbuds, any beacon);
   decode one advertisement payload by hand (flags, service UUIDs,
   manufacturer data) from the btmon hex.
3. **BLE central in Python** (`bleak`): connect to a peripheral (phone app
   like nRF Connect can simulate one) and read/subscribe to a GATT
   characteristic. If you have any BLE sensor (heart-rate band, Xiaomi
   thermometer), log its data.
4. **BLE peripheral on the Pi**: expose your Phase 4 sensor as a GATT service
   via BlueZ's D-Bus API (Python is the sane choice here; C via sd-bus is the
   stretch). Read it from your phone with nRF Connect.
5. **Classic SPP bridge**: RFCOMM serial link Pi ↔ phone/laptop; pipe your
   sensor stream over it; contrast pairing/throughput/power with BLE.
6. **Integration**: BLE sensor readings → MQTT bridge (bleak + your broker),
   i.e. the Pi as a BLE-to-IP gateway — a very common real product shape.

## Part C — Cellular / GSM
1. **Raw AT first** (so the abstraction isn't magic): find the modem's AT
   port (`mmcli -L`, `dmesg`), talk to it with your Phase 4 termios program
   or `minicom`: `AT`, `ATI`, `AT+CSQ` (signal), `AT+COPS?` (operator),
   send yourself an SMS with `AT+CMGF=1` + `AT+CMGS`.
2. **ModemManager properly**: `mmcli -m 0` status; create the data bearer via
   NetworkManager (`nmcli c add type gsm apn <apn>`); verify a second default
   route appears; `ping -I wwan0`.
3. **SMS programmatically**: send/receive via `mmcli --messaging`; wire an
   "SMS command" path (e.g. text `STATUS` → Pi replies with pi-health output).
4. **MQTT over cellular**: point your MQTT client at a public/cloud broker
   over the cellular link; tune keepalive/QoS for a metered, NAT'd, drop-prone
   link; measure data usage per hour of telemetry (this changes your payload
   design — quantify it).
5. **Failover capstone-prep**: configure NM route metrics so Ethernet > Wi-Fi
   > cellular; write a connectivity supervisor (C or shell+systemd timer)
   that detects real reachability (not just link-up), forces failover, logs
   transitions, and publishes current-uplink on MQTT. Pull each cable/radio
   and watch it ride through.

## Stretch
- Wi-Fi packet capture in monitor mode with an external USB adapter
  (onboard radio won't do monitor mode) — view 802.11 frames in Wireshark.
- BLE advertisement *broadcaster* on the Pi (beacon), plus power measurement.
- GPS: many cellular HATs include GNSS — enable it (`AT+CGPS` family or
  ModemManager location API), feed `gpsd`, timestamp your telemetry.
- Compare latency/jitter/throughput across all three uplinks (`ping`,
  `iperf3`) in a table.

## Checkpoint
- Draw the path of one byte from your MQTT client to the antenna for Wi-Fi
  vs cellular: which components differ, which are identical?
- BLE GATT: what are services, characteristics, and notifications, and how
  does your sensor map onto them?
- Why must the field device *initiate* connections on carrier networks, and
  what does that imply for firewalling and for your MQTT architecture?
- Your failover supervisor: why is "default route exists" not the same as
  "internet works", and what did you probe instead?
