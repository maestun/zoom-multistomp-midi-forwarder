#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Usb.h>
#include <usbh_midi.h>
#include "zoom_ms.h"
#include "debug.h"
#include "version.h"

#define MIDI_RX_PIN             7          // SoftwareSerial RX pin for MIDI input
#define MIDI_TX_PIN             5          // SoftwareSerial TX pin for MIDI thru ??

#define MIDI_BAUD_RATE          31250   // Standard MIDI baud rate
#define MIDI_STATUS_MASK        0xF0
#define MIDI_CHANNEL_MASK       0x0F
#define MIDI_PROGRAM_CHANGE     0xC0
#define MIDI_IS_STATUS_BYTE(c)  (c & 0x80)
#define MIDI_TIMEOUT_MS         20

// debug / CC commands
enum eSpecialPC {

    TUNER_TOGGLE = 's',
    TUNER_ON = 'T',
    TUNER_OFF = 't',

    GET_DATA = 'd',
    GET_ID = 'i',
    GET_INDEX = 'x',
    GET_VERSION = 'v',

    NEXT_PATCH = 'n',
    PREV_PATCH = 'p',
};

SoftwareSerial  _midi_serial(MIDI_RX_PIN, MIDI_TX_PIN);
ZoomMSDevice *  _zoom = nullptr;

void handle_special_pc(eSpecialPC pc) {
    if (pc == PREV_PATCH) {
        _zoom->incPatch(-1, true);
    }
    else if (pc == NEXT_PATCH) {
        _zoom->incPatch(1, true);
    }
    else if (pc == TUNER_TOGGLE) {
        _zoom->toggleTuner();
    }
    else if (pc == TUNER_ON) {
        _zoom->enableTuner(true);
    }
    else if (pc == TUNER_OFF) {
        _zoom->enableTuner(false);
    }
    else if (pc == GET_DATA) {
        _zoom->requestPatchData();
    }
    else if (pc == GET_INDEX) {
        dprint(F("Current patch index: "));
        int8_t pi = _zoom->requestPatchIndex();
        dprintln(pi);
    }
    else if (pc == GET_ID) {
        _zoom->requestDeviceID();
        dprint(F("Device name: "));
        dprintln(_zoom->device_name);
        dprint(F("Firmware version: "));
        dprintln(_zoom->fw_version);
    }
    else if (pc == GET_VERSION) {
        dprint(F("Program version: "));
        dprint(GIT_TAG);
        dprint(F(" - "));
        dprint(GIT_HASH);
        dprint(F(" - "));
        dprintln(GIT_BRANCH);
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
            if ((b & MIDI_STATUS_MASK) == MIDI_PROGRAM_CHANGE) {
                dprint(F("MIDI PC: "));
                // wait for data byte with timeout (don't block forever)
                unsigned long timeout = millis() + MIDI_TIMEOUT_MS;
                while (!_midi_serial.available() && millis() < timeout) {
                    _zoom->tick();  // keep USB alive while waiting
                }
                if (_midi_serial.available()) {
                    b = _midi_serial.read();
                    dprintln(b);
                    if (b <= ZOOM_MS_MAX_PATCHES) {
                        // patch number
                        _zoom->sendPatch(--b);
                    }
                    else {
                        // special command
                        handle_special_pc((eSpecialPC)b);
                    }
                } else {
                    dprintln(F("MIDI timeout - no data byte"));
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
    _zoom->tick();
    handle_midi_input();
    handle_serial_cli();
}
