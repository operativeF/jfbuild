// Windows interface layer
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)
//
// This is all very ugly.

#ifndef _WIN32
#error winlayer.c is for Windows only.
#endif

#include "build.hpp"

#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <commdlg.h>

#if USE_OPENGL
#include "glbuild_priv.hpp"
#include "wglext.hpp"
#endif

#include "baselayer_priv.hpp"
#include "engine_priv.hpp"
#include "winlayer.hpp"
#include "pragmas.hpp"
#include "a.hpp"
#include "osd.hpp"
#include "string_utils.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdarg>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>

static BOOL CheckWinVersion();
static LRESULT CALLBACK WndProcCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LPTSTR GetWindowsErrorMsg(DWORD code);
static void ShowErrorBox(const char *m);
static BOOL RegisterWindowClass();
static BOOL CreateAppWindow(int width, int height, int bitspp, bool fs, int refresh);
static void DestroyAppWindow();
static void UpdateAppWindowTitle();
static int SetupOpenGL(int width, int height, unsigned char bitspp);
static void UninitOpenGL();
static const char * getwindowserrorstr(DWORD code);

namespace {

char *argvbuf = nullptr;

// Windows crud
HINSTANCE hInstance{nullptr};
HWND hWindow{nullptr};
HDC hDCWindow{nullptr};
#define WINDOW_CLASS "buildapp"
BOOL window_class_registered{FALSE};
HANDLE instanceflag{nullptr};

int backgroundidle{0};
char apptitle[256] = "Build Engine";
char wintitle[256] = "";

WORD sysgamma[3][256];

#if USE_OPENGL
// OpenGL stuff
HGLRC hGLRC{nullptr};
HANDLE hGLDLL;
glbuild8bit gl8bit;
std::vector<unsigned char> frame;

HWND hGLWindow{nullptr};
HWND dummyhGLwindow{nullptr};
HDC hDCGLWindow{nullptr};

using wglExtStringARB_t = const char* (WINAPI *)(HDC);
using wglChoosePixelFmtARB_t = BOOL (WINAPI *)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
using wglCreateCtxAttribsARB_t = HGLRC (WINAPI *)(HDC, HGLRC, const int *);
using wglSwapIntervalExt_t = BOOL (WINAPI *)(int);
using wglGetSwapIntervalExt_t = int (WINAPI *)();

using wglCreateCtx_t = HGLRC (WINAPI *)(HDC);
using wglDeleteCtx_t = BOOL (WINAPI *)(HGLRC);
using wglGetProcAddr_t = PROC (WINAPI *)(LPCSTR);
using wglMakeCurrent_t = BOOL (WINAPI *)(HDC,HGLRC);
using wglSwapBuffer_t = BOOL (WINAPI *)(HDC);

struct winlayer_glfuncs {
	HGLRC (WINAPI * wglCreateContext)(HDC);
	BOOL (WINAPI * wglDeleteContext)(HGLRC);
	PROC (WINAPI * wglGetProcAddress)(LPCSTR);
	BOOL (WINAPI * wglMakeCurrent)(HDC,HGLRC);
	BOOL (WINAPI * wglSwapBuffers)(HDC);

	const char * (WINAPI * wglGetExtensionsStringARB)(HDC hdc);
	BOOL (WINAPI * wglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
	HGLRC (WINAPI * wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
	BOOL (WINAPI * wglSwapIntervalEXT)(int interval);
	int (WINAPI * wglGetSwapIntervalEXT)();

	int have_ARB_create_context_profile;
	int have_EXT_multisample;
	int have_EXT_swap_control;
	int have_EXT_swap_control_tear;
} wglfunc;

#endif

void fetchkeynames();
void updatemouse();
void updatejoystick();
void UninitDIB();
int SetupDIB(int width, int height);

void shutdownvideo();

// video
int desktopxdim=0,desktopydim=0,desktopbpp=0;
bool desktopmodeset{false};
int windowposx, windowposy;
unsigned modeschecked=0;
unsigned maxrefreshfreq=60;

// input and events
std::array<unsigned int, 2> mousewheel = { 0, 0 };
constexpr auto MouseWheelFakePressTime{100};	// getticks() is a 1000Hz timer, and the button press is faked for 100ms

char taskswitching=1;

char keynames[256][24];

constexpr std::array<int, 256> wscantable = {
/*         x0    x1    x2    x3    x4    x5    x6    x7    x8    x9    xA    xB    xC    xD    xE    xF */
/* 0y */ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
/* 1y */ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
/* 2y */ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
/* 3y */ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
/* 4y */ 0x40, 0x41, 0x42, 0x43, 0x44, 0x59, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
/* 5y */ 0x50, 0x51, 0x52, 0x53, 0,    0,    0,    0x57, 0x58, 0,    0,    0,    0,    0,    0,    0,
/* 6y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 7y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 8y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 9y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Ay */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* By */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Cy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Dy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Ey */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Fy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};

constexpr std::array<int, 256> wxscantable = {
/*         x0    x1    x2    x3    x4    x5    x6    x7    x8    x9    xA    xB    xC    xD    xE    xF */
/* 0y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 1y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x9c, 0x9d, 0,    0,
/* 2y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 3y */ 0,    0,    0,    0,    0,    0xb5, 0,    0,    0xb8, 0,    0,    0,    0,    0xb8, 0,    0,
/* 4y */ 0,    0,    0,    0,    0,    0x45, 0,    0xc7, 0xc8, 0xc9, 0,    0xcb, 0,    0xcd, 0,    0xcf,
/* 5y */ 0xd0, 0xd1, 0xd2, 0xd3, 0,    0,    0,    0,    0,    0,    0,    0x5b, 0x5c, 0x5d, 0,    0,
/* 6y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 7y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 8y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* 9y */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x9d, 0,    0,
/* Ay */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* By */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Cy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Dy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Ey */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/* Fy */ 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};

} // namespace

//-------------------------------------------------------------------------------------------------
//  MAIN CRAP
//=================================================================================================


//
// win_gethwnd() -- gets the window handle
//
intptr_t win_gethwnd()
{
	return (intptr_t)hWindow;
}


//
// win_gethinstance() -- gets the application instance
//
intptr_t win_gethinstance()
{
	return (intptr_t)hInstance;
}


//
// win_allowtaskswitching() -- captures/releases alt+tab hotkeys
//
void win_allowtaskswitching(int onf)
{
	if (onf == taskswitching) {
		return;
	}

	if (onf) {
		::UnregisterHotKey(nullptr, 0);
		::UnregisterHotKey(nullptr, 1);
	} 
	else {
		::RegisterHotKey(nullptr , 0, MOD_ALT, VK_TAB);
		::RegisterHotKey(nullptr , 1, MOD_ALT|MOD_SHIFT, VK_TAB);
	}

	taskswitching = onf;
}


//
// win_allowbackgroundidle() -- allow the application to idle in the background
//
void win_allowbackgroundidle(int onf)
{
	backgroundidle = onf;
}


//
// win_checkinstance() -- looks for another instance of a Build app
//
int win_checkinstance()
{
	if (!instanceflag) {
		return 0;
	}

	return (::WaitForSingleObject(instanceflag,0) == WAIT_TIMEOUT);
}


void win_setmaxrefreshfreq(unsigned frequency)
{
	maxrefreshfreq = frequency;
}

unsigned win_getmaxrefreshfreq()
{
	return maxrefreshfreq;
}


//
// wm_msgbox/wm_ynbox() -- window-manager-provided message boxes
//
int wm_msgbox(const char *name, const char *fmt, ...) {
	std::array<char, 1000> buf;
	va_list va;

	va_start(va,fmt);
	vsprintf(buf.data(),fmt,va);
	va_end(va);

	::MessageBox(hWindow, buf.data(), name, MB_OK | MB_TASKMODAL);

	return 0;
}

int wm_ynbox(const char *name, const char *fmt, ...) {
	std::array<char, 1000> buf;
	va_list va;

	va_start(va,fmt);
	vsprintf(buf.data(),fmt,va);
	va_end(va);

	const int r = ::MessageBox((HWND)win_gethwnd(), buf.data(), name, MB_YESNO | MB_TASKMODAL);

	if (r==IDYES) {
		return 1;
	}

	return 0;
}

//
// wm_filechooser() -- display a file selector dialogue box
//
int wm_filechooser(const std::string& initialdir, const char *initialfile, const char *type, int foropen, std::string& choice)
{
	std::array<char, 100> filter{};
	char* filterp = filter.data();
	char filename[BMAX_PATH + 1] = "";

	choice.clear();

	if (!foropen && initialfile) {
		std::strcpy(filename, initialfile);
	}

	// ext Files\0*.ext\0\0
	fmt::format_to(filterp, "{} Files", type);
	filterp += std::strlen(filterp) + 1;
	fmt::format_to(filterp, "*.{}", type);

	OPENFILENAME ofn;

	::ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWindow;
	ofn.lpstrFilter = filter.data();
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename);
	ofn.lpstrInitialDir = initialdir.c_str();
	ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = type;

