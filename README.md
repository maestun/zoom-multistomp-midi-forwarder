
# Zoom MultiStomp MIDI forwader

## Features

Enables sending standard MIDI `Program Change` messages thru a standard MIDI DIN-5 socket, to control a Zoom MultiStomp effect pedal thru USB.

## Bill of Materials

- [Arduino Pro Mini (3.3v version)](img/APMv33.jpg)
- [USB Host Shield v2.0](img/UHSv2.jpg)
- [FDTI serial programmer (configured as 3.3v) for code upload](img/FSA.jpg)
- Resistors: 10kΩ, 100kΩ
- Diode: 1n4148 or 1n914
- Optocoupler: 4N28
- 5-pin DIN female socket

## Wiring

```
MIDI IN Socket (5-pin DIN):
Pin 4: Current Source (+5V from MIDI sender)
Pin 5: Current Return (MIDI data return)

Connections:

MIDI Pin 4 ──[220Ω]── 4N28 Pin 1 (LED Anode)
MIDI Pin 5 ────────── 4N28 Pin 2 (LED Cathode)
                      4N28 Pin 3 N/C

                      4N28 Pin 4 (Emitter)  ──────────- Arduino GND
                      4N28 Pin 5 (Collector) ──[10kΩ]── Arduino VCC (3.3V)
                      4N28 Pin 5 (Collector) ────────── Arduino MIDI_RX_PIN
                      4N28 Pin 6 (Base) ───────[100kΩ]─ Arduino GND
```

## Supported MIDI PC messages

- PC 0 to 49: will set the Zoom pedal to this patch number
- Special PC values: see `eSpecialPC` enum in [main.cpp](src/main.cpp)
