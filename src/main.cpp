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
enum eSpecialPC {
    // BYPASS_TOGGLE = 'a',
    // BYPASS_ON = 'B',
    // BYPASS_OFF = 'b',

    // FULL_BYPASS_TOGGLE = 'e',
    // FULL_BYPASS_ON = 'F',
    // FULL_BYPASS_OFF = 'f',

    TUNER_TOGGLE = 's',
    TUNER_ON = 'T',
    TUNER_OFF = 't',

    GET_DATA = 'd',
    GET_ID = 'i',
    GET_INDEX = 'x',
    NEXT_PATCH = 'n',
    PREV_PATCH = 'p',
};

SoftwareSerial  _midi_serial(MIDI_RX_PIN, MIDI_TX_PIN);
ZoomMSDevice *  _zoom = nullptr;

void handle_special_pc(eSpecialPC pc) {
    if ((pc == PREV_PATCH)) {
        _zoom->incPatch(-1, true);
    }
    else if ((pc == NEXT_PATCH)) {
        _zoom->incPatch(1, true);
    }
    else if ((pc == TUNER_TOGGLE)) {
        _zoom->toggleTuner();
    }
    else if ((pc == TUNER_ON)) {
        _zoom->enableTuner(true);
    }
    else if ((pc == TUNER_OFF)) {
        _zoom->enableTuner(false);
    }
    // else if ((pc == BYPASS_TOGGLE)) {
    //     _zoom->toggleBypass();
    // }
    // else if ((pc == BYPASS_ON)) {
    //     _zoom->enableBypass(true);
    // }
    // else if ((pc == BYPASS_OFF)) {
    //     _zoom->enableBypass(false);
    // }
    // else if ((pc == FULL_BYPASS_TOGGLE)) {
    //     _zoom->toggleFullBypass();
    // }
    // else if ((pc == FULL_BYPASS_ON)) {
    //     _zoom->enableFullBypass(true);
    // }
    // else if ((pc == FULL_BYPASS_OFF)) {
    //     _zoom->enableFullBypass(false);
    // }
    else if ((pc == GET_DATA)) {
        _zoom->requestPatchData();
    }
    else if ((pc == GET_INDEX)) {
        int8_t pi = _zoom->requestPatchIndex();
        dprint(F("Current patch index: "));
        dprintln(pi);
    }
    else if ((pc == GET_ID)) {
        _zoom->requestDeviceID();
        dprint(F("Device name: "));
        dprintln(_zoom->device_name);
        dprint(F("Firmware version: "));
        dprintln(_zoom->fw_version);
    }
    else {
        dprint(F("Unknown PC: "));
        dprintln(pc);
    }
}

void handle_serial_cli() {
#ifdef SERIAL_DEBUG
    if (Serial.available()) {
        handle_special_pc(static_cast<eSpecialPC>(Serial.read()));
    }
#endif
}

void handle_midi_input() {
    while (_midi_serial.available()) {
        byte b = _midi_serial.read();
        byte channel = (b & MIDI_CHANNEL_MASK) + 1;
        if (MIDI_IS_STATUS_BYTE(b) && 
            (channel == TARGET_MIDI_CHANNEL)) {
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
                    handle_special_pc((eSpecialPC)b);
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
