
#include <Usb.h>
#include <usbh_midi.h>

#include "zoom_ms.h"
#include "debug.h"

// Zoom device characteristics, don't change
#define DEV_ID_MS_50G 				(0x58)
#define DEV_ID_MS_70CDR 			(0x61)
#define DEV_ID_MS_60B 				(0x5f)
#define DEV_PLEN_MS_50G 			(146)
#define DEV_PLEN_MS_70CDR 			(146)
#define DEV_PLEN_MS_60B 			(105)
#define DEV_NAME_MS_50G				F("MS-50G")
#define DEV_NAME_MS_70CDR			F("MS-70CDR")
#define DEV_NAME_MS_60G				F("MS-60G")
#define DEV_NAME_INVALID			F("INVALID")
#define DEV_MAX_FX_PER_PATCH        (5)

#define USB_READ_TIMEOUT            (300)

USB         _usb;
USBH_MIDI   _midi(&_usb);

// id request
uint8_t 			ID_PAK[] = { 0xf0, 0x7e, 0x00, 0x06, 0x01, 0xf7 };
// set editor mode on / off
uint8_t 			EM_PAK[] = { 0xf0, 0x52, 0x00, 0xff /* device ID */, 0x50 /* 0x50: on - 0x51: off */, 0xf7 };
// get patch index
uint8_t 			PI_PAK[] = { 0xf0, 0x52, 0x00, 0xff /* device ID */, 0x33, 0xf7 };
// get patch data
uint8_t 			PD_PAK[] = { 0xf0, 0x52, 0x00, 0xff /* device ID */, 0x29, 0xf7};
// set tuner mode on / off
uint8_t 			TU_PAK[] = { 0xb0, 0x4a, 0x00 /* 0x41: on - 0x0: off */ };
// program change
uint8_t 			PC_PAK[] = { 0xc0, 0x00 /* program number */ };
// bypass effect
uint8_t 			BP_PAK[] = { 0xf0, 0x52, 0x00, 0xff /* device ID */, 0x31, 0x00 /* effect slot number */, 0x00, 0x00 /* 1: on, 0: off */, 0x00, 0x00, 0xf7 };

void debug(uint8_t * buffer, int len, const __FlashStringHelper * aMessage, bool aIsSysEx) {
    dprintln(aMessage);
    int i = 0;
    for (i = 0; i < /*MIDI_MAX_SYSEX_SIZE*/len; i++) {
        dprint("0x");
        if (buffer[i] < 16) 
            dprint("0");
        hprint(buffer[i]);
        dprint(", ");
        if(aIsSysEx && buffer[i] == 0xf7) {
            i++;
            break;
        }
    }

    dprintln("");
    dprint(F("NUM BYTES: "));
    dprintln(i);
}


void ZoomMSDevice::init() {

    _usb.Init();
    int state = 0; 
    uint32_t wait_ms = 0;
    int inc_ms = 100;
    do {
        _usb.Task();
        state = _usb.getUsbTaskState();
        dprint(F("USB STATE: "));
        dprintln(state);
        delay(inc_ms);
        wait_ms += inc_ms;
    } while(state != USB_STATE_RUNNING);

    dprint(F("USB RUNNING "));
    dprint(F(" - Exit loop wait time ms: "));
    dprintln(wait_ms);
    
    
    // identify
    _usb.Task();
    requestDeviceID();
    delay(1000);
    _usb.Task();
    requestDeviceID();

    // disable tuner
    // sendBytes(TU_PAK);
    // requestPatchIndex();
    // enableEditorMode(true);
    // requestPatchData(false);
}

/**
 * @brief Send bytes thru MIDI over USB
 * 
 * @param aBytes byte buffer to send
 * @param aMessage debug message to print
 */
void ZoomMSDevice::sendBytes(uint8_t * aBytes, const __FlashStringHelper * aMessage) {
    // dprint(aMessage);
    _usb.Task();
    // debug(aBytes, 8, F("") , false);
    _midi.SendData(aBytes);
}

void ZoomMSDevice::readResponse() {
    uint16_t recv_read = 0;
    uint16_t recv_count = 0;
    uint8_t rcode = 0;
    
    unsigned long timeout = millis() + USB_READ_TIMEOUT;
    do {
        _usb.Task();
        rcode = _midi.RecvData(&recv_read, (uint8_t *)(_readBuffer + recv_count));
        if(rcode == 0 && recv_read > 0) {
            recv_count += recv_read;
        }
        if (millis() > timeout) {
            dprintln(F("readResponse timeout"));
            break;
        }
        // exit if we received a complete sysex (ends with 0xF7)
        if (recv_count > 0 && _readBuffer[recv_count - 1] == 0xF7) {
            break;
        }
    } while(rcode == 0);
    
    // remove MIDI packet's 1st byte - FIX: only process recv_count bytes
    for(uint16_t i = 0, j = 0; i < recv_count && i < MIDI_MAX_SYSEX_SIZE - 3; i += 4) {
        if (i + 3 < recv_count) {
            _readBuffer[j++] = _readBuffer[i + 1];
            _readBuffer[j++] = _readBuffer[i + 2];
            _readBuffer[j++] = _readBuffer[i + 3];
        }
    }
}

void ZoomMSDevice::enableEditorMode(bool aEnable) {
	EM_PAK[4] = aEnable ? 0x50 : 0x51;
    sendBytes(EM_PAK, F("EDITOR ON\n"));
}

int8_t ZoomMSDevice::requestPatchIndex() {
    sendBytes(PI_PAK, F("REQ PATCH INDEX\n"));
    readResponse();
    _current_patch_index = _readBuffer[7];
    return _current_patch_index;
}

