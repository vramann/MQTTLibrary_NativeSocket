# Phase 03 — GPIO (the character-device way)

**Goal:** solid digital I/O on the Pi 5 in C using **libgpiod v2** — outputs,
inputs with debouncing, and event-driven edges — then the same in Python with
gpiozero, understanding what the convenience layer hides.

**Hardware:** breadboard, LEDs + resistors, push buttons.

**Pi 5 reality check:** header GPIOs live on the RP1 chip. Memory-mapped
tricks and legacy `RPi.GPIO` are dead ends here; the kernel's GPIO character
device (`/dev/gpiochipN`) is the right interface. Don't hard-code the chip
number — discover it (`gpioinfo`, or open by label `pinctrl-rp1`).

## Concepts
- gpiochips, lines, offsets vs "BCM numbering" vs physical pin numbers.
- Requesting lines: direction, bias (pull-up/down — set in software now, not
  config.txt), active-low, debounce.
- Edge events: blocking reads and poll/epoll integration — *no busy-waiting*.
- Electrical basics refresher: current limits per pin (RP1: keep to a few mA,
  3.3 V logic, **not 5 V tolerant**), when you need a transistor/driver.

## Tasks (C first — starter code in `code/`)
1. **Tooling warm-up** (shell): `gpiodetect`, `gpioinfo`, blink an LED purely
   with `gpioset`, read a button with `gpioget`, watch edges with `gpiomon`.
2. **Blink in C**: build `code/blink.c` (libgpiod v2 API) with the provided
   Makefile — natively on the Pi and cross-compiled from your host.
3. **Button → LED**: poll-free version using `gpiod_line_request_read_edge_event`
   with software debounce (`gpiod_line_settings_set_debounce_period_us`).
4. **Event multiplexing**: two buttons + epoll on the request fd, plus a
   timerfd in the same epoll loop (this pattern — fds + epoll — is the
   backbone of the capstone).
5. **Mini-project: reaction timer** — LED lights after random delay; measure
   time to button press with `clock_gettime(CLOCK_MONOTONIC)`; print stats.
6. **Now Python** (`sudo apt install python3-gpiozero`): redo tasks 3 and 5
   with gpiozero in ~15 lines. Skim gpiozero's source enough to see it using
   lgpio/the chardev underneath.

## Stretch
- Charlieplex 6 LEDs with 3 lines.
- Drive a relay or MOSFET for a >3.3 V load — justify the circuit.
- Measure gpioset-toggle vs C-loop toggle frequency with a scope or logic
  analyzer if you have one; explain why the Pi 5 (RP1 over PCIe) has higher
  per-toggle latency than a bare-metal MCU.

## Checkpoint
- Why can't userspace get a stable "GPIO register pointer" on the Pi 5?
- What does requesting a line actually do in the kernel, and why do requests
  block other users of the line?
- Where is the debounce implemented when you set a debounce period?
- When would you *not* use gpiozero?
