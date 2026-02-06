#pragma once

#define ZOOM_MS_MAX_PATCHES             (50)

class ZoomMSDevice {

private:
    uint8_t 	        _readBuffer[MIDI_MAX_SYSEX_SIZE] = {0};
    uint8_t             _patchLen;
    uint8_t             _deviceID;
    int8_t              _current_patch_index = 0; // could be negative
    
public:
    bool                        tuner_enabled = false;
    // bool                        bypassed = false;
    char                        patch_name[11] = {0};
    const __FlashStringHelper * device_name;
    char                        fw_version[5] = {0};

private:
    void debugReadBuffer(const __FlashStringHelper * aMessage, bool aIsSysEx);
    void readResponse();
    void sendBytes(uint8_t * aBytes, const __FlashStringHelper * aMessage = NULL);
    
public:
    ZoomMSDevice();
    void tick();
    int8_t requestPatchIndex();
    void requestPatchData();
    void requestDeviceID();
    void sendPatch(uint8_t patch_index);
    void enableEditorMode(bool aEnable);
    void incPatch(int8_t aOffset, bool aGetIndexOnly = true);
    void toggleTuner();
    void enableTuner(bool aEnable);
    // void toggleBypass();
    // void toggleFullBypass();

    // void enableBypass(bool aEnable);
    // void enableFullBypass(bool aEnable);

};