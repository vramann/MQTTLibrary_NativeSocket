# Phase 00 — Board Bring-up & Setup

**Goal:** a headless Pi 5 you can SSH into from your workstation, with a clear
mental model of the board (BCM2712 + RP1) and the boot process.

**Read first:** [docs/pi5-architecture-notes.md](../docs/pi5-architecture-notes.md)

## Concepts
- Pi 5 board tour: power (27 W PD), fan, RTC battery, PCIe FPC, dual MIPI,
  debug UART connector, 40-pin header.
- Boot flow: on-chip ROM → EEPROM bootloader (SPI flash, updatable) →
  firmware reads `/boot/firmware/config.txt` → kernel + device tree → systemd.
- Raspberry Pi OS variants (Lite vs Desktop, 64-bit); why Lite 64-bit is the
  right choice for this course.

## Tasks
1. **Flash** Raspberry Pi OS Lite (64-bit) with Raspberry Pi Imager. In the
   Imager's customization: set hostname, enable SSH (key-based), configure
   Wi-Fi or plan for Ethernet, set username.
2. **First boot headless**: find the Pi on your network (`ping <hostname>.local`,
   or your router's DHCP table), SSH in.
3. **Update everything**: `sudo apt update && sudo apt full-upgrade`, then
   `sudo rpi-eeprom-update` to check bootloader firmware.
4. **Explore the board from software** — record the answers in PROGRESS.md:
   - `cat /proc/cpuinfo`, `cat /proc/device-tree/model`
   - `lspci` — find RP1 and note it's a PCIe device
   - `pinctrl` — dump the header pin states
   - `vcgencmd measure_temp`, `vcgencmd pmic_read_adc` (power rails)
5. **Learn `config.txt`**: read `/boot/firmware/config.txt`, skim the official
   config.txt documentation. Understand `dtparam`, `dtoverlay` — you'll use
   them constantly from Phase 4 onward.
6. **Set up the debug UART** (optional but very embedded-developer-worthy):
   connect a USB-serial adapter or Debug Probe to the 3-pin UART connector and
   watch the boot log.
7. **Shell/tmux/git comfort**: install `git`, `tmux`, your editor; create an SSH
   key on the Pi and add it to GitHub so the Pi can pull/push this repo.

## Stretch
- Boot from NVMe via a PCIe HAT; enable PCIe Gen 3 (`dtparam=pciex1_gen=3`)
  and benchmark with `hdparm`/`fio`.
- Try `rpi-imager`'s net-install / recovery path so you know it exists.

## Checkpoint — can you answer these?
- What does RP1 do, and why does it appear in `lspci`?
- Where does `config.txt` live and who reads it (kernel or firmware)?
- What's the difference between the EEPROM bootloader and the kernel?
- Why is active cooling recommended on Pi 5?
