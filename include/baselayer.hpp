// Base services interface declaration
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __baselayer_h__
#define __baselayer_h__

#include <array>

#ifdef __cplusplus
extern "C" {
#endif

extern int _buildargc;
extern const char **_buildargv;

extern char quitevent, appactive;

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
extern int xres;
extern int yres;
extern int bpp;
extern int fullscreen;
extern int bytesperline;
extern int imageSize;
extern char offscreenrendering;
extern intptr_t frameplace;

extern void (*baselayer_videomodewillchange)();
extern void (*baselayer_videomodedidchange)();

extern int inputdevices;

// keys
inline constexpr auto KEYFIFOSIZ{64};
extern std::array<char, 256> keystatus;
extern std::array<int, KEYFIFOSIZ> keyfifo;
extern std::array<unsigned char, KEYFIFOSIZ> keyasciififo;
extern int keyfifoplc;
extern int keyfifoend;
extern int keyasciififoplc;
extern int keyasciififoend;

// mouse
extern int mousex;
extern int mousey;
extern int mouseb;

// joystick
extern std::array<int, 8> joyaxis;
extern int joyb;
extern char joynumaxes;
extern char joynumbuttons;


void initputs(const char *);
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
int wm_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice);

int wm_idle(void *);
void wm_setapptitle(const char *name);
void wm_setwindowtitle(const char *name);

// baselayer.c
void makeasmwriteable();

#ifdef __cplusplus
}
#endif

#endif // __baselayer_h__

