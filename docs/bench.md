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

The bitrate doesn't survive a reboot, so `config/systemd/can-up@.service` sets it at boot, once per interface. It waits for the renamed interface to exist before running.

```
just pi-install                                      # copies the .link and service files
sudo systemctl enable can-up@can_obd can-up@can_ecu  # on the Pi, then reboot
```

This brings the interfaces up in normal mode, where the controller acknowledges frames and sends error flags even if no program is running. For a listen-only capture, disable it first and bring the interface up by hand:

```
sudo systemctl disable --now can-up@can_obd
sudo ip link set can_obd up type can bitrate 500000 listen-only on
candump -L can_obd > capture.log
```

Listen-only stays set across `down` and `up` until it's turned off, so the service always passes `listen-only off`. To go back to normal mode:

```
sudo ip link set can_obd down
sudo systemctl enable --now can-up@can_obd
```

On the bench, a frame sent from `can_ecu` showed up repeatedly on `can_obd` in listen-only mode, because nothing acknowledged it and `can_ecu` went error-passive. A frame sent from `can_obd` never reached `can_ecu`.

## Fake ECU

`tools/fake_ecu.py` answers OBD-II requests sent to 0x7DF or 0x7E0 on 0x7E8, in padded 8-byte frames:

- service 0x01: the logger's PIDs (listed below) with values from a simulated drive, plus the supported-PID ranges 0x00 and 0x20
- services 0x03 and 0x07: three stored trouble codes, which need more than one frame, and no pending ones
- service 0x09: a made-up VIN as a multi-frame reply, sent after the requester's flow control frame

It flags any request outside the read-only OBD-II services, and any frame that isn't 8 bytes, since a real ECU may ignore one.

```
python3 tools/fake_ecu.py            # on can_ecu
cansend can_obd 7DF#02010C0000000000 # rpm request
```

## Car info

`tools/car_info.py` asks the engine ECU once for its supported PIDs, VIN, and stored and pending trouble codes. It refuses to send anything outside the read-only OBD-II services, and waits 200 ms between requests. It shares the logger's IDs, so the logger has to be stopped first.

```
python3 tools/car_info.py | tee ~/logs/car-info.txt
```

## Logger

`src/logger.cpp` runs three threads on an ISO-TP socket, physical addressing 0x7E0 to 0x7E8:

- the poller sends requests from `src/scheduler.cpp`, never closer together than 200 ms (table below)
- the receiver decodes each reply, stamps it with the wall-clock time and hands it over through a bounded queue, which drops and counts if it ever fills
- the writer appends each reading to a new file per run, `can-YYYYMMDD-HHMMSS.bin` (UTC), with an `fsync` once a second, and prints a status line

| PID | Value | Unit | Polled every |
|---|---|---|---|
| 0x04 | Engine load | % | 1 s |
| 0x05 | Coolant temperature | C | 5 s |
| 0x06 | Short-term fuel trim | % | 5 s |
| 0x07 | Long-term fuel trim | % | 10 s |
| 0x0B | Manifold pressure | kPa | 2 s |
| 0x0C | Engine speed | rpm | 1 s |
| 0x0D | Vehicle speed | km/h | 1 s |
| 0x0F | Intake air temperature | C | 5 s |
| 0x11 | Throttle position | % | 2 s |

The record format is documented at the top of `src/record.hpp`: 28 bytes, little-endian, with a sequence number and a CRC-32.

`tools/decode_log.py` checks a log and writes CSV. Problems and a summary go to stderr: bad CRCs, gaps in the sequence numbers, and a record cut off at the end of the file. After damage it finds the next record by searching for the `CL` marker.

```
just pull-logs
python3 tools/decode_log.py logs/can-20261103-203836.bin > logs/run.csv
```

`tools/drive_report.py` turns that CSV into `drive.png` and `report.md`: duration, distance, coolant warm-up, the longest gap between records, and min, mean and max for each PID. Given the car info file, it also adds the supported PIDs and trouble codes. It never copies the VIN. It needs matplotlib, so it runs on the PC.

```
python3 tools/drive_report.py logs/run.csv logs/report logs/car-info.txt
```

## Running as a service

`config/systemd/can-logger.service` runs the logger from `~/logs`. It has no `[Install]` section, so it only starts by hand:

```
sudo systemctl start can-logger
journalctl -u can-logger -f
sudo systemctl stop can-logger
```

