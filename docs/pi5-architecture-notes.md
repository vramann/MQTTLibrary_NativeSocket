# Raspberry Pi 5 — Architecture Notes (read before Phase 0)

Coming from microcontrollers or from earlier Pis, these are the facts that
matter most on the Pi 5.

## The big picture: BCM2712 + RP1

The Pi 5 is a **two-chip design**:

- **BCM2712** — the main SoC: 4× Arm Cortex-A76 @ 2.4 GHz, VideoCore VII GPU,
  memory controller (LPDDR4X), HDMI, and a PCIe root complex.
- **RP1** — a Raspberry-Pi-designed **I/O controller ("south bridge")**
  connected to BCM2712 over a PCIe 2.0 x4 link. Nearly all low-speed I/O lives
  here: the 40-pin GPIO header, UART, SPI, I2C, PWM, USB 3.0, Gigabit Ethernet
  MAC, and the two MIPI camera/display transceivers.

Consequence: **anything that talked directly to Broadcom GPIO registers on
Pi 4 and earlier is broken on Pi 5.** The header pins are no longer memory-
mapped Broadcom GPIO — they're behind RP1 across PCIe. This kills old
tutorials and libraries that poke `/dev/mem` (classic `RPi.GPIO`, `wiringPi`,
bare register access). The portable, correct interfaces — character-device
GPIO (`/dev/gpiochipN` via libgpiod), `spidev`, `i2c-dev`, sysfs PWM — all
work fine, and they're what this curriculum teaches.

## Other Pi 5 deltas worth knowing

- **PCIe exposed**: a single-lane external PCIe FPC connector (Gen 2 by
  default; Gen 3 can be enabled in config) — used by NVMe HATs and the AI
  HAT/Kit (Hailo accelerator is a PCIe device).
- **Two 4-lane MIPI connectors**, each usable for camera *or* display. They
  use the newer, smaller 22-pin FPC — Camera Module cables from older Pis need
  the 22-pin cable.
- **Dedicated 3-pin debug UART connector** (great for early-boot debugging;
  works with the Raspberry Pi Debug Probe).
- **Real-time clock** with a battery connector; **power button**; no 3.5 mm
  audio jack; fan connector (active cooling is genuinely recommended —
  the A76s throttle without it).
- **Power**: official 27 W USB-C PD supply recommended; with a weaker supply
  the firmware limits downstream USB current.
- Boot files live in `/boot/firmware/` (`config.txt`, `cmdline.txt`,
  overlays) on current Raspberry Pi OS — not `/boot` as in older docs.

## Gotchas checklist (things that break Pi 4 habits & old blog posts)

1. `RPi.GPIO` (legacy) doesn't work → use **libgpiod** (C), **lgpio**, or
   **gpiozero** (Python; its default backend on Pi 5 is lgpio). `rpi-lgpio`
   exists as a drop-in shim if you must run old `RPi.GPIO` scripts.
2. **gpiochip numbering changed across kernel versions** (header GPIOs
   appeared as `gpiochip4` on early Pi 5 kernels, later renumbered to
   `gpiochip0`). Never hard-code the chip number — discover it with
   `gpioinfo` / `gpiodetect` or open by label (`pinctrl-rp1`).
3. `raspi-gpio` is replaced by the **`pinctrl`** tool on Pi 5.
4. Old `/boot/config.txt` paths, `wiringPi` pin numbers, and `/dev/mem`
   register maps in tutorials → distrust anything pre-2024 until verified.
5. Recent Raspberry Pi OS kernels (6.12+) ship **PREEMPT_RT** — relevant in
   Phase 5. Check with `uname -a` / `uname -v` (look for `PREEMPT_RT`).

## Mental model to carry forward

On a microcontroller you own the hardware; on the Pi you **ask the kernel**
for hardware. Every peripheral in this course follows the same pattern:

1. A **device tree** node/overlay tells the kernel the hardware exists
   (Phase 10 demystifies this).
2. A **kernel driver** binds to it and exposes a **/dev node or sysfs
   interface** (`/dev/gpiochip0`, `/dev/i2c-1`, `/dev/spidev0.0`,
   `/dev/ttyAMA*`, `can0`, `/dev/video*`).
3. **Your C code** talks to that interface with `open`/`read`/`write`/`ioctl`.
4. **Python libraries** wrap step 3 for convenience.

Once you see the pattern twice, every new peripheral is "which /dev node,
which ioctl set?"
