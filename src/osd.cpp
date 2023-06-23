// On-screen Display (ie. console)
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#include "build.hpp"
#include "osd.hpp"
#include "baselayer.hpp"
#include "string_utils.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <deque>
#include <ranges>
#include <string_view>
#include <vector>
#include <utility>

extern int getclosestcol(int r, int g, int b);	// engine.c
extern int qsetmode;	// engine.c

namespace {

struct symbol_t {
	std::string name;
	std::string help;
	int (*func)(const osdfuncparm_t *);
};

using symbol_cont = std::vector<symbol_t>;
using symbol_iter = std::vector<symbol_t>::iterator;
using symbol_const_iter = std::vector<symbol_t>::const_iterator;

symbol_cont symbols;

symbol_iter addnewsymbol(std::string_view name);
symbol_iter findsymbol(std::string_view name, symbol_iter startingat);
symbol_iter findexactsymbol(std::string_view name);

// Map of palette value to colour index for drawing: black, white, light grey, light blue.
std::array<int, 4> palmap256 = { -1, -1, -1, -1 };
constexpr std::array<int, 4> palmap16 = { 0, 15, 7, 9 };

void _internal_drawosdchar(int, int, char, int, int);
void _internal_drawosdstr(int, int, const char*, int, int, int);
void _internal_drawosdcursor(int,int,int,int);
int _internal_getcolumnwidth(int);
int _internal_getrowheight(int);
void _internal_clearbackground(int,int);
int _internal_gettime();
void _internal_onshowosd(int);

constexpr auto TEXTSIZE{16384};

// history display
std::array<char, TEXTSIZE> osdtext;
int  osdpos=0;			// position next character will be written at
int  osdlines=1;			// # lines of text in the buffer
int  osdrows=20;			// # lines of the buffer that are visible
int  osdcols=60;			// width of onscreen display in text columns
int  osdmaxrows=20;		// maximum number of lines which can fit on the screen
int  osdmaxlines = TEXTSIZE / 60;	// maximum lines which can fit in the buffer
bool osdvisible{false};		// onscreen display visible?
int  osdhead=0; 			// topmost visible line number
bool osdinited{false};		// text buffer initialised?
int  osdkey=0x45;		// numlock shows the osd
int  keytime=0;

// command prompt editing
constexpr auto EDITLENGTH{511};
int  osdovertype=0;		// insert (0) or overtype (1)
char osdeditbuf[EDITLENGTH+1];	// editing buffer
char osdedittmp[EDITLENGTH+1];	// editing buffer temporary workspace
int  osdeditlen=0;		// length of characters in edit buffer
int  osdeditcursor=0;		// position of cursor in edit buffer
int  osdeditshift=0;		// shift state
int  osdeditcontrol=0;		// control state
int  osdeditalt=0;		// alt state
int  osdeditcaps=0;		// capslock
int  osdeditwinstart=0;
int  osdeditwinend=60-1-3;
#define editlinewidth (osdcols-1-3) // FIXME: Taking a static variable at preprocess time... :|

// command processing
constexpr auto HISTORYDEPTH{16};
int  osdhistorypos=-1;		// position we are at in the history buffer
char osdhistorybuf[HISTORYDEPTH][EDITLENGTH+1];	// history strings
int  osdhistorysize=0;		// number of entries in history
symbol_iter lastmatch{};

// execution buffer
// the execution buffer works from the command history
int  osdexeccount=0;		// number of lines from the head of the history buffer to execute

// presentation parameters
int  osdpromptshade=0;
int  osdpromptpal=3;	// blue
int  osdeditshade=0;
int  osdeditpal=1;	// white
int  osdtextshade=0;
int  osdtextpal=2;	// light grey

// application callbacks
void (*drawosdchar)(int, int, char, int, int) = _internal_drawosdchar;
void (*drawosdstr)(int, int, const char*, int, int, int) = _internal_drawosdstr;
void (*drawosdcursor)(int, int, int, int) = _internal_drawosdcursor;
int (*getcolumnwidth)(int) = _internal_getcolumnwidth;
int (*getrowheight)(int) = _internal_getrowheight;
void (*clearbackground)(int,int) = _internal_clearbackground;
int (*gettime)() = _internal_gettime;
void (*onshowosd)(int) = _internal_onshowosd;

void findwhite()
{
	if (qsetmode == 200) {
		palmap256[0] = getclosestcol(0, 0, 0);    // black
		palmap256[1] = getclosestcol(63, 63, 63);	// white
		palmap256[2] = getclosestcol(42, 42, 42);	// light grey
		palmap256[3] = getclosestcol(21, 21, 63);	// light blue
	}
}

void _internal_drawosdchar(int x, int y, char ch, int shade, int pal)
{
	const char st[2] = {ch, 0};
	std::ignore = shade;

	int colour;
	int shadow;

	if (qsetmode == 200) {
		colour = palmap256[pal%4];
		shadow = palmap256[0];
	} else {
		colour = palmap16[pal%4];
		shadow = palmap16[0];
		// printext256 happens to work in 2D mode.
	}
	printext256(4 + (x * 8) + 1, 4 + (y * 14) + 1, shadow, -1, st, 2);
	printext256(4 + (x * 8), 4 + (y * 14), colour, -1, st, 2);
}

void _internal_drawosdstr(int x, int y, const char *ch, int len, int shade, int pal)
{
	std::array<char, 1024> st;
	int colour;
	int shadow;

	std::ignore = shade;

	if (len>1023)
		len = 1023;

	std::memcpy(&st[0], ch, len);

	st[len] = 0;

	if (qsetmode == 200) {
		colour = palmap256[pal%4];
		shadow = palmap256[0];
	} else {
		colour = palmap16[pal%4];
		shadow = palmap16[0];
		// printext256 happens to work in 2D mode.
	}

	printext256(4 + (x * 8) + 1, 4 + (y * 14) + 1, shadow, -1, &st[0], 2);
	printext256(4 + (x * 8), 4 + (y * 14), colour, -1, &st[0], 2);
}

void _internal_drawosdcursor(int x, int y, int type, int lastkeypress)
{
	char st[2] = { '\x16', 0 };  // solid lower third of character cell
	int colour;
	int yoff = 2;

	const unsigned int blinkcycle = gettime() - (unsigned)lastkeypress;
	if (blinkcycle % 1000 > 500) return;  // blink each half-second.

	if (type) {
		st[0] = '\xdb';  // solid block
		yoff = 0;
	}

	if (qsetmode == 200) {
		colour = palmap256[2];
	} else {
		colour = palmap16[2];
		// printext256 happens to work in 2D mode.
	}
	printext256(4 + (x * 8), 4 + (y * 14) + yoff, colour, -1, st, 2);
}

int _internal_getcolumnwidth(int w)
{
	return w / 8 - 1;
}

int _internal_getrowheight(int w)
{
	return w / 14;
}

void _internal_clearbackground(int cols, int rows)
{
	std::ignore = cols; std::ignore = rows;
}

int _internal_gettime()
{
	return (int)getticks();
}

void _internal_onshowosd(int shown)
{
	std::ignore = shown;
}

////////////////////////////

int osdcmd_osdvars(const osdfuncparm_t *parm)
{
	const int showval = (parm->parms.size() < 1);

	if (IsSameAsNoCase(parm->name, "osdrows")) {
		if (showval) {
			OSD_Printf("osdrows is %d\n", osdrows); return OSDCMD_OK;
		}
		else {
			const std::string_view parmv{parm->parms[0]};
			/*auto [ptr, ec] = */ std::from_chars(parmv.data(), parmv.data() + parmv.size(), osdrows);
			// TODO: What to do with ec here?
			if (osdrows < 1)
				osdrows = 1;
			else if (osdrows > osdmaxrows)
				osdrows = osdmaxrows;

			return OSDCMD_OK;
		}
	}
	return OSDCMD_SHOWHELP;
}

int osdcmd_listsymbols(const osdfuncparm_t *parm)
{
	std::ignore = parm;

	OSD_Printf("Symbol listing:\n");
	for (const auto& symb : symbols)
		OSD_Printf("     %s\n", symb.name);

	return OSDCMD_OK;
}

int osdcmd_help(const osdfuncparm_t *parm)
{
	if (parm->parms.size() != 1)
		return OSDCMD_SHOWHELP;

	const auto symb = findexactsymbol(parm->parms[0].data());
	
	if (symb == symbols.end()) {
		OSD_Printf("Help Error: \"%s\" is not a defined variable or function\n", parm->parms[0]);
	} else {
		OSD_Printf("%s\n", symb->help);
	}

	return OSDCMD_OK;
}

int osdcmd_clear(const osdfuncparm_t *parm)
{
	if (parm->parms.size() != 0)
		return OSDCMD_SHOWHELP;

	std::ranges::fill(osdtext, 0);

	osdlines = 1;
	osdhead = 0;
	osdpos = 0;

	return OSDCMD_OK;
}

int osdcmd_echo(const osdfuncparm_t *parm)
{
	if (parm->parms.size() == 0)
		return OSDCMD_SHOWHELP;

	for (int i{0}; i < parm->parms.size(); ++i) {
		if (i)
			OSD_Puts(" ");
		OSD_Puts(parm->parms[i]);
	}

	OSD_Puts("\n");

	return OSDCMD_OK;
}

} // namespace