	if (foropen ? ::GetOpenFileName(&ofn) : ::GetSaveFileName(&ofn)) {
		choice = filename;
		return 1;
	}
	else {
		return 0;
	}
}

//
// wm_setapptitle() -- changes the application title
//
void wm_setapptitle(const char *name)
{
	if (name) {
		std::strncpy(apptitle, name, sizeof(apptitle)-1);
		apptitle[ sizeof(apptitle)-1 ] = 0;
	}

	UpdateAppWindowTitle();
	startwin_settitle(apptitle);
}

//
// wm_setwindowtitle() -- changes the rendering window title
//
void wm_setwindowtitle(const std::string& name)
{
	name.copy(wintitle, sizeof(wintitle));

	UpdateAppWindowTitle();
}

namespace {

//
// SignalHandler() -- called when we've sprung a leak
//
void SignalHandler(int signum)
{
	switch (signum) {
		case SIGSEGV:
			buildputs("Fatal Signal caught: SIGSEGV. Bailing out.\n");
			uninitsystem();
			if (stdout) std::fclose(stdout);
			break;
		default:
			break;
	}
}

} // namespace

//
// WinMain() -- main Windows entry point
//
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, const LPSTR lpCmdLine, int nCmdShow)
{
	(void)lpCmdLine;
	(void)nCmdShow;

	hInstance = hInst;

	if (CheckWinVersion() || hPrevInst) {
		::MessageBox(nullptr, "This application must be run under Windows Vista or newer.",
			apptitle, MB_OK|MB_ICONSTOP);
		return -1;
	}

	HDC hdc = GetDC(nullptr);
	int r = ::GetDeviceCaps(hdc, BITSPIXEL);
	::ReleaseDC(nullptr, hdc);

	if (r <= 8) {
		::MessageBox(nullptr, "This application requires a desktop colour depth of 65536-colours or more.",
			apptitle, MB_OK|MB_ICONSTOP);
		return -1;
	}

	// carve up the commandline into more recognizable pieces
	argvbuf = strdup(::GetCommandLine());
	_buildargc = 0;

	if (argvbuf) {
		bool quoted{false};
		bool instring{false};
		bool swallownext{false};

		char *p;
		char *wp; int i;
		for (p=wp=argvbuf; *p; p++) {
			if (*p == ' ') {
				if (instring && !quoted) {
					// end of a string
					*(wp++) = 0;
					instring = false;
				} else if (instring) {
					*(wp++) = *p;
				}
			} else if (*p == '"' && !swallownext) {
				if (instring && quoted) {
					// end of a string
					if (p[1] == ' ') {
						*(wp++) = 0;
						instring = false;
						quoted = false;
					} else {
						quoted = false;
					}
				} else if (instring && !quoted) {
					quoted = true;
				} else if (!instring) {
					instring = true;
					quoted = true;
					_buildargc++;
				}
			} else if (*p == '\\' && p[1] == '"' && !swallownext) {
				swallownext = true;
			} else {
				if (!instring) _buildargc++;
				instring = true;
				*(wp++) = *p;
				swallownext = false;
			}
		}
		*wp = 0;

		_buildargv = static_cast<const char**>(std::malloc(sizeof(char*)*_buildargc));
		wp = argvbuf;
		for (i=0; i<_buildargc; i++,wp++) {
			_buildargv[i] = wp;
			while (*wp) wp++;
		}
	}

	// pipe standard outputs to files
	char *argp;

	if ((argp = std::getenv("BUILD_LOGSTDOUT")) != nullptr) {
		if (IsSameAsNoCase(argp, "TRUE")) {
			std::FILE* fp = std::freopen("stdout.txt", "w", stdout);

			if (!fp) {
				fp = std::fopen("stdout.txt", "w");
			}

			if (fp) {
				setvbuf(fp, nullptr, _IONBF, 0);
			}

			*stdout = *fp;
			*stderr = *fp;
		}
	}

	// install signal handlers
	signal(SIGSEGV, SignalHandler);

	if (::RegisterWindowClass()) {
		return -1;
	}

	atexit(uninitsystem);

	instanceflag = ::CreateSemaphore(nullptr, 1, 1, WINDOW_CLASS);

	startwin_open();
	baselayer_init();
	r = app_main(_buildargc, _buildargv);

	std::fclose(stdout);

	startwin_close();
	if (instanceflag) ::CloseHandle(instanceflag);

	if (argvbuf) std::free(argvbuf);

	return r;
}

namespace {

int set_maxrefreshfreq(const osdfuncparm_t *parm)
{
	if (parm->parms.size() == 0) {
		if (maxrefreshfreq == 0) {
			buildputs("maxrefreshfreq = No maximum\n");
		}
		else {
			buildprintf("maxrefreshfreq = {} Hz\n",maxrefreshfreq);
		}

		return OSDCMD_OK;
	}

	if (parm->parms.size() != 1) {
		return OSDCMD_SHOWHELP;
	}

	const std::string_view parmv{parm->parms[0]};
	int freq{0};
	auto [ptr, ec] = std::from_chars(parmv.data(), parmv.data() + parmv.size(), freq);

	if (freq < 0 || (ec != std::errc{})) {
		return OSDCMD_SHOWHELP;
	}

	maxrefreshfreq = (unsigned)freq;
	modeschecked = 0;

	return OSDCMD_OK;
}

#if USE_OPENGL
int set_glswapinterval(const osdfuncparm_t *parm)
{
	if (!wglfunc.wglSwapIntervalEXT || glunavailable) {
		buildputs("glswapinterval is not adjustable\n");
		return OSDCMD_OK;
	}

	if (parm->parms.size() == 0) {
		if (glswapinterval == -1) buildprintf("glswapinterval is {} (adaptive vsync)\n", glswapinterval);
		else buildprintf("glswapinterval is {}\n", glswapinterval);
		return OSDCMD_OK;
	}

	if (parm->parms.size() != 1) {
		return OSDCMD_SHOWHELP;
	}

	const std::string_view parmv{parm->parms[0]};
	int interval{0};
	auto [ptr, ec] = std::from_chars(parmv.data(), parmv.data() + parmv.size(), interval);

	if (interval < -1 || interval > 2 || (ec != std::errc{})) {
		return OSDCMD_SHOWHELP;
	}

	if (interval == -1 && !wglfunc.have_EXT_swap_control_tear) {
		buildputs("adaptive glswapinterval is not available\n");
		return OSDCMD_OK;
	}

	glswapinterval = interval;
	wglfunc.wglSwapIntervalEXT(interval);

	return OSDCMD_OK;
}
#endif

} // namespace

//
// initsystem() -- init systems
//
int initsystem()
{
	buildputs("Initialising Windows system interface\n");

	// get the desktop dimensions before anything changes them
	DEVMODE desktopmode;
	::ZeroMemory(&desktopmode, sizeof(DEVMODE));
	desktopmode.dmSize = sizeof(DEVMODE);
	::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &desktopmode);

	desktopxdim = desktopmode.dmPelsWidth;
	desktopydim = desktopmode.dmPelsHeight;
	desktopbpp  = desktopmode.dmBitsPerPel;

	std::ranges::fill(curpalette, palette_t{});

	atexit(uninitsystem);

	frameplace = 0;

#if USE_OPENGL
	std::memset(&wglfunc, 0, sizeof(wglfunc));
	
	glunavailable = loadgldriver(getenv("BUILD_GLDRV"));

	if (!glunavailable) {
		// Load the core WGL functions.
		wglfunc.wglGetProcAddress = static_cast<wglGetProcAddr_t>(getglprocaddress("wglGetProcAddress", 0));
		wglfunc.wglCreateContext  = static_cast<wglCreateCtx_t>(getglprocaddress("wglCreateContext", 0));
		wglfunc.wglDeleteContext  = static_cast<wglDeleteCtx_t>(getglprocaddress("wglDeleteContext", 0));
		wglfunc.wglMakeCurrent    = static_cast<wglMakeCurrent_t>(getglprocaddress("wglMakeCurrent", 0));
		wglfunc.wglSwapBuffers    = static_cast<wglSwapBuffer_t>(getglprocaddress("wglSwapBuffers", 0));
		glunavailable = !wglfunc.wglGetProcAddress ||
		 	   !wglfunc.wglCreateContext ||
		 	   !wglfunc.wglDeleteContext ||
		 	   !wglfunc.wglMakeCurrent ||
		 	   !wglfunc.wglSwapBuffers;
	}
	if (glunavailable) {
		buildputs("Failed loading OpenGL driver. GL modes will be unavailable.\n");
		std::memset(&wglfunc, 0, sizeof(wglfunc));
	} else {
		OSD_RegisterFunction("glswapinterval", "glswapinterval: frame swap interval for OpenGL modes. 0 = no vsync, -1 = adaptive, max 2", set_glswapinterval);
	}
#endif

	OSD_RegisterFunction("maxrefreshfreq", "maxrefreshfreq: maximum display frequency to set for OpenGL Polymost modes (0=no maximum)", set_maxrefreshfreq);
	return 0;
}


