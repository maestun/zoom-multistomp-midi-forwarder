#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Usb.h>
#include <usbh_midi.h>
#include "zoom_ms.h"
#include "debug.h"

// Configuration
#define MIDI_BAUD_RATE 31250   // Standard MIDI baud rate

// Pin definitions
#define MIDI_RX_PIN 4          // SoftwareSerial RX pin for MIDI input
#define MIDI_TX_PIN 5          // SoftwareSerial TX pin for MIDI thru
// #define MIDI_TEST_TX_PIN 6     // Pin for simulating MIDI input (test mode)
// #define TEST_BUTTON_PIN 3      // Button pin to trigger test MIDI messages

// MIDI message types
#define PROGRAM_CHANGE      0xC0
#define CONTROL_CHANGE      0xB0
#define MIDI_STATUS_MASK    0xF0
#define MIDI_CHANNEL_MASK   0x0F

// Serial debug commands
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

// MIDI CC commands


// USB and MIDI objects
SoftwareSerial  _midi_serial(MIDI_RX_PIN, MIDI_TX_PIN);
// SoftwareSerial  _midi_test_serial(MIDI_TEST_TX_PIN, MIDI_TEST_TX_PIN); // TX only for test output
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
void handle_midi_input();
void processMidiMessage(const MidiMessage& msg);
void sendMidiThru(const MidiMessage& msg);
void sendUsbMidi(byte channel, byte program);
void printMidiMessage(const MidiMessage& msg);
// void sendTestMidiProgramChange(byte channel, byte program);
// void handleTestButton();
void sendRawMidiBytes(byte* bytes, int length);

void setup() {
    Serial.begin(9600);
    // Serial.println(F("MIDI Program Change Router"));
    // Serial.println(F("USB Host Shield + 5-pin MIDI"));
    
    // Initialize pins
    // pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
    // pinMode(MIDI_TEST_TX_PIN, OUTPUT);
    // digitalWrite(MIDI_TEST_TX_PIN, HIGH); // MIDI idle state is HIGH
    
    // Initialize MIDI serial communication
    _midi_serial.begin(MIDI_BAUD_RATE);
    // _midi_test_serial.begin(MIDI_BAUD_RATE);
    Serial.println(F("MIDI Serial initialized"));
        
    // Serial.print(F("Listening for Program Change on MIDI channel: "));
    // Serial.println(TARGET_MIDI_CHANNEL);
    // Serial.print(F("Press button on pin "));
    // Serial.print(TEST_BUTTON_PIN);
    // Serial.println(" to send test MIDI messages");
    // Serial.println(F("Send 't' via Serial to toggle automatic test mode"));
    // Serial.println(F("Ready..."));

    _zoom = new ZoomMSDevice();
}

void loop() {
    handle_midi_input();
    
    // Handle test button
    // handleTestButton();
    
    // Handle serial commands
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

        // if (command == 't' || command == 'T') {
        //     testMode = !testMode;
        //     Serial.print(F("Test mode: "));
        //     Serial.println(testMode ? F("ON") : F("OFF"));
        //     if (testMode) {
        //         Serial.println(F("Sending test Program Change every 3 seconds..."));
        //     }
        // }
    }
    
    // Automatic test mode
    // if (testMode && millis() - lastTestMessage > 3000) {
    //     sendTestMidiProgramChange(TARGET_MIDI_CHANNEL, testProgramNumber);
    //     testProgramNumber = (testProgramNumber + 1) % 128; // Cycle through 0-127
    //     lastTestMessage = millis();
    // }
    

    // Check for USB MIDI device connection
    // if (_midi_usb) {
    //     // Handle incoming MIDI from 5-pin DIN
    //     handleMidiInput();
    // } else {
    //     // Check periodically for USB device connection
    //     static unsigned long lastCheck = 0;
    //     if (millis() - lastCheck > 1000) {
    //         Serial.println(F("Waiting for USB MIDI device..."));
    //         lastCheck = millis();
    //     }
    // }
}