////////////////////////////


//
// OSD_Cleanup() -- Cleans up the on-screen display
//
void OSD_Cleanup()
{
	symbols.clear();
	osdinited = false;
}


//
// OSD_Init() -- Initialises the on-screen display
//
void OSD_Init()
{
    if (osdinited)
		return;

	osdinited = true;

	std::ranges::fill(osdtext, 0);
	osdlines = 1;

	atexit(OSD_Cleanup);

	OSD_RegisterFunction("listsymbols","listsymbols: lists all the recognized symbols",osdcmd_listsymbols);
	OSD_RegisterFunction("help","help: displays help on the named symbol",osdcmd_help);
	OSD_RegisterFunction("osdrows","osdrows: sets the number of visible lines of the OSD",osdcmd_osdvars);
	OSD_RegisterFunction("clear","clear: clear the OSD",osdcmd_clear);
	OSD_RegisterFunction("echo","echo: write text to the OSD",osdcmd_echo);
}


//
// OSD_SetFunctions() -- Sets some callbacks which the OSD uses to understand its world
//
void OSD_SetFunctions(
		void (*drawchar)(int,int,char,int,int),
		void (*drawstr)(int,int,const char*,int,int,int),
		void (*drawcursor)(int,int,int,int),
		int (*colwidth)(int),
		int (*rowheight)(int),
		void (*clearbg)(int,int),
		int (*gtime)(),
		void (*showosd)(int)
	)
{
	drawosdchar = drawchar;
	drawosdstr = drawstr;
	drawosdcursor = drawcursor;
	getcolumnwidth = colwidth;
	getrowheight = rowheight;
	clearbackground = clearbg;
	gettime = gtime;
	onshowosd = showosd;

	if (!drawosdchar) drawosdchar = _internal_drawosdchar;
	if (!drawosdstr) drawosdstr = _internal_drawosdstr;
	if (!drawosdcursor) drawosdcursor = _internal_drawosdcursor;
	if (!getcolumnwidth) getcolumnwidth = _internal_getcolumnwidth;
	if (!getrowheight) getrowheight = _internal_getrowheight;
	if (!clearbackground) clearbackground = _internal_clearbackground;
	if (!gettime) gettime = _internal_gettime;
	if (!onshowosd) onshowosd = _internal_onshowosd;
}