//
// uninitsystem() -- uninit systems
//
void uninitsystem()
{
	DestroyAppWindow();

	startwin_close();

	uninitinput();
	uninittimer();

	win_allowtaskswitching(1);

	shutdownvideo();
#if USE_OPENGL
	glbuild_unloadfunctions();
	std::memset(&wglfunc, 0, sizeof(wglfunc));
	unloadgldriver();
#endif
}


//
// initputs() -- prints a string to the intitialization window
//
void initputs(std::string_view buf)
{
	startwin_puts(buf);
}


//
// debugprintf() -- sends a debug string to the debugger
//
void debugprintf(const char *f, ...)
{
#ifdef DEBUGGINGAIDS
	va_list va;
	char buf[1024];

	va_start(va,f);
	vsnprintf(buf, sizeof(buf), f, va);
	va_end(va);

	if (::IsDebuggerPresent()) {
		::OutputDebugString(buf);
	}
	else {
		std::fputs(buf, stdout);
	}
#endif
}


//
// handleevents() -- process the Windows message queue
//   returns !0 if there was an important event worth checking (like quitting)
//
namespace {

bool eatosdinput{false};

} // namespace

int handleevents() {
	MSG msg;

	while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			quitevent = true;
		}

		if (startwin_idle((void*)&msg) > 0) {
			continue;
		}

		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	eatosdinput = false;
	updatemouse();
	updatejoystick();

	int rv{0};

	if (!appactive || quitevent) {
		rv = -1;
	}

	sampletimer();

	return rv;
}


//-------------------------------------------------------------------------------------------------
//  INPUT (MOUSE/KEYBOARD)
//=================================================================================================

namespace {

char moustat{0};
char mousegrab{0};
int joyblast{0};

int xinputusernum{-1};

// I don't see any pressing need to store the key-up events yet
#define SetKey(key,state) { \
	keystatus[key] = state; \
		if (state) { \
	keyfifo[keyfifoend] = key; \
	keyfifo[(keyfifoend+1)&(KEYFIFOSIZ-1)] = state; \
	keyfifoend = ((keyfifoend+2)&(KEYFIFOSIZ-1)); \
		} \
}

} // namespace

//
// initinput() -- init input system
//
int initinput()
{
	moustat = 0;
	std::ranges::fill(keystatus, 0);
	keyfifoplc = 0;
	keyfifoend = 0;
	keyasciififoplc = 0;
	keyasciififoend = 0;

	inputdevices = 1;
	joynumaxes=0;
	joynumbuttons=0;

	fetchkeynames();

	{
		XINPUT_CAPABILITIES caps;

		buildputs("Initialising game controllers\n");

		for (DWORD usernum{0}; usernum < XUSER_MAX_COUNT; ++usernum) {
			const DWORD result = ::XInputGetCapabilities(usernum, XINPUT_FLAG_GAMEPAD, &caps);
			if (result == ERROR_SUCCESS && xinputusernum < 0) {
				xinputusernum = (int)usernum;
				inputdevices |= 4;

				joynumbuttons = 15;
				joynumaxes = 6;
			}
		}
		
		if (xinputusernum >= 0) {
			buildprintf("  - Using controller in port {}\n", xinputusernum);
		}
		else {
			buildputs("  - No usable controller found\n");
		}
	}

	return 0;
}


//
// uninitinput() -- uninit input system
//
void uninitinput()
{
	uninitmouse();

	xinputusernum = -1;
	inputdevices &= ~4;
}


//
// initmouse() -- init mouse input
//
int initmouse()
{
	if (moustat) {
		return 0;
	}

	buildputs("Initialising mouse\n");

	// Register for mouse raw input.
	const RAWINPUTDEVICE rid{
		.usUsagePage = 0x01,
		.usUsage = 0x02,
		.dwFlags = 0, // We want legacy events when the mouse is not grabbed, so no RIDEV_NOLEGACY.
	    .hwndTarget = nullptr
	};

	if (!::RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
		buildprintf("initinput: could not register for raw mouse input ({})\n",
			getwindowserrorstr(::GetLastError()));
		return -1;
	}

	// grab input
	moustat = 1;
	inputdevices |= 2;
	grabmouse(1);

	return 0;
}


//
// uninitmouse() -- uninit mouse input
//
void uninitmouse()
{
	if (!moustat) {
		return;
	}

	grabmouse(0);
	moustat = 0;
	mousegrab = 0;

	// Unregister for mouse raw input.
	const RAWINPUTDEVICE rid {
		.usUsagePage = 0x01,
		.usUsage = 0x02,
		.dwFlags = RIDEV_REMOVE,
		.hwndTarget = nullptr
	};

	if (!::RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
		buildprintf("initinput: could not unregister for raw mouse input ({})\n",
			getwindowserrorstr(::GetLastError()));
	}
}

namespace {

void constrainmouse(int a)
{
	if (!hWindow) {
		return;
	}

	if (a) {
		RECT rect;

		::GetWindowRect(hWindow, &rect);

		const LONG x = rect.left + (rect.right - rect.left) / 2;
		const LONG y = rect.top + (rect.bottom - rect.top) / 2;
		rect.left = x - 1;
		rect.right = x + 1;
		rect.top = y - 1;
		rect.bottom = y + 1;

		::ClipCursor(&rect);
		::ShowCursor(FALSE);
	}
	else {
		::ClipCursor(nullptr);
		::ShowCursor(TRUE);
	}
}

void updatemouse()
{
	const unsigned int t = getticks();

	if (!mousegrab) {
		return;
	}

	// we only want the wheel to signal once, but hold the state for a moment
	if (mousewheel[0] > 0 && t - mousewheel[0] > MouseWheelFakePressTime) {
		mousewheel[0] = 0;
		mouseb &= ~16;
	}
	if (mousewheel[1] > 0 && t - mousewheel[1] > MouseWheelFakePressTime) {
		mousewheel[1] = 0;
		mouseb &= ~32;
	}
}

} // namespace

//
// grabmouse() -- show/hide mouse cursor
//
void grabmouse(int a)
{
	if (!moustat) {
		return;
	}

	mousegrab = a;

	constrainmouse(a);

	mousex = 0;
	mousey = 0;
	mouseb = 0;
}


//
// readmousexy() -- return mouse motion information
//
void readmousexy(int *x, int *y)
{
	if (!moustat || !mousegrab) {
		*x = 0;
		*y = 0;
		return;
	}

	*x = mousex;
	*y = mousey;
	mousex = 0;
	mousey = 0;
}


//
// readmousebstatus() -- return mouse button information
//
void readmousebstatus(int *b)
{
	if (!moustat || !mousegrab) {
		*b = 0;
	}
	else {
		*b = mouseb;
	}
}

namespace {

void updatejoystick()
{
	if (xinputusernum < 0) {
		return;
	}

	XINPUT_STATE state;

	::ZeroMemory(&state, sizeof(state));

	if (::XInputGetState(xinputusernum, &state) != ERROR_SUCCESS) {
		buildputs("Joystick error, disabling.\n");
		joyb = 0;
		std::ranges::fill(joyaxis, 0);
		xinputusernum = -1;
		return;
	}

	// We use SDL's game controller button order for BUILD:
	//   A, B, X, Y, Back, (Guide), Start, LThumb, RThumb,
	//   LShoulder, RShoulder, DPUp, DPDown, DPLeft, DPRight
	// So we must shuffle XInput around.
	joyb = ((state.Gamepad.wButtons & 0xF000) >> 12) |		// A,B,X,Y
	       ((state.Gamepad.wButtons & 0x0020) >> 1) |		// Back
	       ((state.Gamepad.wButtons & 0x0010) << 2) |       // Start
	       ((state.Gamepad.wButtons & 0x03C0) << 1) |       // LThumb,RThumb,LShoulder,RShoulder
	       ((state.Gamepad.wButtons & 0x000F) << 8);		// DPadUp,Down,Left,Right

	joyaxis[0] = state.Gamepad.sThumbLX;
	joyaxis[1] = -state.Gamepad.sThumbLY;
	joyaxis[2] = state.Gamepad.sThumbRX;
	joyaxis[3] = -state.Gamepad.sThumbRY;
	joyaxis[4] = (state.Gamepad.bLeftTrigger >> 1) | ((int)state.Gamepad.bLeftTrigger << 7);	// Extend to 0-32767
	joyaxis[5] = (state.Gamepad.bRightTrigger >> 1) | ((int)state.Gamepad.bRightTrigger << 7);
}

} // namespace

void releaseallbuttons()
{
	mousewheel[0] = 0;
	mousewheel[1] = 0;
	mouseb = 0;

	joyb = 0;
	joyblast = 0;

	for (int i{0}; i < 256; i++) {
		//if (!keystatus[i]) continue;
		//if (OSD_HandleKey(i, 0) != 0) {
			OSD_HandleKey(i, 0);
			SetKey(i, 0);
		//}
	}
}

