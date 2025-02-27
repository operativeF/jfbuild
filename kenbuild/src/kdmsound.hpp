// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __kdmsound_h__
#define __kdmsound_h__

#include <string>

#ifdef KDMSOUND_INTERNAL
int initkdm(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample);
void uninitkdm();
void preparekdmsndbuf(unsigned char *sndoffsplc, int sndbufsiz);

// Implemented in the per-platform interface.
int lockkdm();
void unlockkdm();
#endif

// Implemented in the per-platform interface.
void initsb(char dadigistat, char damusistat, int dasamplerate, char danumspeakers, char dabytespersample, char daintspersec, char daquality);
void uninitsb();
void refreshaudio();

void setears(int daposx, int daposy, int daxvect, int dayvect);
void wsayfollow(const std::string& dafilename, int dafreq, int davol, int* daxplc, int* dayplc, char followstat);
void wsay(const std::string& dafilename, int dafreq, int volume1, int volume2);
void loadwaves(const char *wavename);
int loadsong(const std::string& songname);
void musicon();
void musicoff();

#endif