//
// OSD_SetParameters() -- Sets the parameters for presenting the text
//
void OSD_SetParameters(
		int promptshade, int promptpal,
		int editshade, int editpal,
		int textshade, int textpal
	)
{
	osdpromptshade = promptshade;
	osdpromptpal   = promptpal;
	osdeditshade   = editshade;
	osdeditpal     = editpal;
	osdtextshade   = textshade;
	osdtextpal     = textpal;
}


//
// OSD_CaptureKey() -- Sets the scancode for the key which activates the onscreen display
//
int OSD_CaptureKey(int sc)
{
	const int prev{ osdkey };

	if (sc >= 0) {
		osdkey = sc;
	}

	return prev;
}

namespace {

enum class OSDOP {
	START,
	END,
	LEFT,
	LEFT_WORD,
	RIGHT,
	RIGHT_WORD,
	BACKSPACE,
	DELETE,
	DELETE_START,
	DELETE_END,
	DELETE_WORD,
	CANCEL,
	COMPLETE,
	SUBMIT,
	HISTORY_UP,
	HISTORY_DOWN,
	SCROLL_TOP,
	SCROLL_BOTTOM,
	PAGE_UP,
	PAGE_DOWN,
};

void OSD_Manipulate(OSDOP op) {
	int i;
	int j;
	symbol_iter tabc{};

	switch (op) {
		case OSDOP::START:
			osdeditcursor = 0;
			osdeditwinstart = osdeditcursor;
			osdeditwinend = osdeditwinstart+editlinewidth;
			break;
		case OSDOP::END:
			osdeditcursor = osdeditlen;
			osdeditwinend = osdeditcursor;
			osdeditwinstart = osdeditwinend-editlinewidth;
			if (osdeditwinstart<0) {
				osdeditwinstart=0;
				osdeditwinend = editlinewidth;
			}
			break;
		case OSDOP::LEFT:
			if (osdeditcursor>0) {
				osdeditcursor--;
			}
			if (osdeditcursor<osdeditwinstart) {
				osdeditwinend-=(osdeditwinstart-osdeditcursor);
				osdeditwinstart-=(osdeditwinstart-osdeditcursor);
			}
			break;
		case OSDOP::LEFT_WORD:
			if (osdeditcursor>0) {
				while (osdeditcursor>0) {
					if (osdeditbuf[osdeditcursor-1] != 32) break;
					osdeditcursor--;
				}
				while (osdeditcursor>0) {
					if (osdeditbuf[osdeditcursor-1] == 32) break;
					osdeditcursor--;
				}
			}
			if (osdeditcursor<osdeditwinstart) {
				osdeditwinend-=(osdeditwinstart-osdeditcursor);
				osdeditwinstart-=(osdeditwinstart-osdeditcursor);
			}
			break;
		case OSDOP::RIGHT:
			if (osdeditcursor<osdeditlen) {
				osdeditcursor++;
			}
			if (osdeditcursor>=osdeditwinend) {
				osdeditwinstart+=(osdeditcursor-osdeditwinend);
				osdeditwinend+=(osdeditcursor-osdeditwinend);
			}
			break;
		case OSDOP::RIGHT_WORD:
			if (osdeditcursor<osdeditlen) {
				while (osdeditcursor<osdeditlen) {
					if (osdeditbuf[osdeditcursor] == 32) break;
					osdeditcursor++;
				}
				while (osdeditcursor<osdeditlen) {
					if (osdeditbuf[osdeditcursor] != 32) break;
					osdeditcursor++;
				}
			}
			if (osdeditcursor>=osdeditwinend) {
				osdeditwinstart+=(osdeditcursor-osdeditwinend);
				osdeditwinend+=(osdeditcursor-osdeditwinend);
			}
			break;
		case OSDOP::BACKSPACE:
			if (!osdeditcursor || !osdeditlen) return;
			if (!osdovertype) {
				if (osdeditcursor < osdeditlen)
					std::memmove(osdeditbuf+osdeditcursor-1, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
				osdeditlen--;
			}
			osdeditcursor--;
			if (osdeditcursor<osdeditwinstart) {
				osdeditwinstart--;
				osdeditwinend--;
			}
			break;
		case OSDOP::DELETE:
			if (osdeditcursor == osdeditlen || !osdeditlen) return;
			if (osdeditcursor <= osdeditlen-1) std::memmove(osdeditbuf+osdeditcursor, osdeditbuf+osdeditcursor+1, osdeditlen-osdeditcursor-1);
			osdeditlen--;
			break;
		case OSDOP::DELETE_START:
			if (osdeditcursor>0 && osdeditlen) {
				if (osdeditcursor<osdeditlen)
					std::memmove(osdeditbuf, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
				osdeditlen-=osdeditcursor;
				osdeditcursor = 0;
				osdeditwinstart = 0;
				osdeditwinend = editlinewidth;
			}
			break;
		case OSDOP::DELETE_END:
			osdeditlen = osdeditcursor;
			break;
		case OSDOP::DELETE_WORD:
			if (osdeditcursor>0 && osdeditlen>0) {
				i=osdeditcursor;
				while (i>0 && osdeditbuf[i-1]==32) i--;
				while (i>0 && osdeditbuf[i-1]!=32) i--;
				if (osdeditcursor<osdeditlen)
					std::memmove(osdeditbuf+i, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
				osdeditlen -= (osdeditcursor-i);
				osdeditcursor = i;
				if (osdeditcursor < osdeditwinstart) {
					osdeditwinstart=osdeditcursor;
					osdeditwinend=osdeditwinstart+editlinewidth;
				}
			}
			break;
		case OSDOP::CANCEL:
			osdeditlen=0;
			osdeditcursor=0;
			osdeditwinstart=0;
			osdeditwinend=editlinewidth;
			break;
		case OSDOP::COMPLETE:
			if (lastmatch == symbols.end()) {
				for (i=osdeditcursor;i>0;i--) if (osdeditbuf[i-1] == ' ') break;
				for (j=0;osdeditbuf[i] != ' ' && i < osdeditlen;j++,i++)
					osdedittmp[j] = osdeditbuf[i];
				osdedittmp[j] = 0;

				if (j > 0)
					tabc = findsymbol(osdedittmp, symbols.end());
			} else {
				tabc = findsymbol(osdedittmp, std::ranges::next(lastmatch));
				if ((tabc == symbols.end()) && (lastmatch != symbols.end()))
					tabc = findsymbol(osdedittmp, symbols.end());	// wrap
			}

			if (tabc != symbols.end()) {
				for (i=osdeditcursor;i>0;i--) if (osdeditbuf[i-1] == ' ') break;
				osdeditlen = i;
				for (j=0;tabc->name[j] && osdeditlen <= EDITLENGTH;i++,j++,osdeditlen++)
					osdeditbuf[i] = tabc->name[j];
				osdeditcursor = osdeditlen;
				osdeditwinend = osdeditcursor;
				osdeditwinstart = osdeditwinend-editlinewidth;
				if (osdeditwinstart<0) {
					osdeditwinstart=0;
					osdeditwinend = editlinewidth;
				}

				lastmatch = tabc;
			}
			break;
		case OSDOP::SUBMIT:
			if (osdeditlen>0) {
				osdeditbuf[osdeditlen] = 0;
				std::memmove(osdhistorybuf[1], osdhistorybuf[0], (HISTORYDEPTH-1)*(EDITLENGTH+1));
				std::memmove(osdhistorybuf[0], osdeditbuf, EDITLENGTH+1);
				if (osdhistorysize < HISTORYDEPTH) osdhistorysize++;
				if (osdexeccount == HISTORYDEPTH)
					OSD_Printf("Command Buffer Warning: Failed queueing command "
							"for execution. Buffer full.\n");
				else
					osdexeccount++;
				osdhistorypos=-1;
			}

			osdeditlen=0;
			osdeditcursor=0;
			osdeditwinstart=0;
			osdeditwinend=editlinewidth;
			break;
		case OSDOP::HISTORY_UP:
			if (osdhistorypos < osdhistorysize-1) {
				osdhistorypos++;
				std::memcpy(osdeditbuf, osdhistorybuf[osdhistorypos], EDITLENGTH+1);
				osdeditlen = osdeditcursor = 0;
				while (osdeditbuf[osdeditcursor]) {
					osdeditlen++;
					osdeditcursor++;
				}
				if (osdeditcursor<osdeditwinstart) {
					osdeditwinend = osdeditcursor;
					osdeditwinstart = osdeditwinend-editlinewidth;

					if (osdeditwinstart<0) {
						osdeditwinend-=osdeditwinstart;
						osdeditwinstart=0;
					}
				} else if (osdeditcursor>=osdeditwinend) {
					osdeditwinstart+=(osdeditcursor-osdeditwinend);
					osdeditwinend+=(osdeditcursor-osdeditwinend);
				}
			}
			break;
		case OSDOP::HISTORY_DOWN:
			if (osdhistorypos >= 0) {
				if (osdhistorypos == 0) {
					osdeditlen=0;
					osdeditcursor=0;
					osdeditwinstart=0;
					osdeditwinend=editlinewidth;
					osdhistorypos = -1;
				} else {
					osdhistorypos--;
					std::memcpy(osdeditbuf, osdhistorybuf[osdhistorypos], EDITLENGTH+1);
					osdeditlen = osdeditcursor = 0;
					while (osdeditbuf[osdeditcursor]) {
						osdeditlen++;
						osdeditcursor++;
					}
					if (osdeditcursor<osdeditwinstart) {
						osdeditwinend = osdeditcursor;
						osdeditwinstart = osdeditwinend-editlinewidth;

						if (osdeditwinstart<0) {
							osdeditwinend-=osdeditwinstart;
							osdeditwinstart=0;
						}
					} else if (osdeditcursor>=osdeditwinend) {
						osdeditwinstart+=(osdeditcursor-osdeditwinend);
						osdeditwinend+=(osdeditcursor-osdeditwinend);
					}
				}
			}
			break;
		case OSDOP::SCROLL_TOP:
			osdhead = osdlines-1;
			break;
		case OSDOP::SCROLL_BOTTOM:
			osdhead = 0;
			break;
		case OSDOP::PAGE_UP:
			if (osdhead < osdlines-1)
				osdhead++;
			break;
		case OSDOP::PAGE_DOWN:
			if (osdhead > 0)
				osdhead--;
			break;
	}
}

void OSD_InsertChar(int ch)
{
	if (!osdovertype && osdeditlen == EDITLENGTH)	// buffer full, can't insert another char
		return;

	if (!osdovertype) {
		if (osdeditcursor < osdeditlen)
			std::memmove(osdeditbuf+osdeditcursor+1, osdeditbuf+osdeditcursor, osdeditlen-osdeditcursor);
		osdeditlen++;
	} else {
		if (osdeditcursor == osdeditlen)
			osdeditlen++;
	}
	osdeditbuf[osdeditcursor] = ch;
	osdeditcursor++;
	if (osdeditcursor>osdeditwinend) {
		osdeditwinstart++;
		osdeditwinend++;
	}
}

} // namespace

//
// OSD_HandleChar() -- Handles keyboard character input when capturing input.
// 	Returns 0 if the key was handled internally, or the character if it should
// 	be passed on to the game.
//
int OSD_HandleChar(int ch)
{
	if (!osdinited || !osdvisible)
		return ch;

	if (ch < 32 || ch == 127) {
		switch (ch) {
			case 1:		// control a. jump to beginning of line
				OSD_Manipulate(OSDOP::START); break;
			case 2:		// control b, move one character left
				OSD_Manipulate(OSDOP::LEFT); break;
			case 3:		// control c, cancel
				OSD_Manipulate(OSDOP::CANCEL); break;
			case 5:		// control e, jump to end of line
				OSD_Manipulate(OSDOP::END); break;
			case 6:		// control f, move one character right
				OSD_Manipulate(OSDOP::RIGHT); break;
			case 8:
			case 127:	// control h, backspace
				OSD_Manipulate(OSDOP::BACKSPACE); break;
			case 9:		// tab
				OSD_Manipulate(OSDOP::COMPLETE); break;
			case 11:	// control k, delete all to end of line
				OSD_Manipulate(OSDOP::DELETE_END); break;
			case 12:	// control l, clear screen
				break;
			case 13:	// control m, enter
				OSD_Manipulate(OSDOP::SUBMIT); break;
			case 16:	// control p, previous (ie. up arrow)
				OSD_Manipulate(OSDOP::HISTORY_UP); break;
				break;
			case 20:	// control t, swap previous two chars
				break;
			case 21:	// control u, delete all to beginning
				OSD_Manipulate(OSDOP::DELETE_START); break;
			case 23:	// control w, delete one word back
				OSD_Manipulate(OSDOP::DELETE_WORD); break;
		}
	} else {	// text char
		OSD_InsertChar(ch);
	}

	return 0;
}

//
// OSD_HandleKey() -- Handles keyboard input when capturing input.
// 	Returns 0 if the key was handled internally, or the scancode if it should
// 	be passed on to the game.
//
int OSD_HandleKey(int sc, int press)
{
	if (!osdinited || !osdvisible)
		return sc;

	if (!press) {
		if (sc == 42 || sc == 54) // shift
			osdeditshift = 0;
		if (sc == 29 || sc == 157)	// control
			osdeditcontrol = 0;
		if (sc == 56 || sc == 184)	// alt
			osdeditalt = 0;
		return 0;
	}

	keytime = gettime();

	if (sc != 15)
		lastmatch = {};		// reset tab-completion cycle

	switch (sc) {
		case 1:		// escape
			OSD_ShowDisplay(0); break;
		case 211:	// delete
			OSD_Manipulate(OSDOP::DELETE); break;
		case 199:	// home
			if (osdeditcontrol) {
				OSD_Manipulate(OSDOP::SCROLL_TOP);
			} else {
				OSD_Manipulate(OSDOP::START);
			}
			break;
		case 207:	// end
			if (osdeditcontrol) {
				OSD_Manipulate(OSDOP::SCROLL_BOTTOM);
			} else {
				OSD_Manipulate(OSDOP::END);
			}
			break;
		case 201:	// page up
			OSD_Manipulate(OSDOP::PAGE_UP); break;
		case 209:	// page down
			OSD_Manipulate(OSDOP::PAGE_DOWN); break;
		case 200:	// up
			OSD_Manipulate(OSDOP::HISTORY_UP); break;
		case 208:	// down
			OSD_Manipulate(OSDOP::HISTORY_DOWN); break;
		case 203:	// left
			if (osdeditcontrol) {
				OSD_Manipulate(OSDOP::LEFT_WORD);
			} else {
				OSD_Manipulate(OSDOP::LEFT);
			}
			break;
		case 205:	// right
			if (osdeditcontrol) {
				OSD_Manipulate(OSDOP::RIGHT_WORD);
			} else {
				OSD_Manipulate(OSDOP::RIGHT);
			}
			break;
		case 33:	// f
			if (osdeditalt) {
				OSD_Manipulate(OSDOP::RIGHT_WORD);
			}
			break;
		case 48:	// b
			if (osdeditalt) {
				OSD_Manipulate(OSDOP::LEFT_WORD);
			}
			break;
		case 210:	// insert
			osdovertype ^= 1; break;
		case 58:	// capslock
			osdeditcaps ^= 1; break;
		case 42:
		case 54:	// shift
			osdeditshift = 1; break;
		case 29:
		case 157:	// control
			osdeditcontrol = 1; break;
		case 56:
		case 184:	// alt
			osdeditalt = 1; break;
	}

	return 0;
}


//
// OSD_ResizeDisplay() -- Handles readjustment of the display when the screen resolution
// 	changes on us.
//
// TODO: Move memory to heap.
void OSD_ResizeDisplay(int w, int h)
{
	const int newcols = getcolumnwidth(w);
	const int newmaxlines = TEXTSIZE / newcols;

	const int j = std::min(newmaxlines, osdmaxlines);
	const int k = std::min(newcols, osdcols);

	std::vector<char> newtext;
	newtext.resize(TEXTSIZE);

	for(int i{0}; i < j; ++i) {
		std::memcpy(&newtext[0] + newcols * i, &osdtext[0] + osdcols * i, k);
	}

	std::memcpy(&osdtext[0], &newtext[0], TEXTSIZE);

	osdcols = newcols;
	osdmaxlines = newmaxlines;
	osdmaxrows = getrowheight(h) - 2;

	if (osdrows > osdmaxrows) {
		osdrows = osdmaxrows;
	}

	osdpos = 0;
	osdhead = 0;
	osdeditwinstart = 0;
	osdeditwinend = editlinewidth;

	if (osdvisible) {
		findwhite();
	}
}


//
// OSD_ShowDisplay() -- Shows or hides the onscreen display
//
void OSD_ShowDisplay(int onf)
{
	if (onf < 0) {
		onf = !osdvisible;
	}

	osdvisible = (onf != 0);
	osdeditcontrol = 0;
	osdeditshift = 0;

	grabmouse(osdvisible == 0);
	onshowosd(osdvisible);
	releaseallbuttons();
	bflushchars();

	if (osdvisible) {
		findwhite();
	}
}


//
// OSD_Draw() -- Draw the onscreen display
//
void OSD_Draw()
{
	if (!osdvisible || !osdinited) {
		return;
	}

	unsigned int topoffs = osdhead * osdcols;
	int row = osdrows - 1;
	int lines = std::min(osdlines - osdhead, osdrows);

	clearbackground(osdcols, osdrows + 1);

	for (; lines > 0; --lines, --row) {
		drawosdstr(0, row, &osdtext[0] + topoffs, osdcols, osdtextshade, osdtextpal);
		topoffs += osdcols;
	}

	drawosdchar(2, osdrows, '>', osdpromptshade, osdpromptpal);
	if (osdeditcaps)
		drawosdchar(0, osdrows, 'C', osdpromptshade, osdpromptpal);

	const int len = std::min(osdcols - 1 - 3, osdeditlen - osdeditwinstart);
	for (int x{0}; x < len; ++x)
		drawosdchar(3+x,osdrows,osdeditbuf[osdeditwinstart+x],osdeditshade,osdeditpal);

	drawosdcursor(3+osdeditcursor-osdeditwinstart,osdrows,osdovertype,keytime);
}

namespace {

inline void linefeed()
{
	std::memmove(&osdtext[0] + osdcols, &osdtext[0], TEXTSIZE - osdcols);
	std::memset(&osdtext[0], 0, osdcols);

	if (osdlines < osdmaxlines)
		osdlines++;
}

} // namespace

//
// OSD_Printf() -- Print a formatted string to the onscreen display
//   and write it to the log file
//

void OSD_Printf(const char *fmt, ...)
{
	if (!osdinited)
		return;

	char tmpstr[1024];
	va_list va;

	va_start(va, fmt);
	vsnprintf(tmpstr, sizeof(tmpstr), fmt, va);
	va_end(va);

	OSD_Puts(tmpstr);
}

//
// OSD_Puts() -- Print a string to the onscreen display
//   and write it to the log file
//

void OSD_Puts(std::string_view strv)
{
	if (!osdinited)
		return;

	for (auto chp : strv) {
		if (chp == '\r')
			osdpos = 0;
		else if (chp == '\n') {
			osdpos = 0;
			linefeed();
		} else {
			osdtext[osdpos++] = chp;
			if (osdpos == osdcols) {
				osdpos = 0;
				linefeed();
			}
		}
	}
}


//
// OSD_DispatchQueued() -- Executes any commands queued in the buffer
//
void OSD_DispatchQueued()
{
	if (!osdexeccount)
		return;

	int cmd = osdexeccount - 1;
	osdexeccount = 0;

	for (; cmd>=0; cmd--) {
		OSD_Dispatch((const char *) osdhistorybuf[cmd]);
	}
}


namespace {

char *strtoken(char *s, char **ptrptr, int *restart)
{
	char *p;
	char *p2;
	char *start;

	*restart = 0;
	if (!ptrptr) return nullptr;

	// if s != nullptr, we process from the start of s, otherwise
	// we just continue with where ptrptr points to
	if (s) p = s;
	else p = *ptrptr;

	if (!p) return nullptr;

	// eat up any leading whitespace
	while (*p != 0 && *p != ';' && *p == ' ') p++;

	// a semicolon is an end of statement delimiter like a \0 is, so we signal
	// the caller to 'restart' for the rest of the string pointed at by *ptrptr
	if (*p == ';') {
		*restart = 1;
		*ptrptr = p+1;
		return nullptr;
	}
	// or if we hit the end of the input, signal all done by nulling *ptrptr
	else if (*p == 0) {
		*ptrptr = nullptr;
		return nullptr;
	}

	if (*p == '\"') {
		// quoted string
		start = ++p;
		p2 = p;
		while (*p != 0) {
			if (*p == '\"') {
				p++;
				break;
			} else if (*p == '\\') {
				switch (*(++p)) {
					case 'n': *p2 = '\n'; break;
					case 'r': *p2 = '\r'; break;
					default: *p2 = *p; break;
				}
			} else {
				*p2 = *p;
			}
			p2++, p++;
		}
		*p2 = 0;
	} else {
		start = p;
		while (*p != 0 && *p != ';' && *p != ' ') p++;
	}

	// if we hit the end of input, signal all done by nulling *ptrptr
	if (*p == 0) {
		*ptrptr = nullptr;
	}
	// or if we came upon a semicolon, signal caller to restart with the
	// string at *ptrptr
	else if (*p == ';') {
		*p = 0;
		*ptrptr = p+1;
		*restart = 1;
	}
	// otherwise, clip off the token and carry on
	else {
		*(p++) = 0;
		*ptrptr = p;
	}

	return start;
}

} // namespace

//
// OSD_Dispatch() -- Executes a command string
//

int OSD_Dispatch(const char *cmd)
{
	char* state = Bstrdup(cmd);
	char* workbuf = state;

	if (!workbuf)
		return -1;

	int  restart{0};
	std::vector<std::string> parms;
	char* wtp{nullptr};
	do {
		char* wp = strtoken(state, &wtp, &restart);
		if (!wp) {
			state = wtp;
			continue;
		}

		const auto symb = findexactsymbol(wp);
		if (symb == symbols.end()) {
			OSD_Printf("Error: \"%s\" is not defined\n", wp);
			std::free(workbuf);
			return -1;
		}

		osdfuncparm_t ofp;

		ofp.name = wp;
		while (wtp && !restart) {
			wp = strtoken(nullptr, &wtp, &restart);
			if (wp)
				parms.push_back(wp);
		}
		ofp.parms    = parms;
		ofp.raw      = cmd;
		switch (symb->func(&ofp)) {
			case OSDCMD_OK: break;
			case OSDCMD_SHOWHELP: OSD_Printf("%s\n", symb->help); break;
		}

		state = wtp;
	} while (wtp && restart);

	std::free(workbuf);

	return 0;
}


//
// OSD_RegisterFunction() -- Registers a new function
//
int OSD_RegisterFunction(std::string_view name, std::string_view help, int (*func)(const osdfuncparm_t*))
{
	if (!osdinited) {
        buildputs("OSD_RegisterFunction(): OSD not initialised\n");
        return -1;
    }

	if (name.empty()) {
		buildputs("OSD_RegisterFunction(): may not register a function with a null name\n");
		return -1;
	}
	if (!name[0]) {
		buildputs("OSD_RegisterFunction(): may not register a function with no name\n");
		return -1;
	}

	// check for illegal characters in name
	for (const auto* cp = name.data(); *cp; cp++) {
		if ((cp == name) && (*cp >= '0') && (*cp <= '9')) {
			buildprintf("OSD_RegisterFunction(): first character of function name \"{}\" must not be a numeral\n", name);
			return -1;
		}
		if ((*cp < '0') ||
		    (*cp > '9' && *cp < 'A') ||
		    (*cp > 'Z' && *cp < 'a' && *cp != '_') ||
		    (*cp > 'z')) {
			buildprintf("OSD_RegisterFunction(): illegal character in function name \"{}\"\n", name);
			return -1;
		}
	}

	if (help.empty())
		help = "(no description for this function)";
	if (!func) {
		buildputs("OSD_RegisterFunction(): may not register a null function\n");
		return -1;
	}

	auto symb = findexactsymbol(name);
	if ((symb != symbols.end()) && symb->func == func) {
		// Same function being defined a second time, so we'll quietly ignore it.
		return 0;
	} else if (symb != symbols.end()) {
		buildprintf("OSD_RegisterFunction(): \"{}\" is already defined\n", name);
		return -1;
	}

	symb = addnewsymbol(name);
	if (symb == symbols.end()) {
		buildprintf("OSD_RegisterFunction(): Failed registering function \"{}\"\n", name);
		return -1;
	}

	symb->name = name;
	symb->help = help;
	symb->func = func;

	return 0;
}


namespace {

//
// addnewsymbol() -- Allocates space for a new symbol and attaches it
//   appropriately to the lists, sorted.
//
symbol_iter addnewsymbol(std::string_view name)
{
	if(symbols.empty()) {
		symbols.push_back(symbol_t{});
		return symbols.begin();
	}
	else {
		const auto syminsertion = std::ranges::find_if(symbols, [name](const auto& symname) {
			return CmpNoCase(symname, name) < 0; }, &symbol_t::name);

		return symbols.insert(syminsertion, symbol_t{});
	}
}


//
// findsymbol() -- Finds a symbol, possibly partially named;
//                 starting at "startingat" in the symbols vector.
//
symbol_iter findsymbol(std::string_view name, symbol_iter startingat)
{
	std::ranges::subrange symrange = {startingat, symbols.end()};
	return std::ranges::find_if(symrange, [name](const auto& startname) {
		return !CmpNoCaseN(name.data(), startname.c_str(), name.length()); }, &symbol_t::name);
}


//
// findexactsymbol() -- Finds a symbol, complete named
//
symbol_iter findexactsymbol(std::string_view name)
{
	return std::ranges::find(symbols, name, &symbol_t::name);
}

} // namespace