namespace {
//
// fetchkeynames() -- retrieves the names for all the keys on the keyboard
//
void putkeyname(int vsc, int ex, int scan) {
	std::array<TCHAR, 24> tbuf;

	vsc <<= 16;
	vsc |= ex << 24;
	if (::GetKeyNameText(vsc, tbuf.data(), 24) == 0) return;
	::CharToOemBuff(tbuf.data(), keynames[scan], 24-1);

	//buildprintf("VSC %8x scan %-2x = {}\n", vsc, scan, keynames[scan]);
}

void fetchkeynames()
{
	std::memset(keynames, 0, sizeof(keynames));

	for (int i{0}; i < 256; ++i) {
		const int scan = wscantable[i];

		if (scan != 0) {
			putkeyname(i, 0, scan);
		}

		const int xscan = wxscantable[i];

		if (xscan != 0) {
			putkeyname(i, 1, xscan);
		}
	}
}

} // namespace

const char *getkeyname(int num)
{
	if ((unsigned)num >= 256) {
		return nullptr;
	}

	return keynames[num];
}

std::string_view getjoyname(int what, int num)
{
	static constexpr std::array<std::string_view, 6> axisnames = {
		"Left Stick X",
		"Left Stick Y",
		"Right Stick X",
		"Right Stick Y",
		"Left Trigger",
		"Right Trigger",
	};

	static constexpr std::array<std::string_view, 15> buttonnames = {
		"A",
		"B",
		"X",
		"Y",
		"Start",
		"Guide",
		"Back",
		"Left Thumb",
		"Right Thumb",
		"Left Shoulder",
		"Right Shoulder",
		"DPad Up",
		"DPad Down",
		"DPad Left",
		"DPad Right",
	};

	switch (what) {
		case 0:	// axis
			if ((unsigned)num >= (unsigned)6) return {};
			return axisnames[num];

		case 1: // button
			if ((unsigned)num >= (unsigned)15) return {};
			return buttonnames[num];

		default:
			return {};
	}
}

//-------------------------------------------------------------------------------------------------
//  TIMER
//=================================================================================================

namespace {

int64_t timerfreq{0};
int timerlastsample{0};
int timerticspersec{0};
void (*usertimercallback)() = nullptr;

} // namespace

//  This timer stuff is all Ken's idea.

//
// inittimer() -- initialise timer
//
int inittimer(int tickspersecond, void(*callback)())
{
	if (timerfreq) {
		return 0;	// already installed
	}

	buildputs("Initialising timer\n");

	// OpenWatcom seems to want us to query the value into a local variable
	// instead of the global 'timerfreq' or else it gets pissed with an
	// access violation
	int64_t t;
	if (!::QueryPerformanceFrequency((LARGE_INTEGER*)&t)) {
		::ShowErrorBox("Failed fetching timer frequency");
		return -1;
	}
	timerfreq = t;
	timerticspersec = tickspersecond;
	::QueryPerformanceCounter((LARGE_INTEGER*)&t);
	timerlastsample = (int)(t*timerticspersec / timerfreq);

	usertimercallback = callback;

	return 0;
}

//
// uninittimer() -- shut down timer
//
void uninittimer()
{
	if (!timerfreq) {
		return;
	}

	timerfreq = 0;
	timerticspersec = 0;
}

//
// sampletimer() -- update totalclock
//
void sampletimer()
{
	if (!timerfreq) {
		return;
	}

	int64_t i;
	::QueryPerformanceCounter((LARGE_INTEGER*)&i);
	int n = (int)(i * timerticspersec / timerfreq) - timerlastsample;

	if (n > 0) {
		totalclock += n;
		timerlastsample += n;
	}

	if (usertimercallback) {
		for (; n > 0; --n) {
			usertimercallback();
		}
	}
}


//
// getticks() -- returns the millisecond ticks count
//
unsigned int getticks()
{
	if (timerfreq == 0) {
		return 0;
	}

	int64_t i;
	::QueryPerformanceCounter((LARGE_INTEGER*)&i);
	return (unsigned int)(i * INT64_C(1000) / timerfreq);
}


//
// getusecticks() -- returns the microsecond ticks count
//
unsigned int getusecticks()
{
	if (timerfreq == 0) {
		return 0;
	}

	int64_t i;
	::QueryPerformanceCounter((LARGE_INTEGER*)&i);

	return (unsigned int)(i * INT64_C(1000000) / timerfreq);
}


//
// gettimerfreq() -- returns the number of ticks per second the timer is configured to generate
//
int gettimerfreq()
{
	return timerticspersec;
}




//-------------------------------------------------------------------------------------------------
//  VIDEO
//=================================================================================================

namespace {

// DIB stuff
HDC      hDCSection{nullptr};
HBITMAP  hDIBSection{nullptr};
HPALETTE hPalette{nullptr};
VOID    *lpPixels{nullptr};

int setgammaramp(WORD gt[3][256]);
int getgammaramp(WORD gt[3][256]);

void shutdownvideo()
{
#if USE_OPENGL
	frame.clear();
	glbuild_delete_8bit_shader(&gl8bit);
	UninitOpenGL();
#endif
	UninitDIB();

	if (desktopmodeset) {
		::ChangeDisplaySettings(nullptr, 0);
		desktopmodeset = false;
	}
}

} // namespace

//
// setvideomode() -- set the video mode
//
int setvideomode(int x, int y, int c, bool fs)
{
	int refresh{-1};

	if ((fs == fullscreen) && (x == xres) && (y == yres) && (c == bpp) && !videomodereset) {
		OSD_ResizeDisplay(xres,yres);
		return 0;
	}

	const int modenum = checkvideomode(&x, &y, c, fs, 0);

	if (modenum < 0) {
		return -1;
	}

	if (modenum != 0x7fffffff) {
		refresh = validmode[modenum].extra;
	}

	if (hWindow && gammabrightness) {
		setgammaramp(sysgamma);
		gammabrightness = false;
	}

	if (baselayer_videomodewillchange) {
		baselayer_videomodewillchange();
	}

	shutdownvideo();

	buildprintf("Setting video mode {}x{} ({}-bit {})\n",
			x, y, c, (fs ? "fullscreen" : "windowed"));

	if (::CreateAppWindow(x, y, c, fs, refresh)) {
		return -1;
	}

	if (!gammabrightness) {
		if (getgammaramp(sysgamma) >= 0) {
			gammabrightness = true;
		}

		if (gammabrightness && setgamma(curgamma) < 0) {
			gammabrightness = false;
		}
	}

	modechange = true;
	videomodereset = false;

	if (baselayer_videomodedidchange) {
		baselayer_videomodedidchange();
	}

	OSD_ResizeDisplay(xres, yres);

	return 0;
}


//
// getvalidmodes() -- figure out what video modes are available
//

namespace {

void addmode(int x, int y, unsigned char c, bool fs, int ext)
{
	validmode.emplace_back(validmode_t{
		.xdim = x,
		.ydim = y,
		.bpp = c,
		.fs = fs,
		.extra = ext
	});

	buildprintf("  - {}x{} {}-bit {}\n", x, y, c, fs ? "fullscreen" : "windowed"); \
}

#define CHECKL(w,h) if ((w < maxx) && (h < maxy))
#define CHECKLE(w,h) if ((w <= maxx) && (h <= maxy))

#if USE_OPENGL
void cdsenummodes()
{
	DEVMODE dm;
	int i = 0;
	int j = 0;

	struct { unsigned x,y,bpp,freq; } modes[MAXVALIDMODES];
	int nmodes=0;
	static constexpr int maxx{ MAXXDIM };
	static constexpr int maxy{ MAXYDIM };

	// Enumerate display modes.
	::ZeroMemory(&dm,sizeof(DEVMODE));
	dm.dmSize = sizeof(DEVMODE);
	while (nmodes < MAXVALIDMODES && ::EnumDisplaySettings(nullptr, j, &dm)) {
		// Identify the same resolution and bit depth in the existing set.
		for (i=0;i<nmodes;i++) {
			if (modes[i].x == dm.dmPelsWidth
			 && modes[i].y == dm.dmPelsHeight
			 && modes[i].bpp == dm.dmBitsPerPel)
				break;
		}
		// A new mode, or a same format mode with a better refresh rate match.
		if ((i==nmodes) ||
		    (dm.dmDisplayFrequency <= maxrefreshfreq && dm.dmDisplayFrequency > modes[i].freq && maxrefreshfreq > 0) ||
		    (dm.dmDisplayFrequency > modes[i].freq && maxrefreshfreq == 0)) {
			if (i==nmodes) nmodes++;

			modes[i].x = dm.dmPelsWidth;
			modes[i].y = dm.dmPelsHeight;
			modes[i].bpp = dm.dmBitsPerPel;
			modes[i].freq = dm.dmDisplayFrequency;
		}

		j++;
		::ZeroMemory(&dm,sizeof(DEVMODE));
		dm.dmSize = sizeof(DEVMODE);
	}

	// Add what was found to the list.
	for (i=0;i<nmodes;i++) {
		CHECKLE(modes[i].x, modes[i].y) {
			addmode(modes[i].x, modes[i].y, modes[i].bpp, 1, modes[i].freq);
		}
	}
}
#endif

int sortmodes(const struct validmode_t *a, const struct validmode_t *b)
{
	int x;

	if ((x = a->fs   - b->fs)   != 0) return x;
	if ((x = a->bpp  - b->bpp)  != 0) return x;
	if ((x = a->xdim - b->xdim) != 0) return x;
	if ((x = a->ydim - b->ydim) != 0) return x;

	return 0;
}

} // namespace

