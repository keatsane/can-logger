# Car run

The logger ran on a 2017 Audi A4 (B9) on 2026-11-04. The results are in `docs/drive/`:

- [report.md](drive/report.md) and `drive.png`: the summary and plots
- `can-20261104-193233.bin`: the raw log
- `first-request.log`: `candump` of the first request and what followed it

To decode the log and rebuild the summary and plots (the car info lines need the file that holds the VIN, which isn't in the repo):

```
mkdir -p logs
python3 tools/decode_log.py docs/drive/can-20261104-193233.bin > logs/drive.csv
python3 tools/drive_report.py logs/drive.csv logs/report
```

## Connection

- OBD-II pin 6 to CAN0 H, pin 14 to CAN0 L, pin 5 to CAN0 GND. Pin 16, unswitched battery power, stays unconnected and taped.
- The CAN0 termination jumper is off and the link wires to CAN1 are removed, since the car's bus is already terminated.
- The Pi runs from a USB-C car charger in the 12 V socket. It's reached over SSH on a phone hotspot, only while parked.
- Both CAN interfaces were down at boot, so nothing was on the bus before the listen-only capture. The logger only starts by hand.

## Order

Each step ran only after the one before it passed, all while parked:

1. Listen-only capture
2. One OBD-II request, sent by hand
3. `tools/car_info.py`
4. The logger for about 17 s
5. The logger for the drive

## Results (2026-11-04)

- Listen-only: 79.7 s of capture held 250,633 frames, about 3,100 per second, and all of them were the same frame: `1BFC8220#330000A202000000`. The interface reported 0 errors and 0 bus errors. It dropped 29 of the 256,560 frames it received. A controller in listen-only mode doesn't acknowledge frames, so the likely reason is that the sender kept retrying because nothing else on this part of the bus acknowledged it. What sends this frame is unknown.
- Normal mode: unlike the capture, the bus was nearly quiet. Over about 40 s `candump` showed the request, its reply and four more `1BFC8220` frames, each once. This fits the port being behind the car's gateway rather than directly on its internal buses, but the capture doesn't prove that.
- First request: `7E0#0201000000000000` got the reply `7E8#064100BE3EA81300` in 18.7 ms. The controller stayed error-active.
- Car info: the engine ECU listed 38 supported PIDs, including all nine the logger polls, with no stored or pending trouble codes. The multi-frame VIN came back, and flow control worked on a real ECU.
- Drive: 21.3 minutes wrote 5,633 records at 4.42 records/s, which decoded with 0 bad and 0 missing. There were 0 restarts and `vcgencmd get_throttled` read `0x0`. A status line late in the drive read 5,468 sent, 5,468 replies, 0 bad and 0 dropped.
- Timing: records were 190 to 600 ms apart. Measured from the log, the 1 s PIDs averaged 1.09 s (worst 1.81 s), the 2 s PIDs 2.08 s (worst 2.59 s), the 5 s PIDs 5.02 s (worst 5.43 s) and the 10 s PID 10.04 s (worst 10.23 s). The averages match the bench run. A record is stamped when its reply arrives, not when the request goes out, so the spacing between records moves with the reply time: 519 of the 5,632 gaps were shorter than the 200 ms between requests, the shortest 190.0 ms.
- Engine stops: engine speed read 0 three times at a standstill, for about 5, 5 and 9 s, and then the engine ran again. The Pi stayed up and the logger kept going through each one. The longest gap between records in the whole log, 0.60 s, was at the first stop.

## Limits of this run

- It was one car and one short drive, mostly in slow traffic: 6.4 km, 71 km/h at most, with a peak of 2,825 rpm and 135 kPa manifold pressure.
- The engine was already warm when logging started, with coolant at 102 C, so there's no warm-up curve.
- Only the OBD-II data was logged. The listen-only capture wasn't decoded beyond the frame above.
