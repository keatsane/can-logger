# can-logger

An embedded Linux data logger for a car's diagnostic CAN bus. A small Linux board plugs into the OBD-II port of a 2017 Audi A4, polls standard OBD-II data over CAN, and writes it to storage as CRC-framed records. After a drive, a host-side tool turns the logs into a report.

## The stack
- Linux: a Raspberry Pi 5 with a dual-channel SPI CAN HAT, a device-tree overlay for the HAT, and the kernel's SocketCAN and ISO-TP support.
- C++: the logger itself - receive, poll scheduling, and storage on separate threads, with CRC-framed records and a hardware watchdog.
- Python: a fake ECU on the second CAN channel so the logger can be developed and tested without the car, plus log decoding and drive reports.

## Safety
The logger only listens and sends standard read-only OBD-II requests. It never clears trouble codes, writes to a module, or opens a diagnostic session. Every new request type is tested against the bench ECU before it goes on the car.

## Status
In progress. Both CAN channels run on the bench using the overlay in `overlay/can-hat.dts`, with a fake ECU answering OBD-II requests; see [docs/bench.md](docs/bench.md) for the setup and measurements and [docs/bom.md](docs/bom.md) for parts. The logger is not written yet.