void getvalidmodes()
{
	static const int defaultres[][2] = {
		{1920,1200},{1920,1080},{1600,1200},{1680,1050},{1600,900},{1400,1050},{1440,900},{1366,768},
		{1280,1024},{1280,960},{1280,800},{1280,720},{1152,864},{1024,768},{800,600},{640,480},
		{640,400},{512,384},{480,360},{400,300},{320,240},{320,200},{0,0}
	};
	int i;
	int maxx=0;
	int maxy=0;

	if (modeschecked) return;

	buildputs("Detecting video modes:\n");

	// Fullscreen 8-bit modes: upsamples to the desktop mode.
	maxx = desktopxdim;
	maxy = desktopydim;
	for (i=0; defaultres[i][0]; i++) {
		CHECKLE(defaultres[i][0],defaultres[i][1]) {
			addmode(defaultres[i][0], defaultres[i][1], 8, 1, -1);
		}
	}

#if USE_POLYMOST && USE_OPENGL
	// Fullscreen >8-bit modes.
	if (!glunavailable) cdsenummodes();
#endif

	// Windowed modes can't be bigger than the current desktop resolution.
	maxx = desktopxdim-1;
	maxy = desktopydim-1;

	// Windows 8-bit modes
	for (i=0; defaultres[i][0]; i++) {
		CHECKL(defaultres[i][0],defaultres[i][1]) {
			addmode(defaultres[i][0], defaultres[i][1], 8, 0, -1);
		}
	}

#if USE_POLYMOST && USE_OPENGL
	// Windowed >8-bit modes
	if (!glunavailable) {
		for (i=0; defaultres[i][0]; i++) {
			CHECKL(defaultres[i][0],defaultres[i][1]) {
				addmode(defaultres[i][0], defaultres[i][1], desktopbpp, 0, -1);
			}
		}
	}
#endif

	qsort((void*) &validmode[0], validmode.size(), sizeof(struct validmode_t), (int(*)(const void*,const void*))sortmodes);

	modeschecked=1;
}

#undef CHECK


//
// resetvideomode() -- resets the video system
//
void resetvideomode()
{
	videomodereset = true;
	modeschecked = 0;
}


//
// showframe() -- update the display
//
void showframe()
{
#if USE_OPENGL
	if (!glunavailable) {
		if (bpp == 8) {
			glbuild_update_8bit_frame(&gl8bit, &frame[0], xres, yres, bytesperline);
			glbuild_draw_8bit_frame(&gl8bit);
		}

		wglfunc.wglSwapBuffers(hDCGLWindow);
		return;
	}
#endif

	{
		if ((xres == desktopxdim && yres == desktopydim) || !fullscreen) {
			::BitBlt(hDCWindow, 0, 0, xres, yres, hDCSection, 0, 0, SRCCOPY);
		} else {
			int xpos;
			int ypos;
			int xscl;
			int yscl;
			const int desktopaspect = divscalen<16>(desktopxdim, desktopydim);
			const int frameaspect = divscalen<16>(xres, yres);

			if (desktopaspect >= frameaspect) {
				// Desktop is at least as wide as the frame. We maximise frame height and centre on width.
				ypos = 0;
				yscl = desktopydim;
				xscl = mulscalen<16>(desktopydim, frameaspect);
				xpos = (desktopxdim - xscl) >> 1;
			} else {
				// Desktop is narrower than the frame. We maximise frame width and centre on height.
				xpos = 0;
				xscl = desktopxdim;
				yscl = divscalen<16>(desktopxdim, frameaspect);
				ypos = (desktopydim - yscl) >> 1;
			}

			::StretchBlt(hDCWindow, xpos, ypos, xscl, yscl, hDCSection, 0, 0, xres, yres, SRCCOPY);
		}
	}
}


//
// setpalette() -- set palette values
//
// FIXME: Parameters are unused
int setpalette(int start, int num, const unsigned char* dapal)
{
	std::ignore = start;
	std::ignore = num;
	std::ignore = dapal;

#if USE_OPENGL
	if (!glunavailable && bpp == 8) {
		glbuild_update_8bit_palette(&gl8bit, &curpalettefaded[0]);
		return 0;
	}
#endif
	if (hDCSection) {
		std::array<RGBQUAD, 256> rgb;
		int i;

		for (i = 0; i < 256; i++) {
			rgb[i].rgbBlue = curpalettefaded[i].b;
			rgb[i].rgbGreen = curpalettefaded[i].g;
			rgb[i].rgbRed = curpalettefaded[i].r;
			rgb[i].rgbReserved = 0;
		}

		::SetDIBColorTable(hDCSection, 0, 256, rgb.data());
	}

	return 0;
}


//
// setgamma
//

namespace {

int setgammaramp(WORD gt[3][256])
{
	int i;
	i = ::SetDeviceGammaRamp(hDCWindow, gt) ? 0 : -1;
	return i;
}

} // namespace

int setgamma(float gamma)
{
	int i;
	WORD gt[3][256];

	if (!hWindow) return -1;

	gamma = 1.0 / gamma;
	for (i=0;i<256;i++) {
		gt[0][i] =
		gt[1][i] =
		gt[2][i] = static_cast<WORD>(std::min(65535, std::max(0, static_cast<int>(pow((double)i / 256.0, gamma) * 65535.0 + 0.5))));
	}

	return setgammaramp(gt);
}

namespace {

int getgammaramp(WORD gt[3][256])
{
	int i;

	if (!hWindow) return -1;

	i = ::GetDeviceGammaRamp(hDCWindow, gt) ? 0 : -1;

	return i;
}


//
// UninitDIB() -- clean up the DIB renderer
//
void UninitDIB()
{
	if (hPalette) {
		::DeleteObject(hPalette);
		hPalette = nullptr;
	}

	if (hDCSection) {
		::DeleteDC(hDCSection);
		hDCSection = nullptr;
	}

	if (hDIBSection) {
		::DeleteObject(hDIBSection);
		hDIBSection = nullptr;
	}
}


//
// SetupDIB() -- sets up DIB rendering
//
int SetupDIB(int width, int height)
{
	struct binfo {
		BITMAPINFOHEADER header;
		std::array<RGBQUAD, 256> colours;
	} dibsect;

	int i;

	// create the new DIB section
	std::memset(&dibsect, 0, sizeof(dibsect));
	numpages = 1;	// KJS 20031225
	dibsect.header.biSize = sizeof(dibsect.header);
	dibsect.header.biWidth = width|1;	// Ken did this
	dibsect.header.biHeight = -height;
	dibsect.header.biPlanes = 1;
	dibsect.header.biBitCount = 8;
	dibsect.header.biCompression = BI_RGB;
	dibsect.header.biClrUsed = 256;
	dibsect.header.biClrImportant = 256;
	for (i=0; i<256; i++) {
		dibsect.colours[i].rgbBlue = curpalettefaded[i].b;
		dibsect.colours[i].rgbGreen = curpalettefaded[i].g;
		dibsect.colours[i].rgbRed = curpalettefaded[i].r;
	}

	hDIBSection = ::CreateDIBSection(hDCWindow, (BITMAPINFO *)&dibsect, DIB_RGB_COLORS, &lpPixels, nullptr, 0);
	if (!hDIBSection || lpPixels == nullptr) {
		UninitDIB();
		ShowErrorBox("Error creating DIB section");
		return TRUE;
	}

	std::memset(lpPixels, 0, (((width|1) + 4) & ~3)*height);

	// create a compatible memory DC
	hDCSection = ::CreateCompatibleDC(hDCWindow);
	if (!hDCSection) {
		UninitDIB();
		ShowErrorBox("Error creating compatible DC");
		return TRUE;
	}

	// select the DIB section into the memory DC
	if (!::SelectObject(hDCSection, hDIBSection)) {
		UninitDIB();
		ShowErrorBox("Error creating compatible DC");
		return TRUE;
	}

	return FALSE;
}

} // namespace

#if USE_OPENGL

//
// loadgldriver -- loads an OpenGL DLL
//
bool loadgldriver(const char *dll)
{
	if (hGLDLL)
		return false;	// Already loaded

	if (!dll) {
		dll = "OPENGL32.DLL";
	}

	buildprintf("Loading {}\n", dll);

	hGLDLL = ::LoadLibrary(dll);

	if (!hGLDLL)
		return true;

	return false;
}

int unloadgldriver()
{
	if (!hGLDLL) return 0;
	::FreeLibrary(static_cast<HMODULE>(hGLDLL));
	hGLDLL = nullptr;
	return 0;
}

