# Phase 05 — PWM, Timing & Real-Time

**Goal:** generate precise signals (PWM, servos), measure and reason about
latency on Linux, and know what PREEMPT_RT does and doesn't buy you —
the phase where MCU intuition meets Linux reality.

## Concepts
- Hardware PWM vs software PWM; why software PWM jitters on Linux.
- Pi 5 hardware PWM channels on GPIO12/13/18/19 (`dtoverlay=pwm-2chan`
  variants) exposed via `/sys/class/pwm/pwmchip*`.
- Clocks: `CLOCK_MONOTONIC`, `CLOCK_REALTIME`, timerfd, `clock_nanosleep`
  with `TIMER_ABSTIME` (drift-free periodic loops).
- Scheduling: `SCHED_FIFO`/`SCHED_RR`, priorities, `mlockall`, CPU isolation
  (`isolcpus`/`taskset`), and the PREEMPT_RT kernel (check `uname -v`).
- What Linux still can't give you: sub-µs deterministic I/O → when to add a
  microcontroller (Pico) as an I/O coprocessor.

## Tasks
1. **Sysfs PWM by hand** (shell): enable a PWM overlay, export a channel,
   set period/duty via `/sys/class/pwm`, dim an LED. Then wrap it in C
   (open/write on the sysfs files) as a small `pwm.c` helper.
2. **Servo control**: 50 Hz, 1–2 ms pulses on hardware PWM; sweep smoothly.
   Explain why you would *not* do this with gpioset toggling.
3. **Software-timed square wave in C**: `clock_nanosleep(TIMER_ABSTIME)` at
   1 kHz on a GPIO; capture with `gpiomon`/analyzer; histogram the jitter.
4. **Latency measurement**: run `cyclictest` (rt-tests package) idle vs under
   load (`stress-ng`); repeat with `SCHED_FIFO` + `mlockall` in your own
   1 kHz loop; record min/avg/max latency tables in your notes.
5. **GPIO-to-GPIO round trip**: output edge wired to input line; measure
   userspace event round-trip time distribution. Compare against your MCU
   expectations and explain every order of magnitude.
6. **1-Wire detour** (if you have a DS18B20): `dtoverlay=w1-gpio`, read via
   `/sys/bus/w1/` — a nice example of a kernel driver doing protocol work.

## Stretch
- Read the RP1 datasheet's PWM chapter; map sysfs channels to RP1 blocks.
- Isolate a core, pin your loop to it, re-run cyclictest — quantify the gain.
- Prototype the "Pico as I/O coprocessor" pattern: Pico generates exact
  waveforms, Pi commands it over USB-CDC/UART.

## Checkpoint
- Why is `clock_nanosleep(TIMER_ABSTIME)` better than `usleep` in a loop?
- What exactly does PREEMPT_RT change about the kernel, in one paragraph?
- Your 1 kHz loop shows 200 µs worst-case jitter: name three distinct causes
  and one mitigation for each.
- When is the correct engineering answer "add a microcontroller"?
