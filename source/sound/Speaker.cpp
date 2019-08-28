/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Speaker emulation
 *
 * Author: Various
 */

#include "StdAfx.h"
#include "cpu/CPU.h"
#include "video/Frame.h"
#include "util/Log.h"
#include "devices/Memory.h"
#include "sound/Sound.h"
#include "sound/Speaker.h"
#include "video/Video.h"  // VideoRedrawScreen()
#include "state/YamlHelper.h"
#include "debug/Debug.h"

// Notes:
//
// [OLD: 23.191 Apple CLKs == 44100Hz (CLK_6502/44100)]
// 23 Apple CLKS per PC sample (played back at 44.1KHz)
//
//
// The speaker's wave output drives how much 6502 emulation is done in real-time, eg:
// If the speaker's wave buffer is running out of sample-data, then more 6502 cycles
// need to be executed to top-up the wave buffer.
// This is in contrast to the AY8910 voices, which can simply generate more data if
// their buffers are running low.
//

const uint16_t SPEAKER_CHANNEL_COUNT = 1;
const unsigned SPEAKER_BUFSIZE = MAX_SAMPLES * sizeof(short) * SPEAKER_CHANNEL_COUNT;
const int16_t  SPEAKER_DATA_INIT = (int16_t)0x8000;

// Application-wide globals:
int16_t         g_nSpeakerData = SPEAKER_DATA_INIT;
ESoundType      g_soundType = SOUNDTYPE_WAVE;
double          g_fClksPerSpkrSample;           // Setup in SetClksPerSpkrSample()
bool            g_bQuieterSpeaker = false;    // Allow temporary quietening of speaker (8 bit DAC)

// Static globals
static UINT     s_nBufferIdx = 0;
static short * s_pRemainderBuffer = NULL;
static UINT     s_nRemainderBufferSize;     // Setup in SpkrInitialize()
static UINT     s_nRemainderBufferIdx;      // Setup in SpkrInitialize()
static int16_t * s_pSpeakerBuffer = NULL;
static DWORD    s_lastCycleNum = 0;
static DWORD    s_toggles = 0;
static uint64_t s_nSpkrQuietCycleCount = 0;
static uint64_t s_nSpkrLastCycle = 0;
static bool     s_bSpkrToggleFlag = false;
static Voice    s_speakerVoice = { 0 };
static bool     s_bSpkrAvailable = false;

//-----------------------------------------------------------------------------

// Forward refs:
ULONG   Spkr_SubmitWaveBuffer_FullSpeed(short * pSpeakerBuffer, ULONG nNumSamples);
ULONG   Spkr_SubmitWaveBuffer(short * pSpeakerBuffer, ULONG nNumSamples);
void    Spkr_SetActive(bool bActive);

//=============================================================================

static void DisplayBenchmarkResults() {
    DWORD totaltime = GetTickCount() - extbench;
    VideoRedrawScreen();
    TCHAR buffer[64];
    wsprintf(buffer,
        TEXT("This benchmark took %u.%02u seconds."),
        (unsigned)(totaltime / 1000),
        (unsigned)((totaltime / 10) % 100));
    MessageBox(g_hFrameWindow,
        buffer,
        TEXT("Benchmark Results"),
        MB_ICONINFORMATION | MB_SETFOREGROUND);
}

//=============================================================================
//
// DC filtering V2  (Riccardo Macri May 2015) (GH#275)
//
// To prevent loud clicks on Window's sound buffer underruns and constant DC
// being sent out to amplifiers (some soundcards are DC coupled) which is
// not good for them, an attenuator slowly drops the speaker output
// to 0 after the speaker (or 8 bit DAC) has been idle for a couple hundred
// milliseconds.
//
// The approach works as follows:
// - SpkrToggle() is called when the speaker state is flipped by accessing $C030
// - This brings audio up to date then calls ResetDCFilter()
// - ResetDCFilter() sets a counter to a high value
// - every audio sample is processed by DCFilter() as follows:
//   - if the counter is >= 32768, the speaker has been recently toggled
//     and the samples are unaffected
//   - if the counter is < 32768 but > 0, it is used to scale the
//     sample to reduce +ve or -ve speaker states towards zero
//   - In the two cases above, the counter is decremented
//   - if the counter is zero, the speaker has been silent for a while
//     and the output is 0 regardless of the speaker state.
//
// - the initial "high value" is chosen so 10000/44100 = about a
//   quarter of a second of speaker inactivity is needed before attenuation
//   begins.
//
//   NOTE: The attenuation is not ever reducing the level of audio, just
//         the DC offset at which the speaker has been left.
//
//  This approach has zero impact on any speaker tones including PWM
//  due to the samples being unchanged for at least 0.25 seconds after
//  any speaker activity.
//