//
// getglprocaddress
//
void *getglprocaddress(const char *name, int ext)
{
	void *func = nullptr;
	if (!hGLDLL) {
		return nullptr;
	}

	if (ext && wglfunc.wglGetProcAddress) {
		func = wglfunc.wglGetProcAddress(name);
	}

	if (!func) {
		func = ::GetProcAddress(static_cast<HMODULE>(hGLDLL), name);
	}
	
	return func;
}


//
// UninitOpenGL() -- cleans up OpenGL rendering
//

void UninitOpenGL()
{
	if (hGLRC) {
#if USE_POLYMOST
		polymost_glreset();
#endif
		if (!wglfunc.wglMakeCurrent(nullptr,nullptr)) { }
		if (!wglfunc.wglDeleteContext(hGLRC)) { }
		hGLRC = nullptr;
	}
	if (hGLWindow) {
		if (hDCGLWindow) {
			::ReleaseDC(hGLWindow, hDCGLWindow);
			hDCGLWindow = nullptr;
		}

		::DestroyWindow(hGLWindow);
		hGLWindow = nullptr;
	}
}

// Enumerate the WGL interface extensions.
void EnumWGLExts(HDC hdc)
{
	const GLchar *extstr;
	char *workstr;
	char *workptr;
	char *nextptr = nullptr;
	char *ext = nullptr;
	int ack;

	wglfunc.wglGetExtensionsStringARB = static_cast<wglExtStringARB_t>(getglprocaddress("wglGetExtensionsStringARB", 1));
	if (!wglfunc.wglGetExtensionsStringARB) {
		debugprintf("Note: OpenGL does not provide WGL_ARB_extensions_string extension.\n");
		return;
	}

	extstr = wglfunc.wglGetExtensionsStringARB(hdc);

	debugprintf("WGL extensions supported:\n");
	workstr = workptr = strdup(extstr);
	while ((ext = Bstrtoken(workptr, " ", &nextptr, 1)) != nullptr) {
		if (IsSameAsNoCase(ext, "WGL_ARB_pixel_format")) {
			wglfunc.wglChoosePixelFormatARB = static_cast<wglChoosePixelFmtARB_t>(getglprocaddress("wglChoosePixelFormatARB", 1));
			ack = !wglfunc.wglChoosePixelFormatARB ? '!' : '+';
		} else if (IsSameAsNoCase(ext, "WGL_ARB_create_context")) {
			wglfunc.wglCreateContextAttribsARB = static_cast<wglCreateCtxAttribsARB_t>(getglprocaddress("wglCreateContextAttribsARB", 1));
			ack = !wglfunc.wglCreateContextAttribsARB ? '!' : '+';
		} else if (IsSameAsNoCase(ext, "WGL_ARB_create_context_profile")) {
			wglfunc.have_ARB_create_context_profile = 1;
			ack = '+';
		} else if (IsSameAsNoCase(ext, "WGL_EXT_multisample") || IsSameAsNoCase(ext, "WGL_ARB_multisample")) {
			wglfunc.have_EXT_multisample = 1;
			ack = '+';
		} else if (IsSameAsNoCase(ext, "WGL_EXT_swap_control")) {
			wglfunc.have_EXT_swap_control = 1;
			wglfunc.wglSwapIntervalEXT = static_cast<wglSwapIntervalExt_t>(getglprocaddress("wglSwapIntervalEXT", 1));
			wglfunc.wglGetSwapIntervalEXT = static_cast<wglGetSwapIntervalExt_t>(getglprocaddress("wglGetSwapIntervalEXT", 1));
			ack = (!wglfunc.wglSwapIntervalEXT || !wglfunc.wglGetSwapIntervalEXT) ? '!' : '+';
		} else if (IsSameAsNoCase(ext, "WGL_EXT_swap_control_tear")) {
			wglfunc.have_EXT_swap_control_tear = 1;
			ack = '+';
		} else {
			ack = ' ';
		}
		debugprintf("  %s %c\n", ext, ack);
		workptr = nullptr;
	}
	std::free(workstr);
}

//
// SetupOpenGL() -- sets up opengl rendering
//
int SetupOpenGL(int width, int height, unsigned char bitspp)
{
	int err;
	int pixelformat;

	// Step 1. Create a fake context with a safe pixel format descriptor.
	GLuint dummyPixelFormat;
	PIXELFORMATDESCRIPTOR dummyPfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,                             //Version Number
		PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER, //Must Support these
		PFD_TYPE_RGBA,                 //Request An RGBA Format
		32,                            //Color Depth
		0,0,0,0,0,0,                   //Color Bits Ignored
		0,                             //No Alpha Buffer
		0,                             //Shift Bit Ignored
		0,                             //No Accumulation Buffer
		0,0,0,0,                       //Accumulation Bits Ignored
		24,                            //16/24/32 Z-Buffer depth
		8,                             //Stencil Buffer
		0,                             //No Auxiliary Buffer
		PFD_MAIN_PLANE,                //Main Drawing Layer
		0,                             //Reserved
		0,0,0                          //Layer Masks Ignored
	};
	HDC dummyhDC = nullptr;
	HGLRC dummyhGLRC = nullptr;
	const char *errmsg = nullptr;

	dummyhGLwindow = ::CreateWindow(
			WINDOW_CLASS,
			"OpenGL Window",
			WS_CHILD,
			0,0,
			1,1,
			hWindow,
			(HMENU)nullptr,
			hInstance,
			nullptr);
	if (!dummyhGLwindow) {
		errmsg = "Error creating dummy OpenGL child window.";
		goto fail;
	}

	dummyhDC = ::GetDC(dummyhGLwindow);
	if (!dummyhDC) {
		errmsg = "Error getting dummy device context";
		goto fail;
	}

	dummyPixelFormat = ::ChoosePixelFormat(dummyhDC, &dummyPfd);
	if (!dummyPixelFormat) {
		errmsg = "Can't choose dummy pixel format";
		goto fail;
	}

	err = ::SetPixelFormat(dummyhDC, dummyPixelFormat, &dummyPfd);
	if (!err) {
		errmsg = "Can't set dummy pixel format";
		goto fail;
	}

	dummyhGLRC = wglfunc.wglCreateContext(dummyhDC);
	if (!dummyhGLRC) {
		errmsg = "Can't create dummy GL context";
		goto fail;
	}

	if (!wglfunc.wglMakeCurrent(dummyhDC, dummyhGLRC)) {
		errmsg = "Can't activate dummy GL context";
		goto fail;
	}

	// Step 2. Check the WGL extensions.
	::EnumWGLExts(dummyhDC);

	// Step 3. Create the actual window we will use going forward.
	hGLWindow = ::CreateWindow(
			WINDOW_CLASS,
			"OpenGL Window",
			WS_CHILD|WS_VISIBLE,
			0, 0,
			width, height,
			hWindow,
			(HMENU)nullptr,
			hInstance,
			nullptr);
	if (!hGLWindow) {
		errmsg = "Error creating OpenGL child window.";
		goto fail;
	}

	hDCGLWindow = ::GetDC(hGLWindow);
	if (!hDCGLWindow) {
		errmsg = "Error getting device context.";
		goto fail;
	}

#if USE_POLYMOST && USE_OPENGL
	if (!wglfunc.have_EXT_multisample) {
		glmultisample = 0;
	}
