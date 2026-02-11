#pragma once

#define ZOOM_MS_MAX_PATCHES             (50)
#ifndef MIDI_MAX_SYSEX_SIZE
#define MIDI_MAX_SYSEX_SIZE (256)
#endif

class ZoomMSDevice {

private:
    uint8_t 	                _readBuffer[MIDI_MAX_SYSEX_SIZE] = {0};
    uint8_t                     _patchLen;
    int8_t                      _current_patch_index = 0; // could be negative
    const __FlashStringHelper * _deviceName;
    char                        _firmwareVersion[5] = {0};
    
    
public:
    bool                        tuner_enabled = false;
    // bool                        bypassed = false;
    char                        patch_name[11] = {0};

private:
    void debugReadBuffer(const __FlashStringHelper * aMessage, bool aIsSysEx);
    void readResponse();
    void sendBytes(uint8_t * aBytes, const __FlashStringHelper * aMessage = NULL);

public:
    ZoomMSDevice() {};
    void tick();
    void init();
    int8_t requestPatchIndex();
    void requestPatchData();
    void requestDeviceID();
    void sendPatch(uint8_t patch_index);
    void enableEditorMode(bool aEnable);
    void incPatch(int8_t aOffset, bool aGetIndexOnly = true);
    void enableTuner(bool aEnable);
    void toggleTuner();
    const __FlashStringHelper * getDeviceName() {
        return _deviceName;
    }
    const char * getFirmwareVersion() {
        return _firmwareVersion;
    }
    // void toggleBypass();
    // void toggleFullBypass();

    // void enableBypass(bool aEnable);
    // void enableFullBypass(bool aEnable);

};