static UINT g_uDCFilterState = 0;

inline void ResetDCFilter(void) {
    // reset the attenuator with an additional 250ms of full gain
    // (10000 samples) before it starts attenuating
    g_uDCFilterState = 32768 + 10000;
}

inline short DCFilter(short sample_in) {
    if (g_uDCFilterState == 0)      // no sound for a while, stay 0
        return 0;

    if (g_uDCFilterState >= 32768)  // full gain after recent sound
    {
        g_uDCFilterState--;
        return sample_in;
    }

    return (((int)sample_in) * (g_uDCFilterState--)) / 32768;   // scale & divide by 32768 (NB. Don't ">>15" as undefined behaviour)
}

//=============================================================================

static void SetClksPerSpkrSample() {
    //  // 23.191 clks for 44.1Khz (when 6502 CLK=1.0Mhz)
    //  g_fClksPerSpkrSample = g_fCurrentCLK6502 / (double)SPKR_SAMPLE_RATE;

        // Use integer value: Better for MJ Mahon's RT.SYNTH.DSK (integer multiples of 1.023MHz Clk)
        // . 23 clks @ 1.023MHz
    g_fClksPerSpkrSample = (double)(UINT)(g_fCurrentCLK6502 / (double)SPKR_SAMPLE_RATE);
}

//=============================================================================