#endif

	// Step 3. Find and set a suitable pixel format.
	if (wglfunc.wglChoosePixelFormatARB) {
		UINT numformats;
		const int pformatattribs[] = {
			WGL_DRAW_TO_WINDOW_ARB,	GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
			WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB,     bitspp,
			WGL_DEPTH_BITS_ARB,     24,
			WGL_STENCIL_BITS_ARB,   0,
			WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
#if USE_POLYMOST && USE_OPENGL
			WGL_SAMPLE_BUFFERS_EXT, glmultisample > 0,
			WGL_SAMPLES_EXT,        glmultisample > 0 ? (1 << glmultisample) : 0,
#endif
			0,
		};
		if (!wglfunc.wglChoosePixelFormatARB(hDCGLWindow, pformatattribs, nullptr, 1, &pixelformat, &numformats)) {
			errmsg = "Can't choose pixel format.";
			goto fail;
		} else if (numformats < 1) {
			errmsg = "No suitable pixel format available.";
			goto fail;
		}
	} else {
		const PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),
			1,                             //Version Number
			PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER, //Must Support these
			PFD_TYPE_RGBA,                 //Request An RGBA Format
			bitspp,                        //Color Depth
			0,0,0,0,0,0,                   //Color Bits Ignored
			0,                             //No Alpha Buffer
			0,                             //Shift Bit Ignored
			0,                             //No Accumulation Buffer
			0,0,0,0,                       //Accumulation Bits Ignored
			24,                            //16/24/32 Z-Buffer depth
			0,                             //Stencil Buffer
			0,                             //No Auxiliary Buffer
			PFD_MAIN_PLANE,                //Main Drawing Layer
			0,                             //Reserved
			0,0,0                          //Layer Masks Ignored
		};
		pixelformat = ::ChoosePixelFormat(hDCGLWindow, &pfd);
		if (!pixelformat) {
			errmsg = "Can't choose pixel format";
			goto fail;
		}
	}

	::DescribePixelFormat(hDCGLWindow, pixelformat, sizeof(PIXELFORMATDESCRIPTOR), &dummyPfd);
	err = ::SetPixelFormat(hDCGLWindow, pixelformat, &dummyPfd);
	if (!err) {
		errmsg = "Can't set pixel format.";
		goto fail;
	}

	// Step 4. Create a context with the needed profile.
	if (wglfunc.wglCreateContextAttribsARB) {
		int contextattribs[] = {
#if (USE_OPENGL == USE_GL3)
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 2,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#else
			WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
			WGL_CONTEXT_MINOR_VERSION_ARB, 1,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#endif
			0,
		};
		if (!wglfunc.have_ARB_create_context_profile) {
			contextattribs[4] = 0;	//WGL_CONTEXT_PROFILE_MASK_ARB
		}
		hGLRC = wglfunc.wglCreateContextAttribsARB(hDCGLWindow, nullptr, contextattribs);
		if (!hGLRC) {
			errmsg = "Can't create GL context.";
			goto fail;
		}
	} else {
		hGLRC = wglfunc.wglCreateContext(hDCGLWindow);
		if (!hGLRC) {
			errmsg = "Can't create GL context.";
			goto fail;
		}
	}

	// Scrap the dummy stuff.
	if (!wglfunc.wglMakeCurrent(nullptr, nullptr)) { }
	if (!wglfunc.wglDeleteContext(dummyhGLRC)) { }
	::ReleaseDC(dummyhGLwindow, dummyhDC);
	::DestroyWindow(dummyhGLwindow);
	dummyhGLwindow = nullptr;
	dummyhGLRC = nullptr;
	dummyhDC = nullptr;

	if (!wglfunc.wglMakeCurrent(hDCGLWindow, hGLRC)) {
		errmsg = "Can't activate GL context";
		goto fail;
	}

	// Step 5. Done.
	switch (glbuild_init()) {
		case 0:
			break;
		case -1:
			errmsg = "Can't load required OpenGL function pointers.";
			goto fail;
		case -2:
			errmsg = "Minimum OpenGL requirements are not met.";
			goto fail;
		default:
			errmsg = "Other OpenGL initialisation error.";
			goto fail;
	}

	if (wglfunc.have_EXT_swap_control && wglfunc.wglSwapIntervalEXT) {
		if (glswapinterval == -1 && !wglfunc.have_EXT_swap_control_tear) glswapinterval = 1;
		if (!wglfunc.wglSwapIntervalEXT(glswapinterval)) {
			buildputs("note: OpenGL swap interval could not be changed\n");
		}
	}
	numpages = 127;

	return FALSE;

fail:
	if (bpp > 8) {
		ShowErrorBox(errmsg);
	}
	shutdownvideo();

	if (!wglfunc.wglMakeCurrent(nullptr, nullptr)) { }

	if (hGLRC) {
		if (!wglfunc.wglDeleteContext(hGLRC)) { }
	}
	if (hGLWindow) {
		if (hDCGLWindow) {
			::ReleaseDC(hGLWindow, hDCGLWindow);
		}
	}
	hDCGLWindow = nullptr;
	hGLRC = nullptr;
	hGLWindow = nullptr;

	if (dummyhGLRC) {
		if (!wglfunc.wglDeleteContext(dummyhGLRC)) { }
	}
	if (dummyhGLwindow) {
		if (dummyhDC) {
			::ReleaseDC(dummyhGLwindow, dummyhDC);
		}
		::DestroyWindow(dummyhGLwindow);
	}

	return TRUE;
}

#endif	//USE_OPENGL

//
// CreateAppWindow() -- create the application window
//
static BOOL CreateAppWindow(int width, int height, int bitspp, bool fs, int refresh)
{
	RECT rect;
	int ww;
	int wh;
	int wx;
	int wy;
	int vw;
	int vh;
	int stylebits = 0;
	int stylebitsex = 0;

	if (width == xres && height == yres && fs == fullscreen && bitspp == bpp && !videomodereset)
		return FALSE;

	if (hWindow) {
		::ShowWindow(hWindow, SW_HIDE);	// so Windows redraws what's behind if the window shrinks
	}

	if (fs) {
		stylebitsex = WS_EX_TOPMOST;
		stylebits = WS_POPUP;
	} else {
		stylebitsex = 0;
		stylebits = (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);
	}

	if (!hWindow) {
		hWindow = ::CreateWindowEx(
			stylebitsex,
			"buildapp",
			apptitle,
			stylebits,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			320,
			200,
			nullptr,
			nullptr,
			hInstance,
			nullptr);
		if (!hWindow) {
			ShowErrorBox("Unable to create window");
			return TRUE;
		}

		hDCWindow = ::GetDC(hWindow);
		if (!hDCWindow) {
			ShowErrorBox("Error getting device context");
			return TRUE;
		}

		startwin_close();
	} else {
		::SetWindowLong(hWindow,GWL_EXSTYLE,stylebitsex);
		::SetWindowLong(hWindow,GWL_STYLE,stylebits);
	}

	// resize the window
	if (!fs) {
		rect.left = 0;
		rect.top = 0;
		rect.right = width;
		rect.bottom = height;
		::AdjustWindowRectEx(&rect, stylebits, FALSE, stylebitsex);

		ww = (rect.right - rect.left);
		wh = (rect.bottom - rect.top);
		wx = (desktopxdim - ww) / 2;
		wy = (desktopydim - wh) / 2;
		vw = width;
		vh = height;
	} else {
		wx=wy=0;
		ww=vw=desktopxdim;
		wh=vh=desktopydim;
	}
	::SetWindowPos(hWindow, HWND_TOP, wx, wy, ww, wh, 0);

	UpdateAppWindowTitle();
	::ShowWindow(hWindow, SW_SHOWNORMAL);
	::SetForegroundWindow(hWindow);
	::SetFocus(hWindow);

	if (bitspp == 8) {
		int i, j;

#if USE_OPENGL
		if (glunavailable) {
#endif
			// 8-bit software with no GL shader uses classic Windows DIB blitting.
			if (SetupDIB(width, height)) {
				return TRUE;
			}

			frameplace = (intptr_t)lpPixels;
			bytesperline = (((width|1) + 4) & ~3);
#if USE_OPENGL
		} else {
			// Prepare the GLSL shader for 8-bit blitting.
			if (SetupOpenGL(vw, vh, bitspp)) {
				// No luck. Write off OpenGL and try DIB.
				buildputs("OpenGL initialisation failed. Falling back to DIB mode.\n");
				glunavailable = true;
				return ::CreateAppWindow(width, height, bitspp, fs, refresh);
			}

			bytesperline = (((width|1) + 4) & ~3);

			if (glbuild_prepare_8bit_shader(&gl8bit, width, height, bytesperline, vw, vh) < 0) {
				shutdownvideo();
				return -1;
			}

			frame.resize(bytesperline * height);

			frameplace = (intptr_t)&frame[0];
		}
#endif

		imageSize = bytesperline*height;
		setvlinebpl(bytesperline);

		for(i=j=0; i<=height; i++) ylookup[i] = j, j += bytesperline;
		modechange = false;

		numpages = 1;
	} else {
#if USE_OPENGL
		if (fs) {
			DEVMODE dmScreenSettings;

			::ZeroMemory(&dmScreenSettings, sizeof(DEVMODE));
			dmScreenSettings.dmSize = sizeof(DEVMODE);
			dmScreenSettings.dmPelsWidth = width;
			dmScreenSettings.dmPelsHeight = height;
			dmScreenSettings.dmBitsPerPel = bitspp;
			dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
			if (refresh > 0) {
				dmScreenSettings.dmDisplayFrequency = refresh;
				dmScreenSettings.dmFields |= DM_DISPLAYFREQUENCY;
			}

			if (::ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
				ShowErrorBox("Video mode not supported");
				return TRUE;
			}
			desktopmodeset = true;
		}

		::ShowWindow(hWindow, SW_SHOWNORMAL);
		::SetForegroundWindow(hWindow);
		::SetFocus(hWindow);

		if (SetupOpenGL(width, height, bitspp)) {
			return TRUE;
		}

		frameplace = 0;
		bytesperline = 0;
		imageSize = 0;
#else
		return FALSE;
#endif
	}

	xres = width;
	yres = height;
	bpp = bitspp;
	fullscreen = fs;

	modechange = true;

	::UpdateWindow(hWindow);

	return FALSE;
}


//
// DestroyAppWindow() -- destroys the application window
//
static void DestroyAppWindow()
{
	if (hWindow && gammabrightness) {
		setgammaramp(sysgamma);
		gammabrightness = false;
	}

	shutdownvideo();

	if (hDCWindow) {
		::ReleaseDC(hWindow, hDCWindow);
		hDCWindow = nullptr;
	}

	if (hWindow) {
		::DestroyWindow(hWindow);
		hWindow = nullptr;
	}
}

