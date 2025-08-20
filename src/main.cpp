#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Usb.h>
#include <usbh_midi.h>
#include "zoom_ms.h"
#include "debug.h"

// Configuration
#define MIDI_BAUD_RATE 31250   // Standard MIDI baud rate

// Pin definitions
#define MIDI_RX_PIN 7          // SoftwareSerial RX pin for MIDI input
#define MIDI_TX_PIN 5          // SoftwareSerial TX pin for MIDI thru

// MIDI message types

// 1 data byte expected
#define PROGRAM_CHANGE      0xC0

// 2 data bytes expected
#define CONTROL_CHANGE      0xB0
#define CHANNEL_PRESSURE    0xD0

#define MIDI_STATUS_MASK    0xF0
#define MIDI_CHANNEL_MASK   0x0F

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

// Test variables
bool testMode = false;
unsigned long lastTestMessage = 0;
byte testProgramNumber = 0;

// MIDI message buffer
struct MidiMessage {
    byte status;
    byte data1;
    byte data2;
    byte length;
};

// Function prototypes
void sendProgramChange(uint8_t pc);
void handle_midi_input();
void handle_serial_cli();
void processMidiMessage(const MidiMessage& msg);
void sendMidiThru(const MidiMessage& msg);
void sendUsbMidi(byte channel, byte program);
// void printMidiMessage(const MidiMessage& msg);
void sendRawMidiBytes(byte* bytes, int length);

void setup() {
    dprintinit(9600);
    _midi_serial.begin(MIDI_BAUD_RATE);
    _zoom = new ZoomMSDevice();
}

void loop() {
    handle_midi_input();
    handle_serial_cli();
}

void handle_serial_cli() {
    #ifdef SERIAL_DEBUG
    if (Serial.available()) {
        char command = Serial.read();
        if (IS_COMMAND(command, COMMAND_PREV_PATCH)) {
            _zoom->incPatch(-1, true);
        }
        else if (IS_COMMAND(command, COMMAND_NEXT_PATCH)) {
            _zoom->incPatch(1, true);
        }
        else if (IS_COMMAND(command, COMMAND_TOGGLE_TUNER)) {
            _zoom->toggleTuner();
        }
        else if (IS_COMMAND(command, COMMAND_TOGGLE_BYPASS)) {
            _zoom->toggleBypass();
        }
        else if (IS_COMMAND(command, COMMAND_TOGGLE_FULL)) {
            _zoom->toggleFullBypass();
        }
        else if (IS_COMMAND(command, COMMAND_GET_DATA)) {
            _zoom->requestPatchData();
        }
        else if (IS_COMMAND(command, COMMAND_GET_INDEX)) {
            int8_t pi = _zoom->requestPatchIndex();
            dprint(F("Current patch index: "));
            dprintln(pi);
        }
        else if (IS_COMMAND(command, COMMAND_GET_ID)) {
            _zoom->requestDeviceID();
            dprint(F("Device name: "));
            dprintln(_zoom->device_name);
            dprint(F("Firmware version: "));
            dprintln(_zoom->fw_version);
        }
    }
#endif
}

void handle_midi_input() {
    static byte midiBuffer[3];
    static byte bufferIndex = 0;
    static bool expectingData = false;
    static byte expectedBytes = 0;
    
    while (_midi_serial.available()) {
        byte b = _midi_serial.read();
        
        if (b & 0x80) {
            // MSB is 1 => status byte
            bufferIndex = 0;
            midiBuffer[bufferIndex++] = b;
            
            // Determine how many data bytes to expect
            byte messageType = b & MIDI_STATUS_MASK;
            switch (messageType) {
                case PROGRAM_CHANGE:
                    dprintln("MIDI PC");
                case CHANNEL_PRESSURE: // Channel Pressure
                    expectedBytes = 1;
                    break;
                case 0x80: // Note Off
                case 0x90: // Note On
                case 0xA0: // Polyphonic Key Pressure
                case CONTROL_CHANGE: // Control Change
                    dprintln("MIDI CC");
                case 0xE0: // Pitch Bend
                    expectedBytes = 2;
                    break;
                default:
                    expectedBytes = 0; // System messages handled separately
                    break;
            }
            expectingData = (expectedBytes > 0);
        } else if (expectingData && bufferIndex < 3) {
            // data byte
            midiBuffer[bufferIndex++] = b;
            expectedBytes--;
            
            // Check if we have a complete message
            if (expectedBytes == 0) {
                MidiMessage msg;
                msg.status = midiBuffer[0];
                msg.data1 = (bufferIndex > 1) ? midiBuffer[1] : 0;
                msg.data2 = (bufferIndex > 2) ? midiBuffer[2] : 0;
                msg.length = bufferIndex;
                
                // Process the complete MIDI message
                processMidiMessage(msg);
                
                expectingData = false;
                bufferIndex = 0;
            }
        }
    }
}

