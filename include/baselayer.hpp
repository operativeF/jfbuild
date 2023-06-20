// Base services interface declaration
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __baselayer_h__
#define __baselayer_h__

#include "compat.hpp"

#include <array>
#include <string>

inline int _buildargc{0};
inline const char **_buildargv{nullptr};

inline char quitevent{0};
inline bool appactive{true};

enum {
    STARTWIN_CANCEL = 0,
    STARTWIN_RUN = 1,
};
struct startwin_settings;

// NOTE: these are implemented in game-land so they may be overridden in game specific ways
extern int app_main(int argc, char const * const argv[]);
extern int startwin_open();
extern int startwin_close();
extern int startwin_puts(const char *);
extern int startwin_settitle(const char *);
extern int startwin_idle(void *);
extern int startwin_run(struct startwin_settings *);

// video
inline int xres{-1};
inline int yres{-1};
inline int bpp{0};
inline int fullscreen{0};
inline int bytesperline{0};
inline int imageSize{0};
inline char offscreenrendering{0};
inline intptr_t frameplace{0};

extern void (*baselayer_videomodewillchange)();
extern void (*baselayer_videomodedidchange)();

inline int inputdevices{0};

// keys
inline constexpr auto KEYFIFOSIZ{64};
inline std::array<char, 256> keystatus{};
inline std::array<int, KEYFIFOSIZ> keyfifo{};
inline std::array<unsigned char, KEYFIFOSIZ> keyasciififo{};

inline int keyfifoplc{0};
inline int keyfifoend{0};
inline int keyasciififoplc{0};
inline int keyasciififoend{0};

// mouse
inline int mousex{0};
inline int mousey{0};
inline int mouseb{0};

// joystick
inline std::array<int, 8> joyaxis{};
inline int joyb{0};
inline char joynumaxes{0};
inline char joynumbuttons{0};


void initputs(const char* buf);
void debugprintf(const char *,...) PRINTF_FORMAT(1, 2);

int handleevents();

int initinput();
void uninitinput();
void releaseallbuttons();
const char *getkeyname(int num);
const char *getjoyname(int what, int num);	// what: 0=axis, 1=button, 2=hat

unsigned char bgetchar();
int bkbhit();
void bflushchars();
int bgetkey();  // >0 = press, <0 = release
int bkeyhit();
void bflushkeys();

int initmouse();
void uninitmouse();
void grabmouse(int a);
void readmousexy(int *x, int *y);
void readmousebstatus(int *b);

int inittimer(int, void(*)());
void uninittimer();
void sampletimer();
unsigned int getticks();
unsigned int getusecticks();
int gettimerfreq();

int checkvideomode(int *x, int *y, int c, int fs, int forced);
int setvideomode(int x, int y, int c, int fs);
void getvalidmodes();
void resetvideomode();

void showframe();

// FIXME: Parameters are unused.
int setpalette(int start, int num, const unsigned char* dapal);
int setgamma(float gamma);

int wm_msgbox(const char *name, const char *fmt, ...) PRINTF_FORMAT(2, 3);
int wm_ynbox(const char *name, const char *fmt, ...) PRINTF_FORMAT(2, 3);

// initialdir - the initial directory
// initialfile - the initial filename
// type - the file extension to choose (e.g. "map")
// foropen - boolean true, or false if for saving
// choice - the file chosen by the user to be free()'d when done
// Returns -1 if not supported, 0 if cancelled, 1 if accepted
int wm_filechooser(const std::string& initialdir, const char *initialfile, const char *type, int foropen, char **choice);

int wm_idle(void *);
void wm_setapptitle(const char *name);
void wm_setwindowtitle(const char *name);

// baselayer.c
void makeasmwriteable();

#endif // __baselayer_h__

