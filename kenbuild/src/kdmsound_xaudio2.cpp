// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define _WIN32_IE _WIN32_IE_WIN7

#include <windows.h>
#include <xaudio2.h>

#define KDMSOUND_INTERNAL
#include "kdmsound.hpp"

#include "build.hpp"

#include <algorithm>

static IXAudio2 *xaudio;
static IXAudio2MasteringVoice *mastervoice;
static IXAudio2SourceVoice *sourcevoice;

static unsigned char *audiobuffer;
static int buffersize;
static const int NUMBUFFERS = 2;

enum { EVENT_FILL, EVENT_EXIT, MAX_EVENTS };
static HANDLE bufferevents[MAX_EVENTS], bufferthread;
static char bufferfull[NUMBUFFERS];
static CRITICAL_SECTION buffercritsec;
static BOOL buffercritsecinited;

static DWORD WINAPI bufferthreadproc(LPVOID lpParam);

static struct sourcevoicecallback : public IXAudio2VoiceCallback {
    void OnVoiceProcessingPassStart(UINT32) {}
    void OnVoiceProcessingPassEnd() {}
    void OnStreamEnd() {}
    void OnBufferStart(void*) {}
    void OnBufferEnd(void *p) { *(char*)p = 0; SetEvent(bufferevents[EVENT_FILL]); }
    void OnLoopEnd(void*) {}
    void OnVoiceError(void*, HRESULT) {}
} sourcevoicecallback;

void initsb(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample, char daintspersec, char daquality)
{
    (void)daintspersec; (void)daquality;

    HRESULT hr;

    if (xaudio) return;

    if ((dadigistat == 0) && (damusistat != 1))
        return;

#define INITSB_CHECK(cond, ...) if (!(cond)) { \
    buildprintf("initsb(): " __VA_ARGS__); \
    uninitsb(); \
    return; \
}

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(!SUCCEEDED(hr))
        buildprintf("failed to initialise COM ({:08x})\n", hr);
    INITSB_CHECK(SUCCEEDED(hr), "failed to initialise COM (%08x)\n", hr);

    hr = XAudio2Create(&xaudio, 0, XAUDIO2_USE_DEFAULT_PROCESSOR);
    if(!SUCCEEDED(hr))
        buildprintf("failed to create XAudio2 ({:08x})\n", hr);

    hr = xaudio->CreateMasteringVoice(&mastervoice);
    if(!SUCCEEDED(hr))
        buildprintf("failed to create mastering voice ({:08x})\n", hr);

    WAVEFORMATEX sourcefmt = {0};
    sourcefmt.wFormatTag = WAVE_FORMAT_PCM;
    sourcefmt.nChannels = std::max(1, std::min(2, static_cast<int>(danumspeakers))); // FIXME: Don't use char for indexes.
    sourcefmt.nSamplesPerSec = dasamplerate;
    sourcefmt.wBitsPerSample = dabytespersample * 8;
    sourcefmt.nBlockAlign = sourcefmt.nChannels * dabytespersample;
    sourcefmt.nAvgBytesPerSec = sourcefmt.nBlockAlign * sourcefmt.nSamplesPerSec;
    hr = xaudio->CreateSourceVoice(&sourcevoice, &sourcefmt,
            XAUDIO2_VOICE_NOPITCH, XAUDIO2_DEFAULT_FREQ_RATIO, &sourcevoicecallback);
    INITSB_CHECK(SUCCEEDED(hr), "failed to create source voice (%08x)\n", hr);

    INITSB_CHECK(initkdm(dadigistat, damusistat, sourcefmt.nSamplesPerSec,
                    sourcefmt.nChannels, dabytespersample) == 0,
        "failed to initialise KDM\n");

    loadwaves("waves.kwv");

    buffersize = 4*(((sourcefmt.nAvgBytesPerSec/120)+1)&~1);
    audiobuffer = (unsigned char *)std::malloc(buffersize * NUMBUFFERS);
    INITSB_CHECK(audiobuffer, "failed to allocate audio buffer\n");

    bufferevents[EVENT_FILL] = CreateEvent(nullptr, FALSE, TRUE, nullptr);    // Signalled.
    bufferevents[EVENT_EXIT] = CreateEvent(nullptr, FALSE, FALSE, nullptr);   // Unsignalled.
    INITSB_CHECK(bufferevents[EVENT_FILL] && bufferevents[EVENT_EXIT],
        "failed to create buffer events\n");

    InitializeCriticalSection(&buffercritsec);
    buffercritsecinited = TRUE;

    bufferthread = CreateThread(nullptr, 0, bufferthreadproc, nullptr, 0, nullptr);
    INITSB_CHECK(bufferthread, "failed to create buffer thread\n");

    SetThreadPriority(bufferthread, THREAD_PRIORITY_TIME_CRITICAL);

    hr = sourcevoice->Start(0);
    INITSB_CHECK(SUCCEEDED(hr), "failed to start source voice (%08x)\n", hr);
}

