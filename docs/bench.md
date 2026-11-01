# Bench setup

The bench stands in for the car: one CAN channel runs the logger side, the other runs a fake engine ECU, and the two are wired together into a short terminated bus.

## Wiring

- The HAT's two channels are linked with three wires: CAN0 H to CAN1 H, CAN0 L to CAN1 L, CAN0 GND to CAN1 GND.
- Both 120 ohm termination jumpers are on (each channel is one end of the bus). In the car the CAN0 jumper comes off, since the car's bus is already terminated.
- The logic-level jumper is set to 3.3 V.
- The HAT's CAN side is isolated from the Pi, so CAN GND only connects to other CAN GNDs, never to a Pi ground pin.

## Device tree overlay

`overlay/can-hat.dts` describes the HAT to the kernel:

- two 16 MHz fixed clocks, one per MCP2515 crystal
- pin setup on the RP1 GPIO controller: chip selects on GPIO 17 and 16 as outputs, interrupts on GPIO 22 and 13 as inputs
- SPI1 enabled with two GPIO chip selects (GPIO 17 = CS 0, GPIO 16 = CS 1) on the RP1 SPI1 data pins (GPIO 19-21)
- an MCP2515 on each chip select: 10 MHz SPI clock, level-low interrupt

Build and install it on the Pi:

```
dtc -@ -I dts -O dtb -o can-hat.dtbo overlay/can-hat.dts
sudo cp can-hat.dtbo /boot/firmware/overlays/
```

Then add `dtoverlay=can-hat` to `/boot/firmware/config.txt` and reboot. It replaces the vendor's suggested lines (`dtparam=spi=on`, `dtoverlay=spi1-3cs` and two `dtoverlay=mcp2515,...` lines); SPI0 isn't used, and only the two chip selects the HAT is wired to are configured.

## Interface names

The kernel names CAN interfaces `canN` in the order the chips probe, which came up reversed from the board labels. Two udev `.link` files in `config/systemd/` name them by SPI chip select instead:

| Interface | Board terminal | Chip select | Interrupt | Use |
|---|---|---|---|---|
| `can_obd` | CAN0 | GPIO 17 (CS 0) | GPIO 22 | Logger side; goes to the car |
| `can_ecu` | CAN1 | GPIO 16 (CS 1) | GPIO 13 | Bench only; runs the fake ECU |

Install with `sudo cp config/systemd/*.link /etc/systemd/network/` and reboot. Bring both up at 500 kbit/s:

```
sudo ip link set can_obd up type can bitrate 500000
sudo ip link set can_ecu up type can bitrate 500000
```

## Fake ECU

`tools/fake_ecu.py` answers OBD-II service 0x01 requests (sent to 0x7DF or 0x7E0) on 0x7E8, as padded single frames. It supports PID 0x00 (supported PIDs), 0x05 (coolant), 0x0C (RPM) and 0x0D (speed), with values from a simulated drive, and flags any request outside the read-only OBD-II services.

```
python3 tools/fake_ecu.py            # on can_ecu
cansend can_obd 7DF#02010C0000000000 # rpm request
```

## Measurements (2026-11-01)

- **Bit time: 2.0 us**, matching 500 kbit/s. Measured on a Siglent SDS804X HD with CH1 on CAN-H, CH2 on CAN-L and the math trace CH1 - CH2, using time cursors across a single bit. Both channels share the overlay's oscillator setting, so they would agree with each other even if it were wrong; the scope checks it independently.
- **Bit timing chosen by the kernel** (`ip -details link show`): 125 ns time quantum (16 MHz crystal / 2), 16 quanta per bit, sample point 87.5%.
- **Scope CAN decode** at 500 kbit/s read the request as ID 0x7DF, length 8.
- **Terminal mapping:** with the link wires removed, a frame sent on `can_obd` gets no acknowledgment and is retransmitted continuously; the retransmissions appear on the CAN0 terminal and not on CAN1.