void processMidiMessage(const MidiMessage& msg) {
    // byte messageType = msg.status & MIDI_STATUS_MASK;
    byte channel = (msg.status & MIDI_CHANNEL_MASK) + 1; // Convert to 1-16
    
    // Always send message to MIDI thru
    // sendMidiThru(msg);
    
    // Check if this is a Program Change on our target channel
    dprint(F("process midi "));
    dprint(F("chan: "));
    dprintln(channel);
    dprint(F("stat: "));
    dprintln(msg.status);
    dprint(F("d1: "));
    dprintln(msg.data1);
    dprint(F("d2: "));
    dprintln(msg.data2);
    
    if (channel == TARGET_MIDI_CHANNEL) {
        if (msg.status == PROGRAM_CHANGE) {
            dprint(F("Program Change detected - Channel: "));
            dprint(channel);
            dprint(F(", Program: "));
            dprintln(msg.data1);

            sendProgramChange(msg.data1);
            
            // sendUsbMidi(channel - 1, msg.data1); // USB MIDI uses 0-15 for channels
            
            // printMidiMessage(msg);
        }
        else if(msg.status == CONTROL_CHANGE) {
            dprint(F("Control Change detected - Channel: "));
            dprint(channel);
            dprint(F(", data1: "));
            dprintln(msg.data1);
            dprint(F(", data2: "));
            dprintln(msg.data2);

            if (msg.data1 == COMMAND_TOGGLE_BYPASS) {
                _zoom->toggleBypass();
            }
            else if (msg.data1 == COMMAND_TOGGLE_FULL) {
                _zoom->toggleFullBypass();
            }
            else if (msg.data1 == COMMAND_TOGGLE_TUNER) {
                _zoom->toggleTuner();
            }
            else if (msg.data1 == COMMAND_NEXT_PATCH) {
                _zoom->incPatch(1);
            }
            else if (msg.data1 == COMMAND_PREV_PATCH) {
                _zoom->incPatch(-1);
            }
        }
    }
    else {
        // other channel
        dprintln("other channel");
    }
}

void sendMidiThru(const MidiMessage& msg) {
    // Send complete message to MIDI thru port
    _midi_serial.write(msg.status);
    if (msg.length > 1) {
        _midi_serial.write(msg.data1);
    }
    if (msg.length > 2) {
        _midi_serial.write(msg.data2);
    }
}

void sendUsbMidi(byte channel, byte program) {
    // if (_midi_usb) {
    //     // Create USB MIDI message
    //     // USB MIDI format: Cable Number + Code Index Number, MIDI bytes
    //     byte usbMidiMsg[4];
    //     usbMidiMsg[0] = 0x0C; // Cable 0, Program Change
    //     usbMidiMsg[1] = PROGRAM_CHANGE | channel; // Status byte
    //     usbMidiMsg[2] = program; // Program number
    //     usbMidiMsg[3] = 0x00; // Unused for Program Change
        
    //     // Send via USB
    //     _midi_usb.SendData(usbMidiMsg);
        
    //     Serial.println(F("Program Change sent via USB"));
    // } else {
    //     Serial.println(F("USB MIDI device not connected"));
    // }
}

// void printMidiMessage(const MidiMessage& msg) {
//     Serial.print(F("MIDI: "));
//     Serial.print(msg.status, HEX);
//     Serial.print(F(" "));
//     if (msg.length > 1) {
//         Serial.print(msg.data1, HEX);
//         Serial.print(F(" "));
//     }
//     if (msg.length > 2) {
//         Serial.print(msg.data2, HEX);
//     }
//     Serial.println();
// }


void sendProgramChange(uint8_t pc) {
    _zoom->sendPatch(pc);
}