//
// UpdateAppWindowTitle() -- sets the title of the application window
//
static void UpdateAppWindowTitle()
{
	if (!hWindow) {
		return;
	}

	std::array<char, 256 + 3 + 256 + 1> tmp{};		//sizeof(wintitle) + " - " + sizeof(apptitle) + '\0'

	if (wintitle[0]) {
		fmt::format_to(&tmp[0], "{} - {}", wintitle, apptitle);
		::SetWindowText(hWindow, &tmp[0]);
	} else {
		::SetWindowText(hWindow, apptitle);
	}
}

//-------------------------------------------------------------------------------------------------
//  MOSTLY STATIC INTERNAL WINDOWS THINGS
//=================================================================================================

//
// ShowErrorBox() -- shows an error message box
//
static void ShowErrorBox(const char *m)
{
	std::array<TCHAR, 1024> msg;

	::wsprintf(&msg[0], "%s: %s", m, ::GetWindowsErrorMsg(::GetLastError()));
	::MessageBox(nullptr, &msg[0], apptitle, MB_OK|MB_ICONSTOP);
}


//
// CheckWinVersion() -- check to see what version of Windows we happen to be running under
//
static BOOL CheckWinVersion()
{
	OSVERSIONINFO osv;

	::ZeroMemory(&osv, sizeof(osv));
	osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (!::GetVersionEx(&osv)) return TRUE;

	// At least Windows Vista
	if (osv.dwPlatformId != VER_PLATFORM_WIN32_NT) return TRUE;
	if (osv.dwMajorVersion < 6) return TRUE;

	return FALSE;
}

//
// WndProcCallback() -- the Windows window callback
//
static LRESULT CALLBACK WndProcCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#if USE_OPENGL
	if (hGLWindow && hWnd == hGLWindow) return ::DefWindowProc(hWnd,uMsg,wParam,lParam);
	if (dummyhGLwindow && hWnd == dummyhGLwindow) return ::DefWindowProc(hWnd,uMsg,wParam,lParam);
#endif

	switch (uMsg) {
		case WM_SYSCOMMAND:
			// don't let the monitor fall asleep or let the screensaver activate
			if (wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER) return 0;

			// Since DirectInput won't let me set an exclusive-foreground
			// keyboard for some unknown reason I just have to tell Windows to
			// rack off with its keyboard accelerators.
			if (wParam == SC_KEYMENU || wParam == SC_HOTKEY) return 0;
			break;

		case WM_ACTIVATEAPP:
			appactive = static_cast<bool>(wParam);
			if (backgroundidle)
				::SetPriorityClass( ::GetCurrentProcess(),
					appactive ? NORMAL_PRIORITY_CLASS : IDLE_PRIORITY_CLASS );
			break;

		case WM_ACTIVATE:
//			AcquireInputDevices(appactive);
			constrainmouse(LOWORD(wParam) != WA_INACTIVE && HIWORD(wParam) == 0);
			break;

		case WM_SIZE:
			if (wParam == SIZE_MAXHIDE || wParam == SIZE_MINIMIZED)
				appactive = false;
			else
				appactive = true;
//			AcquireInputDevices(appactive);
			break;

		case WM_DISPLAYCHANGE:
			// desktop settings changed so adjust our world-view accordingly
			desktopxdim = LOWORD(lParam);
			desktopydim = HIWORD(lParam);
			desktopbpp  = wParam;
			getvalidmodes();
			break;

		case WM_PAINT:
			break;

			// don't draw the frame if fullscreen
		//case WM_NCPAINT:
			//if (!fullscreen) break;
			//return 0;

		case WM_ERASEBKGND:
			break;//return TRUE;

		case WM_MOVE:
			windowposx = LOWORD(lParam);
			windowposy = HIWORD(lParam);
			return 0;

		case WM_CLOSE:
			quitevent = true;
			return 0;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
			{
				const int press = (lParam & 0x80000000L) == 0;
				const int wscan = (lParam >> 16) & 0xff;
				int scan = 0;

				if (lParam & (1<<24)) {
					scan = wxscantable[wscan];
				} else {
					scan = wscantable[wscan];
				}

				//buildprintf("VK %-2x VSC %8x scan %-2x = {}\n", wParam, (UINT)lParam, scan, keynames[scan]);

				if (scan == 0) {
					// Not a key we want, so give it to the OS to handle.
					break;
				} else if (scan == OSD_CaptureKey(-1)) {
					if (press) {
						OSD_ShowDisplay(-1);
						eatosdinput = true;
					}
				} else if (OSD_HandleKey(scan, press) != 0) {
					if (!keystatus[scan] || !press) {
						SetKey(scan, press);
					}
				}
			}
			return 0;

		case WM_CHAR:
			if (eatosdinput) {
				eatosdinput = false;
			} else if (OSD_HandleChar((unsigned char)wParam)) {
				if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
					keyasciififo[keyasciififoend] = (unsigned char)wParam;
					keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
					//buildprintf("Char {}, {}-{}\n",wParam,keyasciififoplc,keyasciififoend);
				}
			}
			return 0;

		case WM_HOTKEY:
			return 0;

		case WM_INPUT:
			{
				RAWINPUT raw;
				UINT dwSize = sizeof(RAWINPUT);

				::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, (LPVOID)&raw, &dwSize, sizeof(RAWINPUTHEADER));

				if (raw.header.dwType == RIM_TYPEMOUSE) {
					if (!mousegrab) {
						return 0;
					}

					for (int but{ 0 }; but < 4; but++) {  // Sorry XBUTTON2, I didn't plan for you.
						switch ((raw.data.mouse.usButtonFlags >> (but << 1)) & 3) {
							case 1:		// press
								mouseb |= (1 << but);
								break;
							case 2:		// release
								mouseb &= ~(1 << but);
								break;
							default: break;	// no change
						}
					}

					if (raw.data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
						const int direction = (short)raw.data.mouse.usButtonData < 0;	// 1 = down (-ve values), 0 = up

						// Repeated events before the fake button release timer
						// expires need to trigger a release and a new press.
						mousewheel[direction] = getticks();
						mouseb |= (16 << direction);
					}

					if (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
						static LONG absx = 0;
						static LONG absy = 0;
						static bool first{true};

						if (!first) {
							mousex += raw.data.mouse.lLastX - absx;
							mousey += raw.data.mouse.lLastY - absy;
						} else {
							first = false;
						}
						absx = raw.data.mouse.lLastX;
						absy = raw.data.mouse.lLastY;
					} else {
						mousex += raw.data.mouse.lLastX;
						mousey += raw.data.mouse.lLastY;
					}
				}
			}
			return 0;

		case WM_ENTERMENULOOP:
		case WM_ENTERSIZEMOVE:
//			AcquireInputDevices(0);
			return 0;
		case WM_EXITMENULOOP:
		case WM_EXITSIZEMOVE:
//			AcquireInputDevices(1);
			return 0;

		case WM_DESTROY:
			hWindow = nullptr;
			//PostQuitMessage(0);	// JBF 20040115: not anymore
			return 0;

		default:
			break;
	}

	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}


//
// RegisterWindowClass() -- register the window class
//
static BOOL RegisterWindowClass()
{
	WNDCLASSEX wcx;

	if (window_class_registered) return FALSE;

	//buildputs("Registering window class\n");

	wcx.cbSize	= sizeof(wcx);
	wcx.style	= CS_OWNDC;
	wcx.lpfnWndProc	= WndProcCallback;
	wcx.cbClsExtra	= 0;
	wcx.cbWndExtra	= 0;
	wcx.hInstance	= hInstance;
	wcx.hIcon	= static_cast<HICON>(::LoadImage(hInstance, MAKEINTRESOURCE(100), IMAGE_ICON,
				GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
	wcx.hCursor	= ::LoadCursor(nullptr, IDC_ARROW);
	wcx.hbrBackground = ::CreateSolidBrush(RGB(0,0,0));
	wcx.lpszMenuName = nullptr;
	wcx.lpszClassName = WINDOW_CLASS;
	wcx.hIconSm	= static_cast<HICON>(::LoadImage(hInstance, MAKEINTRESOURCE(100), IMAGE_ICON,
				GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
	if (!::RegisterClassEx(&wcx)) {
		ShowErrorBox("Failed to register window class");
		return TRUE;
	}

	window_class_registered = TRUE;

	return FALSE;
}


//
// GetWindowsErrorMsg() -- gives a pointer to a static buffer containing the Windows error message
//
static LPTSTR GetWindowsErrorMsg(DWORD code)
{
	static std::array<TCHAR, 1024> lpMsgBuf;

	::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf[0], lpMsgBuf.size(), nullptr);

	return &lpMsgBuf[0];
}

static const char *getwindowserrorstr(DWORD code)
{
	static std::array<char, 1024> msg;
	std::ranges::fill(msg, 0);
	::OemToCharBuff(GetWindowsErrorMsg(code), &msg[0], msg.size() - 1);
	return &msg[0];
}
