#include <MIDI.h>
#include <SoftwareSerial.h>
#include <usbh_midi.h>
#include <usbhub.h>
#include "debug.h"

#define PIN_LED_STATUS LED_BUILTIN

#define BLINK_RATE_SERIAL_MS (100)
#define BLINK_RATE_USB_MS (300)

const uint8_t MIDI_RX_PIN = 8; // DIN MIDI IN (from optocoupler output)
const uint8_t MIDI_TX_PIN = 9; // DIN MIDI THRU out

// =======================
// USB Host Shield objects
// =======================
USB Usb;
USBH_MIDI MidiUSB(&Usb);
SoftwareSerial _midi_serial(MIDI_RX_PIN, MIDI_TX_PIN); // RX, TX

// =======================
// MIDI DIN input setup
// =======================
// RX pin for MIDI DIN input
MIDI_CREATE_INSTANCE(SoftwareSerial, _midi_serial, MIDI_DIN);

// For MIDI Thru, we'll use Serial1-equivalent TX pin of Pro Mini
// (Actually, Pro Mini has only Serial, so we'll handle thru in software)

void blink(uint32_t rate_ms)
{
    static bool blink = true;
    static uint64_t ts = 0;
    if (millis() - ts > rate_ms)
    {
        digitalWrite(PIN_LED_STATUS, blink);
        blink = !blink;
        ts = millis();
    }
}

// =======================
// Setup
// =======================
void setup()
{

    pinMode(PIN_LED_STATUS, OUTPUT);

    dprintinit.begin(115200); // Debugging
    _midi_serial.begin(31250);  // MIDI baud rate

    if (Usb.Init() == -1)
    {
        // infinite loop error
        dprintln(F("USB Host Shield init failed"));
        while (1)
        {
            blink(BLINK_RATE_USB_MS);
        }
    }

    MIDI_DIN.begin(MIDI_CHANNEL);
    MIDI_DIN.turnThruOff(); // We'll do our own THRU
}

// =======================
// Loop
// =======================
void loop()
{
    Usb.Task();

    // Check for messages from DIN
    if (MIDI_DIN.read())
    {
        byte type = MIDI_DIN.getType();
        byte chan = MIDI_DIN.getChannel();
        byte data1 = MIDI_DIN.getData1();
        byte data2 = MIDI_DIN.getData2();

        // Manual THRU: send raw bytes back out DIN TX
        _midi_serial.write(MIDI_DIN.getRawData(), MIDI_DIN.getRawDataSize());

        // If Program Change on target channel
        if (type == midi::ProgramChange && chan == MIDI_CHANNEL)
        {
            sendProgramChangeToUSB(data1, chan);
        }
    }
}

// =======================
// Send PC to USB device
// =======================
void sendProgramChangeToUSB(byte program, byte channel)
{
    uint8_t msg[2];
    msg[0] = 0xC0 | ((channel - 1) & 0x0F);
    msg[1] = program & 0x7F;
    MidiUSB.SendData(msg);
}