void handle_midi_input() {
    static byte midiBuffer[3];
    static byte bufferIndex = 0;
    static bool expectingData = false;
    static byte expectedBytes = 0;
    
    while (_midi_serial.available()) {
        byte incomingByte = _midi_serial.read();
        
        dprint(F("midi byte: "));
        dprintln(incomingByte);

        // Check if this is a status byte (MSB = 1)
        if (incomingByte & 0x80) {
            // This is a status byte
            bufferIndex = 0;
            midiBuffer[bufferIndex++] = incomingByte;
            
            // Determine how many data bytes to expect
            byte messageType = incomingByte & MIDI_STATUS_MASK;
            switch (messageType) {
                case PROGRAM_CHANGE:
                    dprintln("MIDI PC");
                case 0xD0: // Channel Pressure
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
            // This is a data byte
            midiBuffer[bufferIndex++] = incomingByte;
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
    byte messageType = msg.status & MIDI_STATUS_MASK;
    byte channel = (msg.status & MIDI_CHANNEL_MASK) + 1; // Convert to 1-16
    
    // Always send message to MIDI thru
    // sendMidiThru(msg);
    
    // Check if this is a Program Change on our target channel
    dprint(F("process midi chan: "));
            dprint(channel);
            
            dprintln(msg.data1);
            dprintln(msg.data2);
    
    if (channel == TARGET_MIDI_CHANNEL) {
        if (messageType == PROGRAM_CHANGE) {
            dprint(F("Program Change detected - Channel: "));
            dprint(channel);
            dprint(F(", Program: "));
            dprintln(msg.data1);
            
            // Send Program Change via USB
            sendUsbMidi(channel - 1, msg.data1); // USB MIDI uses 0-15 for channels
            
            printMidiMessage(msg);
        }
        else if(messageType == CONTROL_CHANGE) {
            dprint(F("Control Change detected - Channel: "));
            dprint(channel);
            dprint(F(", data1: "));
            dprintln(msg.data1);
            dprint(F(", data2: "));
            dprintln(msg.data2);
            
            // Send Program Change via USB
            // sendUsbMidi(channel - 1, msg.data1); // USB MIDI uses 0-15 for channels
            
            // printMidiMessage(msg);
        }
    }
    else {
        // other midi
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

void printMidiMessage(const MidiMessage& msg) {
    Serial.print(F("MIDI: "));
    Serial.print(msg.status, HEX);
    Serial.print(F(" "));
    if (msg.length > 1) {
        Serial.print(msg.data1, HEX);
        Serial.print(F(" "));
    }
    if (msg.length > 2) {
        Serial.print(msg.data2, HEX);
    }
    Serial.println();
}

void sendTestMidiProgramChange(byte channel, byte program) {
    // Serial.print(F("Sending test Program Change - Channel: "));
    // Serial.print(channel);
    // Serial.print(F(", Program: "));
    // Serial.println(program);
    
    // // Create Program Change message
    // byte midiMessage[2];
    // midiMessage[0] = PROGRAM_CHANGE | (channel - 1); // Status byte (channel 1-16 -> 0-15)
    // midiMessage[1] = program & 0x7F; // Program number (0-127, ensure MSB is 0)
    
    // // Send raw MIDI bytes
    // sendRawMidiBytes(midiMessage, 2);
    _zoom->incPatch(1, true);
}


void sendProgramChange(uint8_t pc) {
    _zoom->sendPatch(pc);
}


// void handleTestButton() {
//     static bool lastButtonState = HIGH;
//     static unsigned long lastDebounceTime = 0;
//     // static byte buttonProgramNumber = 0;
    
//     bool buttonState = digitalRead(TEST_BUTTON_PIN);
    
//     // Debounce the button
//     if (buttonState != lastButtonState) {
//         lastDebounceTime = millis();
//     }
    
//     if ((millis() - lastDebounceTime) > 50) { // 50ms debounce
//         if (buttonState == LOW && lastButtonState == HIGH) {
//             // Button pressed
//             // sendTestMidiProgramChange(TARGET_MIDI_CHANNEL, buttonProgramNumber);



//             // buttonProgramNumber = (buttonProgramNumber + 1) % 128;

//             _zoom->incPatch(1, true);
//         }
//     }
    
//     lastButtonState = buttonState;
// }

// void sendRawMidiBytes(byte* bytes, int length) {
//     // Send bytes with proper MIDI timing
//     for (int i = 0; i < length; i++) {
//         // Method 1: Direct bit manipulation (most accurate)
//         byte dataByte = bytes[i];
        
//         // Start bit (LOW)
//         digitalWrite(MIDI_TEST_TX_PIN, LOW);
//         delayMicroseconds(32); // 1/31250 ≈ 32µs per bit
        
//         // Data bits (LSB first)
//         for (int bit = 0; bit < 8; bit++) {
//             digitalWrite(MIDI_TEST_TX_PIN, (dataByte >> bit) & 1);
//             delayMicroseconds(32);
//         }
        
//         // Stop bit (HIGH)
//         digitalWrite(MIDI_TEST_TX_PIN, HIGH);
//         delayMicroseconds(32);
        
//         // Small gap between bytes
//         if (i < length - 1) {
//             delayMicroseconds(32);
//         }
//     }
    
//     // Alternative method using SoftwareSerial (comment out the above and uncomment this if preferred)
//     /*
//     for (int i = 0; i < length; i++) {
//         _midi_test_serial.write(bytes[i]);
//         delayMicroseconds(320); // Small delay between bytes
//     }
//     */
// }