void uninitsb()
{
    if (bufferthread) {
        SetEvent(bufferevents[EVENT_EXIT]);
        WaitForSingleObject(bufferthread, INFINITE);
        CloseHandle(bufferthread);
        bufferthread = nullptr;
    }
    if (buffercritsecinited) {
        DeleteCriticalSection(&buffercritsec);
        buffercritsecinited = FALSE;
    }
    if (bufferevents[EVENT_EXIT]) {
        CloseHandle(bufferevents[EVENT_EXIT]);
        bufferevents[EVENT_EXIT] = nullptr;
    }
    if (bufferevents[EVENT_FILL]) {
        CloseHandle(bufferevents[EVENT_FILL]);
        bufferevents[EVENT_FILL] = nullptr;
    }

    if (audiobuffer) {
        std::free(audiobuffer);
        audiobuffer = nullptr;
    }

    uninitkdm();

    if (sourcevoice) {
        sourcevoice->DestroyVoice();
        sourcevoice = nullptr;
    }
    if (mastervoice) {
        mastervoice->DestroyVoice();
        mastervoice = nullptr;
    }
    if (xaudio) {
        xaudio->Release();
        xaudio = nullptr;
    }
}

void refreshaudio()
{
}

int lockkdm()
{
    if (!buffercritsecinited) return -1;
    EnterCriticalSection(&buffercritsec);
    return 0;
}

void unlockkdm()
{
    if (!buffercritsecinited) return;
    LeaveCriticalSection(&buffercritsec);
}

static DWORD WINAPI bufferthreadproc(LPVOID lpParam)
{
    (void)lpParam;

    while (1) {
        switch (WaitForMultipleObjects(2, bufferevents, FALSE, INFINITE)) {
            case WAIT_OBJECT_0 + EVENT_FILL: {
                XAUDIO2_BUFFER buffer = {0};
                HRESULT hr;

                if (lockkdm()) break;

                for (int i = NUMBUFFERS-1; i >= 0; i--) {
                    if (bufferfull[i]) continue;

                    preparekdmsndbuf(audiobuffer + buffersize * i, buffersize);
                    buffer.pAudioData = audiobuffer + buffersize * i;
                    buffer.AudioBytes = buffersize;
                    buffer.pContext = (void *)&bufferfull[i];

                    if (FAILED(hr = sourcevoice->SubmitSourceBuffer(&buffer))) {
                        buildprintf("bufferthreadproc(): failed to submit source buffer ({:08x})\n", hr);
                        break;
                    }

                    bufferfull[i] = 1;
                }

                unlockkdm();
                break;
            }
            case WAIT_OBJECT_0 + EVENT_EXIT:
                return 0;
            case WAIT_TIMEOUT:
               break;
            default:
                return -1;
        }
    }

    return 0;
}
