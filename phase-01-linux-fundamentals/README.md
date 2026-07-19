# Phase 01 — Linux Fundamentals for Embedded Work

**Goal:** fluency with the Linux mechanisms every later phase relies on:
processes, permissions, systemd, /sys and /proc, udev, and the device tree
*as a user* (writing overlays comes in Phase 10).

## Concepts
- Everything is a file: `/dev`, `/sys`, `/proc` and how kernel drivers expose
  hardware through them.
- systemd: units, `systemctl`, `journalctl`, writing a service that starts your
  program at boot.
- Users/groups/permissions for hardware: why `gpio`, `i2c`, `spi`, `dialout`,
  `video` groups exist; udev rules as the mechanism behind them.
- Device tree from the consumer side: `/proc/device-tree`, `dtoverlay` /
  `dtparam` in config.txt, runtime `dtoverlay` command.

## Tasks
1. **/sys safari** (shell): find and cat interesting nodes —
   `/sys/class/thermal/thermal_zone0/temp`, `/sys/class/leds/` (blink the
   onboard ACT LED by writing to `brightness`/`trigger`),
   `/sys/firmware/devicetree/base/model`.
2. **Process & journal basics**: `ps`, `top`/`htop`, `journalctl -b`,
   `dmesg -w` while plugging in a USB device.
3. **Write a systemd service**: a 10-line shell script that logs CPU temp to
   the journal every 30 s; write a `.service` (+`.timer` variant) unit for it,
   enable it, verify across a reboot.
4. **udev**: run `udevadm monitor` while inserting USB; write one udev rule
   that creates a stable symlink for a USB-serial adapter (preview of Phase 4).
5. **Permissions**: check which groups your user is in (`groups`); confirm what
   `/dev/gpiochip*` and `/dev/i2c-*` permissions look like and which group
   grants access without sudo.
6. **Device tree reading**: list `/boot/firmware/overlays/`, read the README
   there; use `dtoverlay -h <some-overlay>` to inspect parameters.
7. **Shell scripting drill**: write a `pi-health.sh` that prints temp, throttle
   state (`vcgencmd get_throttled` decoded), memory, disk, uptime — you'll
   reuse it all course long.

## Stretch
- Compare `strace cat /sys/class/thermal/thermal_zone0/temp` output with your
  expectation — watch the syscalls.
- Explore cgroups: put a CPU-hog process in a slice limited to 50 % of a core.

## Checkpoint
- How does a value physically measured by hardware end up as text in a
  `/sys` file when you `cat` it?
- What's the difference between enabling a service and starting it?
- Which mechanism decides the owner/group of `/dev/i2c-1`?
- What does `dtoverlay=xyz` in config.txt actually change at boot?
