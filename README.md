# can-logger

An embedded Linux data logger for a car's diagnostic CAN bus. A small Linux board plugs into the OBD-II port of a 2017 Audi A4, polls standard OBD-II data over CAN, and writes it to storage in a format built to survive the power being cut mid-write. After a drive, a host-side tool turns the logs into a report.

## The stack
- Linux: a minimal Buildroot image, device-tree bring-up of an SPI CAN controller, and the kernel's SocketCAN and ISO-TP support.
- C++: the logger itself - receive, poll scheduling, and storage on separate threads, with CRC-framed records and a hardware watchdog.
- C: STM32 firmware for a bench ECU that answers OBD-II requests, so the logger can be developed and tested without the car.
- Python: log decoding, power-loss test tooling, and drive reports.

## Safety
The logger only listens and sends standard read-only OBD-II requests. It never clears trouble codes, writes to a module, or opens a diagnostic session. Every new request type is tested against the bench ECU before it goes on the car.

## Status
Planned. Nothing is built yet.