void ZoomMSDevice::requestDeviceID() {
    sendBytes(ID_PAK, F("REQ DEVICE ID\n"));
    readResponse();

    uint8_t device_id = _readBuffer[6];
    switch (device_id) {
    case DEV_ID_MS_50G:
        _patchLen = DEV_PLEN_MS_50G;
        _deviceName = DEV_NAME_MS_50G;
        break;
    case DEV_ID_MS_70CDR:
        _patchLen = DEV_PLEN_MS_70CDR;
        _deviceName = DEV_NAME_MS_70CDR;
        break;
    case DEV_ID_MS_60B:
        _patchLen = DEV_PLEN_MS_60B;
        _deviceName = DEV_NAME_MS_60G;
        break;
    default:
        break;
    }
    BP_PAK[3] = device_id;
    EM_PAK[3] = device_id;
    PI_PAK[3] = device_id;
    PD_PAK[3] = device_id;
    _firmwareVersion[0] = _readBuffer[10];
    _firmwareVersion[1] = _readBuffer[11];
    _firmwareVersion[2] = _readBuffer[12];
    _firmwareVersion[3] = _readBuffer[13];
    
    dprint(F("DEVICE ID: "));
    dprintln(device_id);

    dprint(F("DEVICE NAME: "));
    dprintln(_deviceName);

    dprint(F("DEVICE FW: "));
    dprintln(_firmwareVersion);

    dprint(F("PATCH LEN: "));
    dprintln(_patchLen);
}

void ZoomMSDevice::requestPatchData() {
    sendBytes(PD_PAK, F("REQ DATA\n"));
    readResponse();

    patch_name[0] = _readBuffer[_patchLen - 14];
    patch_name[1] = _readBuffer[_patchLen - 12];
    patch_name[2] = _readBuffer[_patchLen - 11];
    patch_name[3] = _readBuffer[_patchLen - 10];
    patch_name[4] = _readBuffer[_patchLen - 9];
    patch_name[5] = _readBuffer[_patchLen - 8];
    patch_name[6] = _readBuffer[_patchLen - 7];
    patch_name[7] = _readBuffer[_patchLen - 6];
    patch_name[8] = _readBuffer[_patchLen - 4];
    patch_name[9] = _readBuffer[_patchLen - 3];
    patch_name[10] = '\0';

    // bypassed = _readBuffer[6] & 0x1;
    dprintln(patch_name);
    debug((uint8_t*) patch_name, 11, F("hex name"), false);
}


void ZoomMSDevice::sendPatch(uint8_t patch_index) {
    // send current patch number MIDI over USB
    if (patch_index >= 0 && patch_index < ZOOM_MS_MAX_PATCHES) {
        PC_PAK[1] = patch_index;
        sendBytes(PC_PAK, F("Sending patch number: "));
        dprint("sending patch: ");
        dprintln(patch_index);
    }
}

void ZoomMSDevice::incPatch(int8_t aOffset, bool aGetIndexOnly) {
    
    // clamp between 0 and max_patches - 1
    _current_patch_index += aOffset;
    _current_patch_index = _current_patch_index > (ZOOM_MS_MAX_PATCHES - 1) ? 0 : _current_patch_index;
    _current_patch_index = _current_patch_index < 0 ? (ZOOM_MS_MAX_PATCHES -1) : _current_patch_index;

    sendPatch(_current_patch_index);

    if (aGetIndexOnly) {
        requestPatchIndex();
    } else {
        requestPatchData();
    }
}

// void ZoomMSDevice::toggleFullBypass() {
//     static bool bypassed = true;
//     bypassed = !bypassed;
//     BP_PAK[7] = bypassed ? 0x1 : 0x0;
//     for (int i = 0; i < DEV_MAX_FX_PER_PATCH; i++) {
//         BP_PAK[5] = i;
//         sendBytes(BP_PAK);
//     }
//     dprintln(bypassed ? F("Full Bypass ON") : F("Full Bypass OFF"));
// }


// void ZoomMSDevice::enableFullBypass(bool aEnabled) {
//     BP_PAK[7] = aEnabled ? 0x0 : 0x1;
//     for (int i = 0; i < DEV_MAX_FX_PER_PATCH; i++) {
//         BP_PAK[5] = i;
//         sendBytes(BP_PAK);
//     }
//     // bypassed = aEnabled;
//     dprintln(aEnabled ? F("Full Bypass ON") : F("Full Bypass OFF"));
// }

// void ZoomMSDevice::toggleBypass() {
// 	bypassed = !bypassed;
//     BP_PAK[5] = 0; // consider the 1st slot to be the line selector
// 	BP_PAK[7] = bypassed ? 0x1 : 0x0;
//     sendBytes(BP_PAK, bypassed ? F("FX Bypass ON") : F("FX Bypass OFF"));
//     dprintln("");
// }

void ZoomMSDevice::toggleTuner() {
	tuner_enabled = !tuner_enabled;
	TU_PAK[2] = tuner_enabled ? 0x41 : 0x0;
    sendBytes(TU_PAK, tuner_enabled ? F("Tuner ON") : F("Tuner OFF"));
    dprintln("");
}


void ZoomMSDevice::enableTuner(bool aEnable) {
    TU_PAK[2] = aEnable ? 0x41 : 0x0;
    sendBytes(TU_PAK, aEnable ? F("Tuner ON") : F("Tuner OFF"));
	tuner_enabled = aEnable;
    dprintln("");
}

void ZoomMSDevice::tick() {
    _usb.Task();
}