# Prerequisites — have these ready before starting

Split into what you need **on day one** and what can arrive **just in time**
for its phase. Don't buy everything upfront: phases 5–10 are reorderable, so
order hardware when the phase before it starts.

## 1. Must-have before Phase 00 (day one)

### Hardware
- [ ] **Raspberry Pi 5** — 8 GB recommended (the AI/camera phases appreciate
      it; 4 GB works everywhere else).
- [ ] **Official 27 W USB-C PD power supply** (or equivalent PD supply that
      negotiates 5 V/5 A — underpowered supplies cause USB current limiting
      and mystery instability; don't start your learning with that variable).
- [ ] **Active cooler** (official Pi 5 Active Cooler or a fan case) — the
      A76 cores throttle without it.
- [ ] **Storage**: 32 GB+ microSD, A2-class, from a reputable brand.
      (NVMe + M.2 HAT is a nice Phase 0 stretch, not a requirement.)
- [ ] **Network path**: Ethernet cable to your router *or* 2.4/5 GHz Wi-Fi
      you control. Access to the router's admin page (DHCP table) makes
      headless discovery painless.
- [ ] **A workstation** (Linux, macOS, or Windows-with-WSL2) with a microSD
      card reader. Linux or WSL2 makes the cross-compile phase smoothest.
- [ ] **micro-HDMI → HDMI cable** — strictly optional for the headless
      workflow, but worth having for the day SSH won't answer.

### Accounts & workstation software
- [ ] **GitHub account** with an SSH key set up (this curriculum lives in
      git; the Pi will need pull/push access in Phase 0).
- [ ] **Raspberry Pi Imager** installed on the workstation.
- [ ] **SSH client + terminal comfort** (any OS default is fine).
- [ ] Editor of choice; VS Code with Remote-SSH is the path of least
      resistance for Phase 2's remote workflow, but not mandatory.

### Knowledge assumed (you have this — listed for completeness)
- Solid **C** (pointers, structs, build/link model).
- Basic electronics: Ohm's law, LED + series resistor, pull-up/pull-down,
  reading a simple schematic and a datasheet.
- Command-line basics: `cd`/`ls`/editing a file over SSH.
- **Not** assumed: Linux internals, Python, kernel/device-tree knowledge,
  networking beyond "what an IP address is" — the curriculum teaches these.

## 2. Needed from Phase 03 (first hardware phase)
- [ ] **Breadboard + jumper wires** — crucially **female-to-male** jumpers
      (Pi header is male pins), plus male-male for the breadboard.
- [ ] **LEDs** (5+), **resistors** (330 Ω and 10 kΩ assortments),
      **push buttons** (2+). A "Pi/Arduino starter kit" covers all of this.
- [ ] **Multimeter** — any basic DMM; non-negotiable for debugging wiring.

## 3. Just-in-time hardware, by phase

| Phase | Buy before starting it |
|---|---|
| 04 — buses | BME280 (I2C), MPU6050 (I2C w/ INT pin), MCP3008 (SPI ADC), potentiometer; USB-serial (TTL 3.3 V!) adapter. Optional: SSD1306 OLED, RS-485 transceiver or USB-RS485 dongle, DS18B20, INMP441 I2S mic |
| 05 — timing | SG90-class hobby servo; DS18B20 if not bought already. Optional: Raspberry Pi Pico (I/O-coprocessor stretch) |
| 06 — MQTT | nothing new (your MQTT library + mosquitto) |
| 07 — wireless | Cellular: SIM7600/A7670-class HAT **or** USB LTE dongle + **data SIM** (prepaid IoT SIM is fine; confirm LTE bands for your country; avoid 2G-only modules). Optional: USB Wi-Fi adapter capable of monitor mode |
| 08 — CAN | MCP2518FD-based CAN HAT (preferred; MCP2515 acceptable) + a **second CAN node** (second Pi, USB-CAN adapter, or MCU board with CAN) + twisted pair + two 120 Ω terminators (HATs often have one on-board — check) |
| 09 — camera | **Camera Module 3** + **22-pin FPC cable for Pi 5** (the older 15-pin cable does not fit) |
| 10 — AI HAT | **AI HAT+ (Hailo-8, 26 TOPS)** preferred, or AI Kit (Hailo-8L, 13 TOPS); tall standoffs/GPIO extender if stacking with other HATs (you generally can't stack the AI HAT with the CAN HAT — plan to swap) |
| 11 — kernel | nothing new |
| 12 — capstone | RTC battery (nice), and whatever your chosen integration needs |

**Strongly recommended, any time after Phase 3:** a cheap **8-channel 24 MHz
logic analyzer** (~“Saleae-clone”, used with sigrok/PulseView). It turns
Phases 4, 5 and 8 from faith into observation. An oscilloscope is great but
optional.

## 4. Environment & safety
- A stable bench/desk where the breadboard can stay wired between sessions.
- **3.3 V discipline**: Pi GPIO is 3.3 V and *not* 5 V tolerant — check every
  module's logic level before wiring; keep 5 V away from GPIO pins.
- **Power down before rewiring** the header, and never hot-plug the camera
  FPC or any HAT.
- Basic ESD sense: touch ground before handling boards; keep boards in bags.
- A phone camera habit: photograph every working wiring setup before
  changing it (also feeds the capstone's documentation requirement).

## 5. Quick self-check before Phase 00
You're ready if you can: clone a repo over SSH from your workstation, explain
what a pull-up resistor does, and commit to ~3–5 hrs/week — phases average
1–3 weeks each at that pace (the capstone 2–4 weekends).
