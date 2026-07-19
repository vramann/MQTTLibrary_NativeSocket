# Curated Resources

Prefer official docs; the Pi 5 changed enough that older third-party tutorials
are often wrong (see the gotchas list in pi5-architecture-notes.md).

## Official / primary
- Raspberry Pi documentation: https://www.raspberrypi.com/documentation/
  (especially "Raspberry Pi hardware", "config.txt", "Camera", "AI Kit / AI HAT+")
- RP1 peripherals datasheet (the south bridge): search "RP1 peripherals PDF" on
  raspberrypi.com — worth skimming once in Phase 0.
- Raspberry Pi OS / firmware GitHub: https://github.com/raspberrypi
  (`linux`, `firmware`, `rpicam-apps`, `picamera2`, `utils`)

## Per-topic
- **libgpiod** (v2 API + tools): https://libgpiod.readthedocs.io/ and
  `man gpioset gpioget gpiomon gpioinfo`
- **gpiozero**: https://gpiozero.readthedocs.io/
- **I2C/SPI kernel interfaces**: kernel docs `Documentation/i2c/dev-interface`
  and `Documentation/spi/spidev` (https://docs.kernel.org/)
- **SocketCAN**: https://docs.kernel.org/networking/can.html ;
  can-utils: https://github.com/linux-can/can-utils ;
  python-can: https://python-can.readthedocs.io/
- **ISO-TP / UDS**: kernel CAN_ISOTP docs; ISO 14229 overview articles
  (ties into your UDS client project)
- **Wi-Fi / NetworkManager**: `man nmcli` and
  https://networkmanager.dev/docs/ ; Pi networking docs on raspberrypi.com
- **Bluetooth / BlueZ**: https://www.bluez.org/ (D-Bus API docs live in the
  bluez source tree under `doc/`); `bleak` (Python BLE):
  https://bleak.readthedocs.io/ ; debug with `btmon`
- **Cellular / ModemManager**: https://modemmanager.org/ (`mmcli` docs);
  your modem vendor's AT command manual (SIMCom/Quectel PDFs) is the ground
  truth for Part C of Phase 7
- **libcamera / rpicam-apps**: https://www.raspberrypi.com/documentation/computers/camera_software.html
- **picamera2 manual**: https://datasheets.raspberrypi.com/camera/picamera2-manual.pdf
- **Hailo on Pi 5**: https://github.com/hailo-ai/hailo-rpi5-examples and the
  Raspberry Pi "AI Kit / AI HAT+" docs pages
- **Device tree**: https://www.raspberrypi.com/documentation/computers/configuration.html#device-trees-overlays-and-parameters
- **Kernel modules on Pi**: Raspberry Pi "Kernel" documentation section
  (building, headers, cross-compiling)

## Books (optional, solid)
- *The Linux Programming Interface* — Kerrisk (the C/syscall bible; use as reference)
- *Exploring Raspberry Pi* — Molloy (older Pis, but the Linux-interfacing
  chapters age well — cross-check pin/chip specifics against Pi 5 docs)
- *Linux Device Drivers* (LDD3, free online) + modern supplements for Phase 11
