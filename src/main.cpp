#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Usb.h>
#include <usbh_midi.h>
#include "zoom_ms.h"
#include "debug.h"

#define MIDI_RX_PIN             7          // SoftwareSerial RX pin for MIDI input
#define MIDI_TX_PIN             5          // SoftwareSerial TX pin for MIDI thru

#define MIDI_BAUD_RATE          31250   // Standard MIDI baud rate
#define MIDI_STATUS_MASK        0xF0
#define MIDI_CHANNEL_MASK       0x0F
#define MIDI_PROGRAM_CHANGE     0xC0
#define MIDI_IS_STATUS_BYTE(c)  (c & 0x80)

// debug / CC commands
const char COMMAND_TOGGLE_BYPASS =  'B';
const char COMMAND_GET_DATA =       'D';
const char COMMAND_TOGGLE_FULL =    'F';
const char COMMAND_GET_ID =         'I';
const char COMMAND_NEXT_PATCH =     'N';
const char COMMAND_PREV_PATCH =     'P';
const char COMMAND_TOGGLE_TUNER =   'T';
const char COMMAND_GET_INDEX =      'X';
#define LOWERCASE(c)                ((c) + 32)
#define IS_COMMAND(x, c)            ((x == c) || (x == LOWERCASE(c)))

// USB and MIDI objects
SoftwareSerial  _midi_serial(MIDI_RX_PIN, MIDI_TX_PIN);
ZoomMSDevice *  _zoom = nullptr;

void handle_special_pc(uint8_t pc) {
        if (IS_COMMAND(pc, COMMAND_PREV_PATCH)) {
            _zoom->incPatch(-1, true);
        }
        else if (IS_COMMAND(pc, COMMAND_NEXT_PATCH)) {
            _zoom->incPatch(1, true);
        }
        else if (IS_COMMAND(pc, COMMAND_TOGGLE_TUNER)) {
            _zoom->toggleTuner();
        }
        else if (IS_COMMAND(pc, COMMAND_TOGGLE_BYPASS)) {
            _zoom->toggleBypass();
        }
        else if (IS_COMMAND(pc, COMMAND_TOGGLE_FULL)) {
            _zoom->toggleFullBypass();
        }
        else if (IS_COMMAND(pc, COMMAND_GET_DATA)) {
            _zoom->requestPatchData();
        }
        else if (IS_COMMAND(pc, COMMAND_GET_INDEX)) {
            int8_t pi = _zoom->requestPatchIndex();
            dprint(F("Current patch index: "));
            dprintln(pi);
        }
        else if (IS_COMMAND(pc, COMMAND_GET_ID)) {
            _zoom->requestDeviceID();
            dprint(F("Device name: "));
            dprintln(_zoom->device_name);
            dprint(F("Firmware version: "));
            dprintln(_zoom->fw_version);
        }
}

void handle_serial_cli() {
#ifdef SERIAL_DEBUG
    if (Serial.available()) {
        handle_special_pc(Serial.read());
    }
#endif
}

void handle_midi_input() {
    while (_midi_serial.available()) {
        byte b = _midi_serial.read();
        if (MIDI_IS_STATUS_BYTE(b)) {
            // status byte, check...
            if ((b & MIDI_STATUS_MASK) == MIDI_PROGRAM_CHANGE) {
                dprint(F("MIDI PC: "));
                // next byte is PC number
                b = _midi_serial.read();
                dprintln(b);
                if (b < ZOOM_MS_MAX_PATCHES) {
                    // patch number
                    _zoom->sendPatch(--b);
                }
                else {
                    // special command
                    handle_special_pc(b);
                }
            }
        } 
    }
}

void setup() {
    dprintinit(9600);
    _midi_serial.begin(MIDI_BAUD_RATE);
    _zoom = new ZoomMSDevice();
}

void loop() {
    handle_midi_input();
    handle_serial_cli();
}


// void sendMidiThru(const MidiMessage& msg) {
//     // Send complete message to MIDI thru port
//     _midi_serial.write(msg.status);
//     if (msg.length > 1) {
//         _midi_serial.write(msg.data1);
//     }
//     if (msg.length > 2) {
//         _midi_serial.write(msg.data2);
//     }
// }
