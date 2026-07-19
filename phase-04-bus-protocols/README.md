# Phase 04 — Bus Protocols: UART, I2C, SPI & Other Embedded Links

**Goal:** talk to real peripherals over the three classic serial buses, first
at the kernel-interface level in C (`termios`, `i2c-dev` ioctls, `spidev`
ioctls), then with Python (`pyserial`, `smbus2`, `spidev`) — then broaden to
the rest of the embedded-comms toolbox (RS-485/Modbus, 1-Wire, I2S, USB) and
build the judgment to pick the right link for a job.

**Hardware suggestions:** USB-serial adapter (UART), BME280 or any I2C sensor,
MPU6050 (I2C, interrupt-capable), MCP3008 ADC or any SPI device, optional
SSD1306/ILI9341 display. For the "other links" section: MAX485-style
RS-485 transceiver or a USB-RS485 dongle, DS18B20 (1-Wire), optional I2S
microphone (INMP441) or DAC.

## Enabling the buses (config.txt / raspi-config)
- I2C: `dtparam=i2c_arm=on` → `/dev/i2c-1` on GPIO2/3 (pins 3/5).
- SPI: `dtparam=spi=on` → `/dev/spidev0.0`, `/dev/spidev0.1`.
- UART on the header: `enable_uart=1` → use the `/dev/serial0` symlink
  (GPIO14/15). Note the Pi 5's 3-pin debug connector is a *separate* UART.
- Verify with `ls -l /dev/i2c* /dev/spidev* /dev/serial*` and `i2cdetect -y 1`
  (`sudo apt install i2c-tools`).

## Concepts
- UART: baud/framing, termios raw mode, VMIN/VTIME, line disciplines.
- I2C: addresses, register-read/write idiom, clock stretching, `I2C_SLAVE`
  ioctl vs `I2C_RDWR` combined transactions, SMBus subset.
- SPI: modes 0–3, chip selects, full-duplex transfers, `SPI_IOC_MESSAGE`.
- Datasheet reading: register maps, ID registers, calibration data.

## Tasks (C first)
1. **UART loopback**: jumper TX→RX on the header; write a C program using
   termios (raw mode) that sends and verifies a pattern. Then talk to your
   USB-serial adapter (`/dev/ttyUSB0`) between Pi and PC.
2. **I2C scan in C**: reimplement a minimal `i2cdetect` using `I2C_SLAVE` +
   probe reads. Compare against the real tool.
3. **Sensor driver in C**: read your I2C sensor's ID register, then implement
   a proper little driver module (`bme280.c`/`mpu6050.c` with init/read API):
   handle the register map and (for BME280) the calibration math. Log
   readings once per second.
4. **SPI in C**: talk to the MCP3008 (or your SPI device) with
   `SPI_IOC_MESSAGE` full-duplex transfers; read a potentiometer voltage.
5. **Combine + interrupt**: MPU6050 data-ready interrupt wired to a GPIO —
   reuse your Phase 3 epoll pattern: edge event → I2C read. This
   GPIO-IRQ + bus-read combo is *the* canonical embedded-Linux pattern.
6. **Python pass**: redo the sensor read with `smbus2` and the ADC with
   `spidev`; note how much boilerplate disappears and where the Python
   version would fall short (throughput, latency, combined transactions).

## Beyond the big three — other embedded comms

The Pi 5 speaks more than UART/I2C/SPI. Do at least the RS-485/Modbus and
1-Wire tasks; treat the rest as guided tours you can return to when a project
needs them.

7. **RS-485 / Modbus RTU** — the industrial workhorse: multi-drop,
   differential, hundreds of meters. Wire a MAX485-class transceiver to the
   UART (mind the driver-enable line) or just use a USB-RS485 dongle. First
   move raw bytes between two nodes; then speak **Modbus RTU**: implement a
   register read (function 0x03) with CRC16 in C over your termios port, and
   verify against `mbpoll` or a Python `pymodbus` peer. If you have any
   Modbus device (energy meter, VFD, sensor), read it for real.
8. **1-Wire**: DS18B20 temperature sensor via `dtoverlay=w1-gpio`; read it
   from `/sys/bus/w1/`. One wire, parasitic power, unique 64-bit IDs — note
   the kernel driver does all protocol timing (contrast with your termios
   work). (Phase 5 references this same setup.)
9. **USB as an embedded bus**: you've been using USB-serial adapters —
   now look under the hood: `lsusb -t`, `udevadm info`, watch enumeration in
   `dmesg`. Read a USB HID device (any gamepad/keyboard) from
   `/dev/input/event*` with a small C program using the `evdev` interface —
   input devices are the gentlest introduction to USB host-side work.
10. **I2S digital audio** (optional hardware): enable an I2S mic (INMP441)
    or DAC overlay, record/play with `arecord`/`aplay`, and understand why
    audio gets its own bus (continuous synchronous streaming vs
    transactional I2C/SPI).
11. **The selection matrix** (paper task, bring to a tutoring session):
    build a table — UART, I2C, SPI, RS-485, 1-Wire, I2S, USB, CAN (next
    phases), Ethernet — vs speed, distance, topology (point-to-point /
    multi-drop / star), wires, addressing, robustness, and a "use it when…"
    one-liner each. This table is the phase's real deliverable.

Related links you'll meet elsewhere: **CAN** gets all of Phase 8;
**PCIe** is how RP1, NVMe and the AI HAT attach (Phases 0 and 10);
**Ethernet/Wi-Fi/BT/cellular** are Phases 6–7.

## Stretch
- Bit-bang I2C on two GPIOs in C to *feel* the protocol; then never do it again.
- Drive an SSD1306 OLED (I2C) or ILI9341 LCD (SPI): framebuffer-style updates,
  measure achievable FPS vs SPI clock.
- Use a logic analyzer to capture a transaction and annotate the trace —
  I2C, SPI, and a Modbus RTU exchange if you did task 7.
- Modbus TCP variant of task 7 over Ethernet; compare framing with RTU.

## Checkpoint
- What does the `I2C_RDWR` ioctl let you do that plain read/write can't, and
  why does it matter for register reads?
- Why does SPI always shift data in both directions?
- In termios, what do VMIN/VTIME control, and what goes wrong if you leave
  the port in canonical mode?
- From the BME280 datasheet: why can't you use the raw ADC values directly?
- Why does RS-485 reach hundreds of meters when I2C struggles past a meter?
- A design needs 12 sensors, 20 m apart, in an electrically noisy cabinet:
  which bus, and what would change your answer?