- SIGINT and SIGTERM stop it cleanly: the threads finish, queued readings are written and the file is synced. That exits 0, so systemd leaves it stopped.
- If a thread hits an error, the logger stops and exits 1, and systemd restarts it after 5 s.
- `WatchdogSec=10`: once a second the writer sends systemd `WATCHDOG=1`, but only if the poller and receiver have both gone around their loops since the last time. If that stops for 10 s, systemd kills and restarts the logger.
- systemd also feeds the Pi's hardware watchdog, which is on by default here (60 s timeout).
- The ACT LED blinks while records are being written, as a check with no laptop. The service takes the LED over from the SD card activity trigger at start and gives it back at stop. A missing LED never stops the logging.

## Measurements (2026-11-01)

- Bit time: 2.0 us, matching 500 kbit/s. Measured on a Siglent SDS804X HD with CH1 on CAN-H, CH2 on CAN-L and the math trace CH1 - CH2, using time cursors across a single bit. Both channels share the overlay's oscillator setting, so they would agree with each other even if it were wrong; the scope checks it independently.
- Bit timing chosen by the kernel (`ip -details link show`): 125 ns time quantum (16 MHz crystal / 2), 16 quanta per bit, sample point 87.5%.
- Scope CAN decode at 500 kbit/s read the request as ID 0x7DF, length 8.
- Terminal mapping: with the link wires removed, a frame sent on `can_obd` gets no acknowledgment and is retransmitted continuously; the retransmissions appear on the CAN0 terminal and not on CAN1.

## Logger measurements (2026-11-03)

All against the fake ECU.

- Poll rate, nine PIDs: a 16.0-minute run under systemd wrote 4,238 records at 4.41 records/s, decoded with 0 bad and 0 missing, and the fake ECU answered 4,238 requests and flagged none. There were 0 restarts and `vcgencmd get_throttled` read `0x0`. Records were 201 to 593 ms apart. Measured from the log, the 1 s PIDs averaged 1.09 s (worst 1.81 s), the 2 s PIDs 2.08 s (worst 2.59 s), the 5 s PIDs 5.02 s and the 10 s PID 10.04 s. They run slightly long because each period restarts from when the request was actually sent.
- Car info: `tools/car_info.py` against the fake ECU read back all ten supported PIDs across two ranges, the multi-frame VIN and three stored trouble codes, with the kernel's ISO-TP socket handling flow control.
- One-hour run, with the earlier table of coolant, RPM and speed: 61.6 minutes under systemd wrote 15,742 records at 4.26 records/s, and the log decoded with 0 bad and 0 missing. The fake ECU answered 15,742 requests and flagged none. There were 0 restarts, `vcgencmd get_throttled` read `0x0` at the end, and the logger used 2.6 s of CPU. Time between records stayed between 200.8 and 301.9 ms, so the 200 ms minimum spacing held. The log came to 441 KB.
- Record integrity: a 36-record log stopped with `pkill -TERM` decoded with 0 bad and 0 missing. Decoder behavior on damaged files is covered by `tests/decode_log_test.py`.
- Interface failure: `sudo ip link set can_obd down` stopped the logger with `Network is down` and exit code 1. systemd retried every 5 s, and the logger recovered on its own once the interface came back up.
- Silent bus: with the fake ECU stopped for about 15 s and then about 20 s, `sent` kept rising, `replies` froze, and the watchdog did not fire. Replies resumed when it restarted. The other channel was still up and acknowledging frames.
- Hang: `pkill -STOP logger` froze the whole process, and systemd reported `Watchdog timeout` 10 s after the last status line.
- One stuck thread: with a temporary long sleep in the poller, `sent` froze while the writer kept printing status lines. The watchdog fired 11 s after the start and systemd restarted the logger.
- Nothing acknowledging: with `can_ecu` down, the logger stopped within about 2 s with `Communication error on send` and exit code 1. `can_obd` went to error-passive.
- Bus errors: with `can_ecu` at 250 kbit/s against `can_obd` at 500 kbit/s, the logger stopped the same way and `can_obd` went bus-off. `restart-ms` is 0, so it stays off the bus until someone restarts the interface.
- Disk full: on a 16 KB tmpfs (the smallest possible with this kernel's 16 KB pages), the logger stopped with `writer: write: Input/output error` and exit code 1. The file decoded to 585 good records, 0 bad, 0 missing, and a partial record at byte 16380.
- Hardware watchdog: with `kernel.panic` at 0, a kernel crash through `/proc/sysrq-trigger` rebooted the Pi on its own, and both CAN interfaces came back up.