static void InitRemainderBuffer() {
    delete[] s_pRemainderBuffer;

    SetClksPerSpkrSample();

    s_nRemainderBufferSize = (UINT)g_fClksPerSpkrSample;
    if ((double)s_nRemainderBufferSize < g_fClksPerSpkrSample)
        s_nRemainderBufferSize++;

    s_pRemainderBuffer = new short[s_nRemainderBufferSize];
    memset(s_pRemainderBuffer, 0, s_nRemainderBufferSize);

    s_nRemainderBufferIdx = 0;
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//=============================================================================

void SpkrDestroy() {
    Spkr_DSUninit();

    //

    if (g_soundType == SOUNDTYPE_WAVE) {
        delete[] s_pSpeakerBuffer;
        delete[] s_pRemainderBuffer;

        s_pSpeakerBuffer = NULL;
        s_pRemainderBuffer = NULL;
    }
}

//=============================================================================

void SpkrInitialize() {
    if (g_fh) {
        fprintf(g_fh, "Spkr Config: soundtype = %d ", g_soundType);
        switch (g_soundType) {
        case SOUNDTYPE_NONE:   fprintf(g_fh, "(NONE)\n"); break;
        case SOUNDTYPE_WAVE:   fprintf(g_fh, "(WAVE)\n"); break;
        default:           fprintf(g_fh, "(UNDEFINED!)\n"); break;
        }
    }

    if (g_bDisableDirectSound) {
        s_speakerVoice.bMute = true;
    }
    else {
        s_bSpkrAvailable = Spkr_DSInit();
    }

    //

    if (g_soundType == SOUNDTYPE_WAVE) {
        InitRemainderBuffer();

        s_pSpeakerBuffer = new short[SPKR_SAMPLE_RATE];    // Buffer can hold a max of 1 seconds worth of samples
    }
}

//=============================================================================

// NB. Called when /g_fCurrentCLK6502/ changes
void SpkrReinitialize() {
    if (g_soundType == SOUNDTYPE_WAVE) {
        InitRemainderBuffer();
    }
}

//=============================================================================

void SpkrReset() {
    s_nBufferIdx = 0;
    s_nSpkrQuietCycleCount = 0;
    s_bSpkrToggleFlag = false;

    InitRemainderBuffer();
    Spkr_SubmitWaveBuffer(NULL, 0);
    Spkr_SetActive(false);
    Spkr_Demute();
}

//=============================================================================

BOOL SpkrSetEmulationType(HWND window, ESoundType newtype) {
    SpkrDestroy();    // GH#295: Destroy for all types (even SOUND_NONE)

    g_soundType = newtype;
    if (g_soundType != SOUNDTYPE_NONE)
        SpkrInitialize();

    if (g_soundType != newtype)
        switch (newtype) {

        case SOUNDTYPE_WAVE:
            MessageBox(window,
                TEXT("The emulator is unable to initialize a waveform ")
                TEXT("output device.  Make sure you have a sound card ")
                TEXT("and a driver installed and that windows is ")
                TEXT("correctly configured to use the driver.  Also ")
                TEXT("ensure that no other program is currently using ")
                TEXT("the device."),
                TEXT("Configuration"),
                MB_ICONEXCLAMATION | MB_SETFOREGROUND);
            return 0;

        }

    return 1;
}

//=============================================================================

static void ReinitRemainderBuffer(UINT nCyclesRemaining) {
    if (nCyclesRemaining == 0)
        return;

    for (s_nRemainderBufferIdx = 0; s_nRemainderBufferIdx < nCyclesRemaining; s_nRemainderBufferIdx++)
        s_pRemainderBuffer[s_nRemainderBufferIdx] = g_nSpeakerData;

    _ASSERT(s_nRemainderBufferIdx < s_nRemainderBufferSize);
}

static void UpdateRemainderBuffer(ULONG * pnCycleDiff) {
    if (s_nRemainderBufferIdx) {
        while ((s_nRemainderBufferIdx < s_nRemainderBufferSize) && *pnCycleDiff) {
            s_pRemainderBuffer[s_nRemainderBufferIdx] = g_nSpeakerData;
            s_nRemainderBufferIdx++;
            (*pnCycleDiff)--;
        }

        if (s_nRemainderBufferIdx == s_nRemainderBufferSize) {
            s_nRemainderBufferIdx = 0;
            signed long nSampleMean = 0;
            for (UINT i = 0; i < s_nRemainderBufferSize; i++)
                nSampleMean += (signed long)s_pRemainderBuffer[i];
            nSampleMean /= (signed long)s_nRemainderBufferSize;

            if (s_nBufferIdx < SPKR_SAMPLE_RATE - 1)
                s_pSpeakerBuffer[s_nBufferIdx++] = DCFilter((short)nSampleMean);
        }
    }
}

static void UpdateSpkr() {
    if (!g_bFullSpeed || SoundCore_GetTimerState()) {
        ULONG nCycleDiff = (ULONG)(g_nCumulativeCycles - s_nSpkrLastCycle);

        UpdateRemainderBuffer(&nCycleDiff);

        ULONG nNumSamples = (ULONG)((double)nCycleDiff / g_fClksPerSpkrSample);

        ULONG nCyclesRemaining = (ULONG)((double)nCycleDiff - (double)nNumSamples * g_fClksPerSpkrSample);

        while ((nNumSamples--) && (s_nBufferIdx < SPKR_SAMPLE_RATE - 1))
            s_pSpeakerBuffer[s_nBufferIdx++] = DCFilter(g_nSpeakerData);

        ReinitRemainderBuffer(nCyclesRemaining);  // Partially fill 1Mhz sample buffer
    }

    s_nSpkrLastCycle = g_nCumulativeCycles;
}

//=============================================================================

// Called by emulation code when Speaker I/O reg is accessed
//

BYTE __stdcall SpkrToggle(WORD, WORD, BYTE, BYTE, ULONG nExecutedCycles) {
    s_bSpkrToggleFlag = true;

    if (!g_bFullSpeed)
        Spkr_SetActive(true);

    if (extbench) {
        DisplayBenchmarkResults();
        extbench = 0;
    }

    if (g_soundType == SOUNDTYPE_WAVE) {
        CpuCalcCycles(nExecutedCycles);

        UpdateSpkr();

        short speakerDriveLevel = SPEAKER_DATA_INIT;
        if (g_bQuieterSpeaker)    // quieten the speaker if 8 bit DAC in use
            speakerDriveLevel /= 4; // NB. Don't shift -ve number right: undefined behaviour (MSDN says: implementation-dependent)

        ResetDCFilter();

        if (g_nSpeakerData == speakerDriveLevel)
            g_nSpeakerData = ~speakerDriveLevel;
        else
            g_nSpeakerData = speakerDriveLevel;
    }

    return MemReadFloatingBus(nExecutedCycles);
}

//=============================================================================

// Called by ContinueExecution()
void SpkrUpdate(DWORD totalcycles) {
    if (!s_bSpkrToggleFlag) {
        if (!s_nSpkrQuietCycleCount) {
            s_nSpkrQuietCycleCount = g_nCumulativeCycles;
        }
        else if (g_nCumulativeCycles - s_nSpkrQuietCycleCount > (unsigned __int64)g_fCurrentCLK6502 / 5) {
            // After 0.2 sec of Apple time, deactivate spkr voice
            // . This allows emulator to auto-switch to full-speed g_nAppMode for fast disk access
            Spkr_SetActive(false);
        }
    }
    else {
        s_nSpkrQuietCycleCount = 0;
        s_bSpkrToggleFlag = false;
    }

    //

    if (g_soundType == SOUNDTYPE_WAVE) {
        UpdateSpkr();
        ULONG nSamplesUsed;

        if (g_bFullSpeed)
            nSamplesUsed = Spkr_SubmitWaveBuffer_FullSpeed(s_pSpeakerBuffer, s_nBufferIdx);
        else
            nSamplesUsed = Spkr_SubmitWaveBuffer(s_pSpeakerBuffer, s_nBufferIdx);

        _ASSERT(nSamplesUsed <= s_nBufferIdx);
        memmove(s_pSpeakerBuffer, &s_pSpeakerBuffer[nSamplesUsed], s_nBufferIdx - nSamplesUsed);    // FIXME-TC: _Size * 2
        s_nBufferIdx -= nSamplesUsed;
    }
}

// Called from SoundCore_TimerFunc() for FADE_OUT
void SpkrUpdate_Timer() {
    if (g_soundType == SOUNDTYPE_WAVE) {
        UpdateSpkr();
        ULONG nSamplesUsed;

        nSamplesUsed = Spkr_SubmitWaveBuffer_FullSpeed(s_pSpeakerBuffer, s_nBufferIdx);

        _ASSERT(nSamplesUsed <= s_nBufferIdx);
        memmove(s_pSpeakerBuffer, &s_pSpeakerBuffer[nSamplesUsed], s_nBufferIdx - nSamplesUsed);  // FIXME-TC: _Size * 2 (GH#213?)
        s_nBufferIdx -= nSamplesUsed;
    }
}

//=============================================================================

static DWORD dwByteOffset = (DWORD)-1;
static int nNumSamplesError = 0;
static int nDbgSpkrCnt = 0;

// FullSpeed g_nAppMode, 2 cases:
// i) Short burst of full-speed, so PlayCursor doesn't complete sound from previous fixed-speed session.
// ii) Long burst of full-speed, so PlayCursor completes sound from previous fixed-speed session.

// Try to:
// 1) Output remaining samples from SpeakerBuffer (from previous fixed-speed session)
// 2) Output pad samples to keep the VoiceBuffer topped-up

// If nNumSamples>0 then these are from previous fixed-speed session.
// - Output these before outputting zero-pad samples.

static ULONG Spkr_SubmitWaveBuffer_FullSpeed(short * pSpeakerBuffer, ULONG nNumSamples) {
    //char szDbg[200];
    nDbgSpkrCnt++;

    if (!s_speakerVoice.bActive)
        return nNumSamples;

    // pSpeakerBuffer can't be NULL, as reset clears g_bFullSpeed, so 1st SpkrUpdate() never calls here
    _ASSERT(pSpeakerBuffer != NULL);

    //

    DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
    SHORT * pDSLockedBuffer0, * pDSLockedBuffer1;
    //bool bBufferError = false;

    DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
    HRESULT hr = s_speakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
    if (FAILED(hr))
        return nNumSamples;

    if (dwByteOffset == (DWORD)-1) {
        // First time in this func (probably after re-init (Spkr_SubmitWaveBuffer()))

        dwByteOffset = dwCurrentPlayCursor + (SPEAKER_BUFSIZE / 8) * 3;    // Ideal: 0.375 is between 0.25 & 0.50 full
        dwByteOffset %= SPEAKER_BUFSIZE;
        //sprintf(szDbg, "[Submit_FS] PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X [REINIT]\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);
    }
    else {
        // Check that our offset isn't between Play & Write positions

        if (dwCurrentWriteCursor > dwCurrentPlayCursor) {
            // |-----PxxxxxW-----|
            if ((dwByteOffset > dwCurrentPlayCursor) && (dwByteOffset < dwCurrentWriteCursor)) {
                //sprintf(szDbg, "[Submit_FS] PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);
                dwByteOffset = dwCurrentWriteCursor;
            }
        }
        else {
            // |xxW----------Pxxx|
            if ((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor)) {
                //sprintf(szDbg, "[Submit_FS] PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);
                dwByteOffset = dwCurrentWriteCursor;
            }
        }
    }

    // Calc bytes remaining to be played
    int nBytesRemaining = dwByteOffset - dwCurrentPlayCursor;
    if (nBytesRemaining < 0)
        nBytesRemaining += SPEAKER_BUFSIZE;
    if ((nBytesRemaining == 0) && (dwCurrentPlayCursor != dwCurrentWriteCursor))
        nBytesRemaining = SPEAKER_BUFSIZE;     // Case when complete buffer is to be played

    //

    UINT nNumPadSamples = 0;

    if (nBytesRemaining < SPEAKER_BUFSIZE / 4) {
        // < 1/4 of play-buffer remaining (need *more* data)
        nNumPadSamples = ((SPEAKER_BUFSIZE / 4) - nBytesRemaining) / sizeof(short);

        if (nNumPadSamples > nNumSamples)
            nNumPadSamples -= nNumSamples;
        else
            nNumPadSamples = 0;

        // NB. If nNumPadSamples>0 then all nNumSamples are to be used
    }

    //

    UINT nBytesFree = SPEAKER_BUFSIZE - nBytesRemaining;   // Calc free buffer space
    ULONG nNumSamplesToUse = nNumSamples + nNumPadSamples;

    if (nNumSamplesToUse * sizeof(short) > nBytesFree)
        nNumSamplesToUse = nBytesFree / sizeof(short);

    //

    if (nNumSamplesToUse >= 128) // Limit the buffer unlock/locking to a minimum
    {
        if (!DSGetLock(s_speakerVoice.lpDSBvoice,
            dwByteOffset, (DWORD)nNumSamplesToUse * sizeof(short),
            &pDSLockedBuffer0, &dwDSLockedBufferSize0,
            &pDSLockedBuffer1, &dwDSLockedBufferSize1))
            return nNumSamples;

        //

        DWORD dwBufferSize0 = 0;
        DWORD dwBufferSize1 = 0;

        if (nNumSamples) {
            //sprintf(szDbg, "[Submit_FS] C=%08X, PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X ***\n", nDbgSpkrCnt, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);

            if (nNumSamples * sizeof(short) <= dwDSLockedBufferSize0) {
                dwBufferSize0 = nNumSamples * sizeof(short);
                dwBufferSize1 = 0;
            }
            else {
                dwBufferSize0 = dwDSLockedBufferSize0;
                dwBufferSize1 = nNumSamples * sizeof(short) - dwDSLockedBufferSize0;

                if (dwBufferSize1 > dwDSLockedBufferSize1)
                    dwBufferSize1 = dwDSLockedBufferSize1;
            }

            memcpy(pDSLockedBuffer0, &pSpeakerBuffer[0], dwBufferSize0);
            nNumSamples = dwBufferSize0 / sizeof(short);

            if (pDSLockedBuffer1 && dwBufferSize1) {
                memcpy(pDSLockedBuffer1, &pSpeakerBuffer[dwDSLockedBufferSize0 / sizeof(short)], dwBufferSize1);
                nNumSamples += dwBufferSize1 / sizeof(short);
            }
        }

        if (nNumPadSamples) {
            //sprintf(szDbg, "[Submit_FS] C=%08X, PC=%08X, WC=%08X, Diff=%08X, Off=%08X, PS=%08X, Data=%04X\n", nDbgSpkrCnt, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumPadSamples, g_nSpeakerData); OutputDebugString(szDbg);

            dwBufferSize0 = dwDSLockedBufferSize0 - dwBufferSize0;
            dwBufferSize1 = dwDSLockedBufferSize1 - dwBufferSize1;

            if (dwBufferSize0) {
                wmemset((wchar_t *)pDSLockedBuffer0, (wchar_t)DCFilter(g_nSpeakerData), dwBufferSize0 / sizeof(wchar_t));
            }

            if (pDSLockedBuffer1) {
                wmemset((wchar_t *)pDSLockedBuffer1, (wchar_t)DCFilter(g_nSpeakerData), dwBufferSize1 / sizeof(wchar_t));
            }
        }

        // Commit sound buffer
        hr = s_speakerVoice.lpDSBvoice->Unlock((void *)pDSLockedBuffer0, dwDSLockedBufferSize0,
            (void *)pDSLockedBuffer1, dwDSLockedBufferSize1);
        if (FAILED(hr))
            return nNumSamples;

        dwByteOffset = (dwByteOffset + (DWORD)nNumSamplesToUse * sizeof(short) * SPEAKER_CHANNEL_COUNT) % SPEAKER_BUFSIZE;
    }

    return nNumSamples;
}

//-----------------------------------------------------------------------------

static ULONG Spkr_SubmitWaveBuffer(short * pSpeakerBuffer, ULONG nNumSamples) {
    //char szDbg[200];
    nDbgSpkrCnt++;

    if (!s_speakerVoice.bActive)
        return nNumSamples;

    if (pSpeakerBuffer == NULL) {
        // Re-init from SpkrReset()
        dwByteOffset = (DWORD)-1;
        nNumSamplesError = 0;

        // Don't call DSZeroVoiceBuffer() - get noise with "VIA AC'97 Enhanced Audio Controller"
        // . I guess SpeakerVoice.Stop() isn't really working and the new zero buffer causes noise corruption when submitted.
        DSZeroVoiceWritableBuffer(&s_speakerVoice, "Spkr", SPEAKER_BUFSIZE);

        return 0;
    }

    //

    DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
    SHORT * pDSLockedBuffer0, * pDSLockedBuffer1;
    bool bBufferError = false;

    DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
    HRESULT hr = s_speakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
    if (FAILED(hr))
        return nNumSamples;

    if (dwByteOffset == (DWORD)-1) {
        // First time in this func (probably after re-init (above))

        dwByteOffset = dwCurrentPlayCursor + (SPEAKER_BUFSIZE / 8) * 3;    // Ideal: 0.375 is between 0.25 & 0.50 full
        dwByteOffset %= SPEAKER_BUFSIZE;
        //sprintf(szDbg, "[Submit]   PC=%08X, WC=%08X, Diff=%08X, Off=%08X [REINIT]\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset); OutputDebugString(szDbg);
    }
    else {
        // Check that our offset isn't between Play & Write positions

        if (dwCurrentWriteCursor > dwCurrentPlayCursor) {
            // |-----PxxxxxW-----|
            if ((dwByteOffset > dwCurrentPlayCursor) && (dwByteOffset < dwCurrentWriteCursor)) {
                double fTicksSecs = (double)GetTickCount() / 1000.0;
                //sprintf(szDbg, "%010.3f: [Submit]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
                //OutputDebugString(szDbg);
                //if (g_fh) fprintf(g_fh, szDbg);

                dwByteOffset = dwCurrentWriteCursor;
                nNumSamplesError = 0;
                bBufferError = true;
            }
        }
        else {
            // |xxW----------Pxxx|
            if ((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor)) {
                double fTicksSecs = (double)GetTickCount() / 1000.0;
                //sprintf(szDbg, "%010.3f: [Submit]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
                //OutputDebugString(szDbg);
                //if (g_fh) fprintf(g_fh, "%s", szDbg);

                dwByteOffset = dwCurrentWriteCursor;
                nNumSamplesError = 0;
                bBufferError = true;
            }
        }
    }

    // Calc bytes remaining to be played
    int nBytesRemaining = dwByteOffset - dwCurrentPlayCursor;
    if (nBytesRemaining < 0)
        nBytesRemaining += SPEAKER_BUFSIZE;
    if ((nBytesRemaining == 0) && (dwCurrentPlayCursor != dwCurrentWriteCursor))
        nBytesRemaining = SPEAKER_BUFSIZE;     // Case when complete buffer is to be played

    // Calc correction factor so that play-buffer doesn't under/overflow
    const int nErrorInc = SoundCore_GetErrorInc();
    if (nBytesRemaining < SPEAKER_BUFSIZE / 4)
        nNumSamplesError += nErrorInc;              // < 1/4 of play-buffer remaining (need *more* data)
    else if (nBytesRemaining > SPEAKER_BUFSIZE / 2)
        nNumSamplesError -= nErrorInc;              // > 1/2 of play-buffer remaining (need *less* data)
    else
        nNumSamplesError = 0;                       // Acceptable amount of data in buffer

    const int nErrorMax = SoundCore_GetErrorMax();              // Cap feedback to +/-nMaxError units
    if (nNumSamplesError < -nErrorMax) nNumSamplesError = -nErrorMax;
    if (nNumSamplesError > nErrorMax) nNumSamplesError = nErrorMax;
    g_nCpuCyclesFeedback = (int)((double)nNumSamplesError * g_fClksPerSpkrSample);

    //

    UINT nBytesFree = SPEAKER_BUFSIZE - nBytesRemaining;   // Calc free buffer space
    ULONG nNumSamplesToUse = nNumSamples;

    if (nNumSamplesToUse * sizeof(short) > nBytesFree)
        nNumSamplesToUse = nBytesFree / sizeof(short);

    if (bBufferError)
        pSpeakerBuffer = &pSpeakerBuffer[nNumSamples - nNumSamplesToUse];

    //

    if (nNumSamplesToUse) {
        //sprintf(szDbg, "[Submit]    C=%08X, PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X +++\n", nDbgSpkrCnt, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamplesToUse); OutputDebugString(szDbg);

        if (!DSGetLock(s_speakerVoice.lpDSBvoice,
            dwByteOffset, (DWORD)nNumSamplesToUse * sizeof(short),
            &pDSLockedBuffer0, &dwDSLockedBufferSize0,
            &pDSLockedBuffer1, &dwDSLockedBufferSize1))
            return nNumSamples;

        memcpy(pDSLockedBuffer0, &pSpeakerBuffer[0], dwDSLockedBufferSize0);

        if (pDSLockedBuffer1) {
            memcpy(pDSLockedBuffer1, &pSpeakerBuffer[dwDSLockedBufferSize0 / sizeof(short)], dwDSLockedBufferSize1);
        }

        // Commit sound buffer
        hr = s_speakerVoice.lpDSBvoice->Unlock((void *)pDSLockedBuffer0, dwDSLockedBufferSize0,
            (void *)pDSLockedBuffer1, dwDSLockedBufferSize1);
        if (FAILED(hr))
            return nNumSamples;

        dwByteOffset = (dwByteOffset + (DWORD)nNumSamplesToUse * sizeof(short) * SPEAKER_CHANNEL_COUNT) % SPEAKER_BUFSIZE;
    }

    return bBufferError ? nNumSamples : nNumSamplesToUse;
}

//-----------------------------------------------------------------------------

void Spkr_Mute() {
    if (s_speakerVoice.bActive && !s_speakerVoice.bMute) {
        s_speakerVoice.lpDSBvoice->SetVolume(DSBVOLUME_MIN);
        s_speakerVoice.bMute = true;
    }
}

void Spkr_Demute() {
    if (s_speakerVoice.bActive && s_speakerVoice.bMute) {
        s_speakerVoice.lpDSBvoice->SetVolume(s_speakerVoice.nVolume);
        s_speakerVoice.bMute = false;
    }
}

//-----------------------------------------------------------------------------

static bool g_bSpkrRecentlyActive = false;

static void Spkr_SetActive(bool bActive) {
    if (!s_speakerVoice.bActive)
        return;

    if (bActive) {
        // Called by SpkrToggle() or SpkrReset()
        g_bSpkrRecentlyActive = true;
        s_speakerVoice.bRecentlyActive = true;
    }
    else {
        // Called by SpkrUpdate() after 0.2s of speaker inactivity
        g_bSpkrRecentlyActive = false;
        s_speakerVoice.bRecentlyActive = false;
        g_bQuieterSpeaker = 0;  // undo any muting (for 8 bit DAC)
    }
}

bool Spkr_IsActive() {
    return g_bSpkrRecentlyActive;
}

//-----------------------------------------------------------------------------

DWORD SpkrGetVolume() {
    return s_speakerVoice.dwUserVolume;
}

void SpkrSetVolume(DWORD dwVolume, DWORD dwVolumeMax) {
    s_speakerVoice.dwUserVolume = dwVolume;

    s_speakerVoice.nVolume = NewVolume(dwVolume, dwVolumeMax);

    if (s_speakerVoice.bActive)
        s_speakerVoice.lpDSBvoice->SetVolume(s_speakerVoice.nVolume);
}

//=============================================================================

bool Spkr_DSInit() {
    //
    // Create single Apple speaker voice
    //

    if (!g_bDSAvailable)
        return false;

    s_speakerVoice.bIsSpeaker = true;

    HRESULT hr = DSGetSoundBuffer(&s_speakerVoice, DSBCAPS_CTRLVOLUME, SPEAKER_BUFSIZE, SPKR_SAMPLE_RATE, 1);
    if (FAILED(hr)) {
        if (g_fh) fprintf(g_fh, "Spkr: DSGetSoundBuffer failed (%08X)\n", hr);
        return false;
    }

    if (!DSZeroVoiceBuffer(&s_speakerVoice, "Spkr", SPEAKER_BUFSIZE))
        return false;

    s_speakerVoice.bActive = true;

    // Volume might've been setup from value in Registry
    if (!s_speakerVoice.nVolume)
        s_speakerVoice.nVolume = DSBVOLUME_MAX;

    s_speakerVoice.lpDSBvoice->SetVolume(s_speakerVoice.nVolume);

    //

    DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
    hr = s_speakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
    if (SUCCEEDED(hr) && (dwCurrentPlayCursor == dwCurrentWriteCursor)) {
        // KLUDGE: For my WinXP PC with "VIA AC'97 Enhanced Audio Controller"
        // . Not required for my Win98SE/WinXP PC with PCI "Soundblaster Live!"
        Sleep(200);

        hr = s_speakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
        char szDbg[100];
        sprintf(szDbg, "[DSInit] PC=%08X, WC=%08X, Diff=%08X\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor); OutputDebugString(szDbg);
    }

    return true;
}

void Spkr_DSUninit() {
    if (s_speakerVoice.lpDSBvoice && s_speakerVoice.bActive) {
        s_speakerVoice.lpDSBvoice->Stop();
        s_speakerVoice.bActive = false;
    }

    DSReleaseSoundBuffer(&s_speakerVoice);
}

//=============================================================================

#define SS_YAML_KEY_LASTCYCLE "Last Cycle"

static std::string SpkrGetSnapshotStructName(void) {
    static const std::string name("Speaker");
    return name;
}

void SpkrSaveSnapshot(YamlSaveHelper & yamlSaveHelper) {
    YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SpkrGetSnapshotStructName().c_str());
    yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_LASTCYCLE, s_nSpkrLastCycle);
}

void SpkrLoadSnapshot(YamlLoadHelper & yamlLoadHelper) {
    if (!yamlLoadHelper.GetSubMap(SpkrGetSnapshotStructName()))
        return;

    s_nSpkrLastCycle = yamlLoadHelper.LoadUint64(SS_YAML_KEY_LASTCYCLE);

    yamlLoadHelper.PopMap();
}