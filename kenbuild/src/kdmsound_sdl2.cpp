// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#if defined __APPLE__
# include <SDL2/SDL.h>
#else
# include <SDL2/SDL.h>
#endif

#define KDMSOUND_INTERNAL
#include "kdmsound.hpp"

#if (SDL_MAJOR_VERSION != 2)
#  error This must be built with SDL2
#endif

namespace {

SDL_AudioDeviceID dev;

void preparesndbuf(void *udata, Uint8 *sndoffsplc, int sndbufsiz);

} // namespace

void initsb(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample, char daintspersec, char daquality)
{
    SDL_AudioSpec want, have;

    (void)daintspersec; (void)daquality;

    if (dev) return;

    if ((dadigistat == 0) && (damusistat != 1))
        return;

    SDL_memset(&want, 0, sizeof(want));
    want.freq = dasamplerate;
    want.format = dabytespersample == 1 ? AUDIO_U8 : AUDIO_S16SYS;
    want.channels = std::max(1, std::min(2, danumspeakers)); // FIXME: Don't use char for counts
    want.samples = (((want.freq/120)+1)&~1);
    want.callback = preparesndbuf;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_Log("Failed to initialise SDL audio subsystem: %s", SDL_GetError());
        return;
    }

    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
            SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (dev == 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
        return;
    }

    if (initkdm(dadigistat, damusistat, have.freq, have.channels, SDL_AUDIO_BITSIZE(have.format)>>3)) {
        SDL_CloseAudioDevice(dev);
        dev = 0;
        SDL_Log("Failed to initialise KDM");
        return;
    }

    loadwaves("waves.kwv");

    SDL_PauseAudioDevice(dev, 0);
}

void uninitsb()
{
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;

    uninitkdm();
}

int lockkdm()
{
    if (!dev) return -1;
    SDL_LockAudioDevice(dev);
    return 0;
}

void unlockkdm()
{
    if (!dev) return;
    SDL_UnlockAudioDevice(dev);
}

void refreshaudio()
{
}

namespace {

void preparesndbuf(void *udata, Uint8 *sndoffsplc, int sndbufsiz)
{
    (void)udata;

    preparekdmsndbuf(sndoffsplc, sndbufsiz);
}

} // namespace
