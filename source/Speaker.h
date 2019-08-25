#pragma once

// Registry soundtype:
#define  REG_SOUNDTYPE_NONE    0
#define  REG_SOUNDTYPE_DIRECT  1    // Not supported from 1.26
#define  REG_SOUNDTYPE_SMART   2    // Not supported from 1.26
#define  REG_SOUNDTYPE_WAVE    3

enum ESoundType {
    SOUNDTYPE_NONE = 0,
    SOUNDTYPE_WAVE
};

extern ESoundType g_soundType;
extern double     g_fClksPerSpkrSample;
extern bool       g_bQuieterSpeaker;
extern short      g_nSpeakerData;

void    SpkrDestroy ();
void    SpkrInitialize ();
void    SpkrReinitialize ();
void    SpkrReset();
BOOL    SpkrSetEmulationType (HWND window, ESoundType newSoundType);
void    SpkrUpdate (DWORD);
void    SpkrUpdate_Timer();
DWORD   SpkrGetVolume();
void    SpkrSetVolume(DWORD dwVolume, DWORD dwVolumeMax);
void    Spkr_Mute();
void    Spkr_Demute();
bool    Spkr_IsActive();
bool    Spkr_DSInit();
void    Spkr_DSUninit();
void    SpkrSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    SpkrLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
