# Phase 04 — Bus Protocols: UART, I2C, SPI

**Goal:** talk to real peripherals over the three classic serial buses, first
at the kernel-interface level in C (`termios`, `i2c-dev` ioctls, `spidev`
ioctls), then with Python (`pyserial`, `smbus2`, `spidev`).

**Hardware suggestions:** USB-serial adapter (UART), BME280 or any I2C sensor,
MPU6050 (I2C, interrupt-capable), MCP3008 ADC or any SPI device, optional
SSD1306/ILI9341 display.

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

## Stretch
- Bit-bang I2C on two GPIOs in C to *feel* the protocol; then never do it again.
- Drive an SSD1306 OLED (I2C) or ILI9341 LCD (SPI): framebuffer-style updates,
  measure achievable FPS vs SPI clock.
- Use a logic analyzer to capture a transaction and annotate the trace.

## Checkpoint
- What does the `I2C_RDWR` ioctl let you do that plain read/write can't, and
  why does it matter for register reads?
- Why does SPI always shift data in both directions?
- In termios, what do VMIN/VTIME control, and what goes wrong if you leave
  the port in canonical mode?
- From the BME280 datasheet: why can't you use the raw ADC values directly?
