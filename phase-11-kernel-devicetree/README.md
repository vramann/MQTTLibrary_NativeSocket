# Phase 11 — Kernel Modules & Device Tree Overlays

**Goal:** cross the userspace/kernel boundary you've been leaning on all
course: write a device tree overlay from scratch and a small kernel module,
so drivers stop being magic. Optional-but-recommended before the capstone.

## Concepts
- Device tree source (.dts) → compiled overlay (.dtbo) → merged at boot by
  firmware; fragments, targets, labels, `compatible` strings, how a driver
  binds to a node.
- Kernel module lifecycle: init/exit, `printk`/`dev_info`, module parameters,
  building out-of-tree against headers (`linux-headers-rpi-2712` /
  `raspberrypi-kernel-headers` package on Pi OS).
- Char device basics: `file_operations`, misc devices, copy_to/from_user.
- Kernel GPIO/IRQ consumer APIs (`gpiod_get`, `request_irq`) — same concepts
  as userspace libgpiod, one floor down.
- Judgment: when a userspace daemon is the right answer anyway (usually!).

## Tasks
1. **Read before writing**: decompile an existing overlay
   (`dtc -I dtb -O dts /boot/firmware/overlays/mcp2515-can0.dtbo`) — you used
   this exact overlay in Phase 8; now read what it actually declares
   (SPI node, oscillator clock, interrupt GPIO, `compatible = "microchip,mcp2515"`).
2. **Write an overlay**: create a `.dts` that declares a device on a GPIO —
   e.g. a `gpio-leds` node for your breadboard LED (heartbeat trigger!) or a
   `gpio-keys` node for your button (it becomes a real input device —
   verify with `evtest`). Compile with `dtc`, install to
   `/boot/firmware/overlays/`, load with `dtoverlay=` and at runtime with
   `sudo dtoverlay <name>`.
3. **hello.ko**: minimal module, `insmod`/`rmmod`, watch `dmesg`; add a module
   parameter and a `/proc` or sysfs read hook.
4. **Char driver**: a misc device `/dev/mydev` implementing read/write over an
   internal buffer; test with `cat`/`echo` and a small C test program.
5. **GPIO IRQ in-kernel**: module that requests your button GPIO + IRQ, counts
   presses, exposes the count via sysfs. Compare the in-kernel debounce/IRQ
   view with your Phase 3 userspace version.
6. **Bind it together**: make your char driver instantiate from *your own
   device tree overlay node* (a `compatible` string you invent + platform
   driver match) — the full driver-model round trip.

## Stretch
- Cross-compile the module from your workstation against the Pi kernel
  headers/source (the real-world workflow for kernel iteration).
- `ftrace`/`trace-cmd` a path from your `read()` syscall into your driver.
- Read the RP1 driver source in the Pi kernel tree for one peripheral you've
  used (PWM or SPI) — map it to behavior you observed in earlier phases.

## Checkpoint
- How does the kernel decide which driver binds to your overlay's node?
- Why must data cross via `copy_to_user` instead of a plain memcpy?
- What context does your IRQ handler run in, and what must it never do?
- For a new custom sensor product: overlay + kernel driver, or userspace
  daemon on i2c-dev? Argue both sides, pick one.
