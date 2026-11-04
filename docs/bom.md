# Bill of materials

Parts in use so far.

| Part | Qty | Notes | Reference |
|---|---|---|---|
| Raspberry Pi 5, 4GB | 1 | Main board, runs Raspberry Pi OS Lite (64-bit) | [Product brief (raspberrypi.com)](https://datasheets.raspberrypi.com/rpi5/raspberry-pi-5-product-brief.pdf) |
| Raspberry Pi 27W USB-C power supply | 1 | 5.1 V 5 A bench supply | [Product page (raspberrypi.com)](https://www.raspberrypi.com/products/27w-power-supply/) |
| Raspberry Pi Active Cooler | 1 | Fits under the CAN HAT | [Product page (raspberrypi.com)](https://www.raspberrypi.com/products/active-cooler/) |
| Raspberry Pi Debug Probe | 1 | Serial console on the Pi 5's 3-pin UART connector | [Documentation (raspberrypi.com)](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html) |
| Raspberry Pi RTC battery | 1 | ML2020 rechargeable cell for the Pi 5 RTC; charging enabled with `dtparam=rtc_bbat_vchg=3000000` | [Product page (raspberrypi.com)](https://www.raspberrypi.com/products/rtc-battery/) |
| SanDisk Extreme microSD, 64GB | 1 | OS card | - |
| Waveshare 2-CH CAN HAT+ | 1 | Two MCP2515 controllers on SPI1, isolated SN65HVD230 transceivers, 16 MHz crystals, jumper termination | [Wiki (waveshare.com)](https://www.waveshare.com/wiki/2-CH_CAN_HAT+), [Schematic (waveshare.com)](https://files.waveshare.com/wiki/2-CH-CAN-HAT%2B/2-CH_CAN_HAT%2B.pdf) |
| - MCP2515 CAN controller | 2 | On the HAT | [Product page (microchip.com)](https://www.microchip.com/en-us/product/mcp2515) |
| - SN65HVD230 CAN transceiver | 2 | On the HAT | [Datasheet (ti.com)](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf) |

Test equipment: Siglent SDS804X HD oscilloscope.
