// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "build.hpp"
#include "engine_priv.hpp"
#include "pragmas.hpp"
#include "osd.hpp"
#include "cache1d.hpp"
#include "editor.hpp"
#include "string_utils.hpp"
#include "version.hpp"

#include "baselayer.hpp"
#ifdef RENDERTYPEWIN
#include "winlayer.hpp"
#endif

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <numeric>
#include <string_view>
#include <utility>

constexpr auto TIMERINTSPERSECOND{120};

#define updatecrc16(crc,dat) (crc = (((crc<<8)&65535)^crctable[((((unsigned short)crc)>>8)&65535)^dat]))

int vel;
int svel;
int angvel;

std::array<int, NUMBUILDKEYS> buildkeys = {
	0xc8,0xd0,0xcb,0xcd,0x2a,0x9d,0x1d,0x39,
	0x1e,0x2c,0xd1,0xc9,0x33,0x34,
	0x9c,0x1c,0xd,0xc,0xf,0x45
};

int posx;
int posy;
int posz;
int horiz{100};
int mousexsurp{0};
int mouseysurp{0};
short ang;
short cursectnum;
int hvel;

int grponlymode{0};

int ytop16;
int ydimgame{480};
int bppgame{8};
int forcesetup{1};

int zlock{0x7fffffff};
int zmode{0};
int whitecol;
int blackcol;
int kensplayerheight{32};
int kenswalldist{128};
short defaultspritecstat{0};

std::array<unsigned char, 4096> tempbuf;

char names[MAXTILES][25];

bool asksave{false};
extern short searchwall;

short pointhighlight{-1};
extern short linehighlight;
extern short highlightcnt;
short grid{3};
short gridlock{1};
bool showtags{true};
short showspriteextents{0};
int zoom{768};
int gettilezoom{1};

int numsprites;

std::array<short, MAXWALLS> highlight;
std::array<short, MAXSECTORS> highlightsector;
short highlightsectorcnt{-1};

short temppicnum;
short tempcstat;
short templotag;
short temphitag;
short tempextra;
unsigned char tempshade;
unsigned char temppal;
unsigned char tempvis;
unsigned char tempxrepeat;
unsigned char tempyrepeat;
unsigned char somethingintab{255};

namespace {

std::array<int, 256> crctable;
std::string kensig;

int bakydim16;
int bakytop16;

int synctics{0};
int lockclock{0};
short oldmousebstatus{0};

std::array<short, MAXTILES> localartfreq;
std::array<short, MAXTILES> localartlookup;
short localartlookupnum;

std::array<unsigned char, MAXSECTORS> pskysearch;
std::string boardfilename;
std::string selectedboardfilename;
CACHE1D_FIND_REC* finddirs{nullptr};
CACHE1D_FIND_REC* findfiles{nullptr};
CACHE1D_FIND_REC* finddirshigh{nullptr};
CACHE1D_FIND_REC* findfileshigh{nullptr};
int numdirs{0};
int numfiles{0};
int currentlist{0};

int repeatcountx;
int repeatcounty;

std::array<int, 640> fillist;

} // namespace

enum class ClockDir_t {
	CW, // clockwise
	CCW // counter-clockwise
};

void qsetmodeany(int,int);
void clear2dscreen();
void draw2dgrid(int posxe, int posye, short ange, int zoome, short gride);
void draw2dscreen(int posxe, int posye, short ange, int zoome, short gride);

unsigned char changechar(unsigned char dachar, int dadir, unsigned char smooshyalign, bool boundcheck);
void adjustmark(int *xplc, int *yplc, short danumwalls);
bool checkautoinsert(int dax, int day, short danumwalls);
void keytimerstuff();
ClockDir_t clockdir(short wallstart);
void flipwalls(short numwalls, short newnumwalls);
void insertpoint(short linehighlight, int dax, int day);
void deletepoint(short point);
void deletesector(short sucksect);
void checksectorpointer(short i, short sectnum);
void fixrepeats(short i);
short loopinside(int x, int y, short startwall);
void fillsector(short sectnum, unsigned char fillcolor);
short whitelinescan(short dalinehighlight);
void printcoords16(int posxe, int posye, short ange);
void copysector(short soursector, short destsector, short deststartwall, bool copystat);
void showsectordata(short sectnum);
void showwalldata(short wallnum);
void showspritedata(short spritenum);
void drawtilescreen(int pictopleft, int picbox);
void overheadeditor();
int getlinehighlight(int xplc, int yplc);
void fixspritesectors();
void movewalls(int start, int offs);
int loadnames();
void updatenumsprites();
void getclosestpointonwall(int x, int y, int dawall, int *nx, int *ny);
void initcrc();
void AutoAlignWalls(int nWall0, int ply);
int gettile(int tilenum);

std::string findfilename(const std::string& path);
int menuselect(int newpathmode);
void getfilenames(const std::string& path, const std::string& kind);
void clearfilenames();

void clearkeys() {
	std::ranges::fill(keystatus, 0);
}

extern int qsetmode;

namespace {

int osdcmd_restartvid(const osdfuncparm_t *parm)
{
	std::ignore = parm;

	if (qsetmode != 200) {
		return OSDCMD_OK;
	}

	resetvideomode();
	if (setgamemode(fullscreen, xdim, ydim, bpp)) {
		buildputs("restartvid: Reset failed...\n");
	}

	return OSDCMD_OK;
}

int osdcmd_vidmode(const osdfuncparm_t *parm)
{
	int newx{ xdim };
	int newy{ ydim };
	int newbpp{ bpp };
	bool newfullscreen{ fullscreen };

	if (qsetmode != 200) {
		return OSDCMD_OK;
	}

	if (parm->parms.size() < 1 || parm->parms.size() > 4) {
		return OSDCMD_SHOWHELP;
	}

	if (parm->parms.size() == 4) {
		// fs, res, bpp switch
		int tmpval{0};
		const std::string_view parmv{parm->parms[3]};
		std::from_chars(parmv.data(), parmv.data() + parmv.size(), tmpval);
		// TODO: Use return result here?
		newfullscreen = (tmpval != 0);
	}
	if (parm->parms.size() >= 3) {
		// res & bpp switch
		const std::string_view parmv{parm->parms[2]};
		std::from_chars(parmv.data(), parmv.data() + parmv.size(), newbpp);
		// FIXME: Use return result here?
	}
	if (parm->parms.size() >= 2) {
		// res switch
		const std::string_view parmy{parm->parms[1]};
		const std::string_view parmx{parm->parms[0]};
		std::from_chars(parmy.data(), parmy.data() + parmy.size(), newy);
		std::from_chars(parmx.data(), parmx.data() + parmx.size(), newx);
		// TODO: Use return results here?
	}
	if (parm->parms.size() == 1) {
		// bpp switch
		const std::string_view parmv{parm->parms[0]};
		std::from_chars(parmv.data(), parmv.data() + parmv.size(), newbpp);
		// TODO: Use return results here?
	}

	if (setgamemode(newfullscreen, newx, newy, newbpp)) {
		buildputs("vidmode: Mode change failed!\n");
	}

	xdimgame = newx;
	ydimgame = newy;
	bppgame = newbpp;
	fullscreen = newfullscreen;

	return OSDCMD_OK;
}

int osdcmd_mapversion(const osdfuncparm_t *parm)
{
	if (parm->parms.size() < 1) {
		buildprintf("mapversion is {}\n", mapversion);
		return OSDCMD_OK;
	}
	
	const std::string_view parmv{parm->parms[0]};
	int newversion{0};
	auto [ptr, ec] = std::from_chars(parmv.data(), parmv.data() + parmv.size(), newversion);

	if (newversion < 5 || newversion > 8 || (ec != std::errc{})) {
		return OSDCMD_SHOWHELP;
	}

	buildprintf("mapversion is now {} (was {})\n", newversion, mapversion);
	mapversion = newversion;

	return OSDCMD_OK;
}

int osdcmd_showspriteextents(const osdfuncparm_t *parm)
{
	if (parm->parms.size() != 1) {
		buildprintf("showspriteextents is {}\n", showspriteextents);
		return OSDCMD_OK;
	}
	
	const std::string_view parmv{parm->parms[0]};
	int newval{0};
	auto [ptr, ec] = std::from_chars(parmv.data(), parmv.data() + parmv.size(), newval);

	if (newval < 0 || newval > 2 || (ec != std::errc{})) {
		return OSDCMD_SHOWHELP;
	}

	buildprintf("showspriteextents is now {} (was {})\n", newval, showspriteextents);
	showspriteextents = newval;
	return OSDCMD_OK;
}

} // namespace

#if defined RENDERTYPEWIN || (defined RENDERTYPESDL && (defined __APPLE__ || defined HAVE_GTK))
# define HAVE_STARTWIN
#endif

static constexpr char defsfilename[] = "kenbuild.def";

int app_main(int argc, char const * const argv[])
{
	int grpstoadd{ 0 };
	char const** grps{ nullptr };
	int i;
	int j;

#ifdef HAVE_STARTWIN
	char cmdsetup = 0;
    struct startwin_settings settings;
    int startretval = STARTWIN_RUN;
#endif

	pathsearchmode = PATHSEARCH_SYSTEM;		// unrestrict findfrompath so that full access to the filesystem can be had

	OSD_RegisterFunction("restartvid","restartvid: reinitialise the video mode",osdcmd_restartvid);
	OSD_RegisterFunction("vidmode","vidmode [xdim ydim] [bpp] [fullscreen]: immediately change the video mode",osdcmd_vidmode);
	OSD_RegisterFunction("mapversion","mapversion [ver]: change the map version for save (min 5, max 8)", osdcmd_mapversion);
	OSD_RegisterFunction("showspriteextents","showspriteextents [state]: show floor/wall sprite extents and "
		"walldist clipping boundary (0 = off, 1 = extents, 2 = +clip)", osdcmd_showspriteextents);

	wm_setapptitle("BUILD by Ken Silverman");

#ifdef RENDERTYPEWIN
	win_allowbackgroundidle(1);
#endif

	editstatus = true;

	for (i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			if (IsSameAsNoCase(argv[i], "-g") || IsSameAsNoCase(argv[i], "-grp")) {
				i++;
				if (grpstoadd == 0) grps = (char const **)std::malloc(sizeof(char const *) * argc);
				grps[grpstoadd++] = argv[i];
			}
			else if (IsSameAsNoCase(argv[i], "-help") || IsSameAsNoCase(argv[i], "--help") || IsSameAsNoCase(argv[i], "-?")) {
				std::string_view s =
					"BUILD by Ken Silverman\n"
					"Syntax: build [options] mapname\n"
					"Options:\n"
					"\t-grp name.ext\tUse an extra GRP or ZIP file.\n"
					"\t-g name.ext\tSame as above.\n"
#ifdef HAVE_STARTWIN
					"\t-setup\tDisplays the configuration dialogue box before entering the editor.\n"
#endif
					;
#ifdef HAVE_STARTWIN
				wm_msgbox("BUILD by Ken Silverman","{}",s);
#else
				fmt::print("{}\n", s);
#endif
				return 0;
			}
#ifdef HAVE_STARTWIN
			else if (IsSameAsNoCase(argv[i], "-setup")) cmdsetup = 1;
#endif
			continue;
		}
		if (boardfilename.empty()) {
			boardfilename = argv[i];
		}
	}
	if (boardfilename.empty()) {
		boardfilename = "newboard.map";
	} else if (!boardfilename.ends_with(".map")) {
		boardfilename.append(".map");
	}
	//Bcanonicalisefilename(boardfilename,0);

	if ((i = ExtInit()) < 0) return -1;

#ifdef HAVE_STARTWIN
    std::memset(&settings, 0, sizeof(settings));
    settings.fullscreen = fullscreen;
    settings.xdim2d = xdim2d;
    settings.ydim2d = ydim2d;
    settings.xdim3d = xdimgame;
    settings.ydim3d = ydimgame;
    settings.bpp3d = bppgame;
    settings.forcesetup = forcesetup;

    if (i || forcesetup || cmdsetup) {
        if (quitevent) return 0;

        startretval = startwin_run(&settings);
        if (startretval == STARTWIN_CANCEL)
            return 0;
    }

    fullscreen = settings.fullscreen;
    xdim2d = settings.xdim2d;
    ydim2d = settings.ydim2d;
    xdimgame = settings.xdim3d;
    ydimgame = settings.ydim3d;
    bppgame = settings.bpp3d;
    forcesetup = settings.forcesetup;
#endif

	if (grps && grpstoadd > 0) {
		for (i=0;i<grpstoadd;i++) {
			buildprintf("Adding {}\n",grps[i]);
			initgroupfile(grps[i]);
		}
		std::free((void *)grps);
	}

	buildsetlogfile("build.log");

	inittimer(TIMERINTSPERSECOND, keytimerstuff);
	initinput();
	initmouse();

	loadpics("tiles000.art",1048576);
	loadnames();

	kensig = "BUILD by Ken Silverman";
	initcrc(); 

	if (!loaddefinitionsfile(defsfilename)) buildputs("Definitions file loaded.\n");

	if (setgamemode(fullscreen,xdimgame,ydimgame,bppgame) < 0)
	{
		ExtUnInit();
		uninitengine();
		buildprintf("{} * {} not supported in this graphics mode\n",xdim,ydim);
		std::exit(0);
	}

	setbrightness(brightness, palette, 0);

	int dark = std::numeric_limits<int>::max();
	int light = 0;
	for(i=0;i<256;i++)
	{
		j = ((int)palette[i*3])+((int)palette[i*3+1])+((int)palette[i*3+2]);
		if (j > light) { light = j; whitecol = i; }
		if (j < dark) { dark = j; blackcol = i; }
	}

	for(auto& aWall : wall) {
		aWall.extra = -1; 
	}

	for(auto& aSprite : sprite) {
		aSprite.extra = -1;
	}

	wm_setwindowtitle("(new board)");

	ExtPreLoadMap();
	j = pathsearchmode == PATHSEARCH_GAME && grponlymode ? KOPEN4LOAD_ANYGRP : KOPEN4LOAD_ANY;
	i = loadboard(boardfilename.data(), j, &posx, &posy, &posz, &ang, &cursectnum);
	if (i == -2) i = loadoldboard(boardfilename.data(), j, &posx, &posy, &posz, &ang, &cursectnum);
	if (i < 0)
	{
		initspritelists();
		posx = 32768;
		posy = 32768;
		posz = 0;
		ang = 1536;
		numsectors = 0;
		numwalls = 0;
		cursectnum = -1;
		overheadeditor();
		keystatus[buildkeys[14]] = 0;
	}
	else
	{
		ExtLoadMap(boardfilename);
	}

	updatenumsprites();

	startposx = posx;
	startposy = posy;
	startposz = posz;
	startang = ang;
	startsectnum = cursectnum;

	totalclock = 0;

	bool quitflag{false};
	while (!quitflag)
	{
		if (handleevents()) {
			if (quitevent) {
				keystatus[1] = 1;
				quitevent = false;
			}
		}

		OSD_DispatchQueued();

		ExtPreCheckKeys();

		drawrooms(posx,posy,posz,ang,horiz,cursectnum);
		ExtAnalyzeSprites();
		drawmasks();

		ExtCheckKeys();

		nextpage();
		synctics = totalclock-lockclock;
		lockclock += synctics;

		if (keystatus[1] > 0)
		{
			keystatus[1] = 0;
			printmessage256("Really want to quit?");
			showframe();

			synctics = totalclock-lockclock;
			lockclock += synctics;

			while ((keystatus[1]|keystatus[0x1c]|keystatus[0x39]|keystatus[0x31]) == 0)
			{
				if (handleevents()) {
					if (quitevent) {
						quitflag = true;
						break;
					}
				}

				if (keystatus[0x15] != 0) {
					keystatus[0x15] = 0;
					quitflag = true;
					break;
				}
			}
		}
	}

	if (asksave)
	{
		printmessage256("Save changes?");
		showframe();

		while ((keystatus[1]|keystatus[0x1c]|keystatus[0x39]|keystatus[0x31]) == 0)
		{
			if (handleevents()) if (quitevent) break;	// like saying no

			if (keystatus[0x15] != 0) {
				int bad;

				keystatus[0x15] = 0;

				std::string filename{boardfilename};
				if (pathsearchmode == PATHSEARCH_GAME) {
					filename = findfilename(filename);
				}

				fixspritesectors();   //Do this before saving!
				updatesector(startposx,startposy,&startsectnum);
				ExtPreSaveMap();
				if (mapversion < 7) {
					bad = saveoldboard(filename.c_str(), &startposx, &startposy, &startposz, &startang, &startsectnum);
				} else {
					bad = saveboard(filename,&startposx,&startposy,&startposz,&startang,&startsectnum);
				}
				if (!bad) {
					ExtSaveMap(filename.c_str());
				}
				break;
			}
		}
	}

	clearfilenames();
	ExtUnInit();
	uninitengine();

	buildprintf("Memory status: {}({}) bytes\n", cachesize, artsize);
	buildprintf("{}\n", kensig);
	return(0);
}

void showmouse()
{
	drawline256((searchx+1)<<12, (searchy  )<<12, (searchx+5)<<12, (searchy  )<<12, whitecol);
	drawline256((searchx  )<<12, (searchy+1)<<12, (searchx  )<<12, (searchy+5)<<12, whitecol);
	drawline256((searchx-1)<<12, (searchy  )<<12, (searchx-5)<<12, (searchy  )<<12, whitecol);
	drawline256((searchx  )<<12, (searchy-1)<<12, (searchx  )<<12, (searchy-5)<<12, whitecol);
}

void setoverheadviewport()
{
	bakydim16 = ydim16;
	bakytop16 = ytop16;
	ydim16 = yres - STATUS2DSIZ;
	ytop16 = 0;
}

void setstatusbarviewport()
{
	bakydim16 = ydim16;
	bakytop16 = ytop16;
	ydim16 = STATUS2DSIZ;
	ytop16 = yres - STATUS2DSIZ;
}

void restoreviewport()
{
	ydim16 = bakydim16;
	ytop16 = bakytop16;
}

void editinput()
{
	unsigned char smooshyalign;
	unsigned char repeatpanalign;
	unsigned char buffer[80];
	short startwall;
	short endwall;
	short dasector;
	short daang;
	int mousx;
	int mousy;
	int mousz;
	int bstatus;
	int i;
	int j;
	int k;
	int templong=0;
	int doubvel;
	int changedir;
	int dashade[2];
	int goalz;
	int xvect;
	int yvect;
	int hiz;
	int loz;
	short hitsect;
	short hitwall;
	short hitsprite;
	int hitx;
	int hity;
	int hitz;
	int dax;
	int day;
	int hihit;
	int lohit;

	if (keystatus[0x57] > 0)  //F11 - brightness
	{
		keystatus[0x57] = 0;
		brightness++;
		if (brightness >= 16) brightness = 0;
		setbrightness(brightness, palette, 0);
	}
	if (keystatus[88] > 0)   //F12
	{
		screencapture("captxxxx.tga",keystatus[0x2a]|keystatus[0x36]);
		keystatus[88] = 0;
	}

	mousz = 0;
	getmousevalues(&mousx,&mousy,&bstatus);
	{
	  const div_t ldx = div(mulscalen<16>(mousx<<16, msens) + mousexsurp, (1<<16)); mousx = ldx.quot; mousexsurp = ldx.rem;
	  const div_t ldy = div(mulscalen<16>(mousy<<16, msens) + mouseysurp, (1<<16)); mousy = ldy.quot; mouseysurp = ldy.rem;
	}
	searchx += mousx;
	searchy += mousy;
	if (searchx < 4) searchx = 4;
	if (searchy < 4) searchy = 4;
	if (searchx > xdim-5) searchx = xdim-5;
	if (searchy > ydim-5) searchy = ydim-5;
	showmouse();

	if (keystatus[0x3b] > 0) posx--;
	if (keystatus[0x3c] > 0) posx++;
	if (keystatus[0x3d] > 0) posy--;
	if (keystatus[0x3e] > 0) posy++;
	if (keystatus[0x43] > 0) ang--;
	if (keystatus[0x44] > 0) ang++;

	if (angvel != 0)          //ang += angvel * constant
	{                         //ENGINE calculates angvel for you
		doubvel = synctics;
		if (keystatus[buildkeys[4]] > 0)  //Lt. shift makes turn velocity 50% faster
			doubvel += (synctics>>1);
		ang += ((angvel*doubvel)>>4);
		ang = (ang+2048)&2047;
	}
	if ((vel|svel) != 0)
	{
		doubvel = synctics;
		if (keystatus[buildkeys[4]] > 0)     //Lt. shift doubles forward velocity
			doubvel += synctics;
		xvect = 0, yvect = 0;
		if (vel != 0)
		{
			xvect += ((vel*doubvel*(int)sintable[(ang+2560)&2047])>>3);
			yvect += ((vel*doubvel*(int)sintable[(ang+2048)&2047])>>3);
		}
		if (svel != 0)
		{
			xvect += ((svel*doubvel*(int)sintable[(ang+2048)&2047])>>3);
			yvect += ((svel*doubvel*(int)sintable[(ang+1536)&2047])>>3);
		}
		clipmove(&posx,&posy,&posz,&cursectnum,xvect,yvect,kenswalldist,4L<<8,4L<<8,CLIPMASK0);
	}
	getzrange(posx,posy,posz,cursectnum,&hiz,&hihit,&loz,&lohit,kenswalldist,CLIPMASK0);

	if (keystatus[0x3a] > 0)
	{
		zmode++;
		if (zmode == 3) zmode = 0;
		if (zmode == 1) zlock = (loz-posz)&0xfffffc00;
		keystatus[0x3a] = 0;
	}

	if (zmode == 0)
	{
		goalz = loz-(kensplayerheight<<8);   //playerheight pixels above floor
		if (goalz < hiz+(16<<8))   //ceiling&floor too close
			goalz = ((loz+hiz)>>1);
		goalz += mousz;
		if (keystatus[buildkeys[8]] > 0)                            //A (stand high)
		{
			if (keystatus[0x1d] > 0)
				horiz = std::max(-100, horiz - ((keystatus[buildkeys[4]] + 1) * synctics * 2));
			else
			{
				goalz -= (16<<8);
				if (keystatus[buildkeys[4]] > 0)    //Either shift key
					goalz -= (24<<8);
			}
		}
		if (keystatus[buildkeys[9]] > 0)                            //Z (stand low)
		{
			if (keystatus[0x1d] > 0)
				horiz = std::min(300, horiz + ((keystatus[buildkeys[4]] + 1) * synctics * 2));
			else
			{
				goalz += (12<<8);
				if (keystatus[buildkeys[4]] > 0)    //Either shift key
					goalz += (12<<8);
			}
		}

		if (goalz != posz)
		{
			if (posz < goalz) hvel += 32;
			if (posz > goalz) hvel = ((goalz-posz)>>3);

			posz += hvel;
			if (posz > loz-(4<<8)) posz = loz-(4<<8), hvel = 0;
			if (posz < hiz+(4<<8)) posz = hiz+(4<<8), hvel = 0;
		}
	}
	else
	{
		goalz = posz;
		if (keystatus[buildkeys[8]] > 0)                            //A
		{
			if (keystatus[0x1d] > 0) {
				horiz = std::max(-100, horiz - ((keystatus[buildkeys[4]] + 1) * synctics * 2));
			} else {
				if (zmode != 1)
					goalz -= (8<<8);
				else
				{
					zlock += (4<<8);
					keystatus[buildkeys[8]] = 0;
				}
			}
		}
		if (keystatus[buildkeys[9]] > 0)                            //Z (stand low)
		{
			if (keystatus[0x1d] > 0) {
				horiz = std::min(300, horiz + ((keystatus[buildkeys[4]] + 1) * synctics * 2));
			} else {
				if (zmode != 1)
					goalz += (8<<8);
				else if (zlock > 0)
				{
					zlock -= (4<<8);
					keystatus[buildkeys[9]] = 0;
				}
			}
		}

		if (goalz < hiz+(4<<8)) goalz = hiz+(4<<8);
		if (goalz > loz-(4<<8)) goalz = loz-(4<<8);
		if (zmode == 1) goalz = loz-zlock;
		if (goalz < hiz+(4<<8)) goalz = ((loz+hiz)>>1);  //ceiling&floor too close
		if (zmode == 1) posz = goalz;

		if (goalz != posz)
		{
			//if (posz < goalz) hvel += (32<<keystatus[buildkeys[4]]);
			//if (posz > goalz) hvel -= (32<<keystatus[buildkeys[4]]);
			if (posz < goalz) hvel = ((synctics* 192)<<keystatus[buildkeys[4]]);
			if (posz > goalz) hvel = ((synctics*-192)<<keystatus[buildkeys[4]]);

			posz += hvel;

			if (posz > loz-(4<<8)) posz = loz-(4<<8), hvel = 0;
			if (posz < hiz+(4<<8)) posz = hiz+(4<<8), hvel = 0;
		}
		else
			hvel = 0;
	}

	searchit = 2;
	if (searchstat >= 0)
	{
		if ((bstatus&1) > 0)
			searchit = 0;
		if (keystatus[0x4a] > 0)  // -
		{
			keystatus[0x4a] = 0;
			if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
			{
				if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL
				{
					if (visibility < 16384) visibility += visibility;
				}
				else
				{
					if ((keystatus[0x2a]|keystatus[0x36]) == 0)
						k = 16; else k = 1;

					if (highlightsectorcnt >= 0)
						for(i=0;i<highlightsectorcnt;i++)
							if (highlightsector[i] == searchsector)
							{
								while (k > 0)
								{
									for(i=0;i<highlightsectorcnt;i++)
									{
										g_sector[highlightsector[i]].visibility++;
										if (g_sector[highlightsector[i]].visibility == 240)
											g_sector[highlightsector[i]].visibility = 239;
									}
									k--;
								}
								break;
							}
					while (k > 0)
					{
						g_sector[searchsector].visibility++;
						if (g_sector[searchsector].visibility == 240)
							g_sector[searchsector].visibility = 239;
						k--;
					}
					asksave = true;
				}
			}
			else
			{
				k = 0;
				if (highlightsectorcnt >= 0)
				{
					for(i=0;i<highlightsectorcnt;i++)
						if (highlightsector[i] == searchsector)
						{
							k = 1;
							break;
						}
				}

				if (k == 0)
				{
					if (searchstat == 0) wall[searchwall].shade++;
					if (searchstat == 1) g_sector[searchsector].ceilingshade++;
					if (searchstat == 2) g_sector[searchsector].floorshade++;
					if (searchstat == 3) sprite[searchwall].shade++;
					if (searchstat == 4) wall[searchwall].shade++;
				}
				else
				{
					for(i=0;i<highlightsectorcnt;i++)
					{
						dasector = highlightsector[i];

						g_sector[dasector].ceilingshade++;        //sector shade
						g_sector[dasector].floorshade++;

						startwall = g_sector[dasector].wallptr;   //wall shade
						endwall = startwall + g_sector[dasector].wallnum - 1;
						for(j=startwall;j<=endwall;j++)
							wall[j].shade++;

						j = headspritesect[dasector];           //sprite shade
						while (j != -1)
						{
							sprite[j].shade++;
							j = nextspritesect[j];
						}
					}
				}
				asksave = true;
			}
		}
		if (keystatus[0x4e] > 0)  // +
		{
			keystatus[0x4e] = 0;
			if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
			{
				if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL
				{
					if (visibility > 32) visibility >>= 1;
				}
				else
				{
					if ((keystatus[0x2a]|keystatus[0x36]) == 0)
						k = 16; else k = 1;

					if (highlightsectorcnt >= 0)
						for(i=0;i<highlightsectorcnt;i++)
							if (highlightsector[i] == searchsector)
							{
								while (k > 0)
								{
									for(i=0;i<highlightsectorcnt;i++)
									{
										g_sector[highlightsector[i]].visibility--;
										if (g_sector[highlightsector[i]].visibility == 239)
											g_sector[highlightsector[i]].visibility = 240;
									}
									k--;
								}
								break;
							}
					while (k > 0)
					{
						g_sector[searchsector].visibility--;
						if (g_sector[searchsector].visibility == 239)
							g_sector[searchsector].visibility = 240;
						k--;
					}
					asksave = true;
				}
			}
			else
			{
				k = 0;
				if (highlightsectorcnt >= 0)
				{
					for(i=0;i<highlightsectorcnt;i++)
						if (highlightsector[i] == searchsector)
						{
							k = 1;
							break;
						}
				}

				if (k == 0)
				{
					if (searchstat == 0) wall[searchwall].shade--;
					if (searchstat == 1) g_sector[searchsector].ceilingshade--;
					if (searchstat == 2) g_sector[searchsector].floorshade--;
					if (searchstat == 3) sprite[searchwall].shade--;
					if (searchstat == 4) wall[searchwall].shade--;
				}
				else
				{
					for(i=0;i<highlightsectorcnt;i++)
					{
						dasector = highlightsector[i];

						g_sector[dasector].ceilingshade--;        //sector shade
						g_sector[dasector].floorshade--;

						startwall = g_sector[dasector].wallptr;   //wall shade
						endwall = startwall + g_sector[dasector].wallnum - 1;
						for(j=startwall;j<=endwall;j++)
							wall[j].shade--;

						j = headspritesect[dasector];           //sprite shade
						while (j != -1)
						{
							sprite[j].shade--;
							j = nextspritesect[j];
						}
					}
				}
				asksave = true;
			}
		}
		if (keystatus[0xc9] > 0) // PGUP
		{
			k = 0;
			if (highlightsectorcnt >= 0)
			{
				for(i=0;i<highlightsectorcnt;i++)
					if (highlightsector[i] == searchsector)
					{
						k = 1;
						break;
					}
			}

			if ((searchstat == 0) || (searchstat == 1))
			{
				if (k == 0)
				{
					i = headspritesect[searchsector];
					while (i != -1)
					{
						templong = getceilzofslope(searchsector,sprite[i].x,sprite[i].y);
						templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
						if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
						if (sprite[i].z == templong)
							sprite[i].z -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
						i = nextspritesect[i];
					}
					g_sector[searchsector].ceilingz -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
				}
				else
				{
					for(j=0;j<highlightsectorcnt;j++)
					{
						i = headspritesect[highlightsector[j]];
						while (i != -1)
						{
							templong = getceilzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
							templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
							if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
							if (sprite[i].z == templong)
								sprite[i].z -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
							i = nextspritesect[i];
						}
						g_sector[highlightsector[j]].ceilingz -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
					}
				}
			}
			if (searchstat == 2)
			{
				if (k == 0)
				{
					i = headspritesect[searchsector];
					while (i != -1)
					{
						templong = getflorzofslope(searchsector,sprite[i].x,sprite[i].y);
						if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
						if (sprite[i].z == templong)
							sprite[i].z -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
						i = nextspritesect[i];
					}
					g_sector[searchsector].floorz -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
				}
				else
				{
					for(j=0;j<highlightsectorcnt;j++)
					{
						i = headspritesect[highlightsector[j]];
						while (i != -1)
						{
							templong = getflorzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
							if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
							if (sprite[i].z == templong)
								sprite[i].z -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
							i = nextspritesect[i];
						}
						g_sector[highlightsector[j]].floorz -= 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
					}
				}
			}
			if (g_sector[searchsector].floorz < g_sector[searchsector].ceilingz)
				g_sector[searchsector].floorz = g_sector[searchsector].ceilingz;
			if (searchstat == 3)
			{
				if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL - put sprite on ceiling
				{
					sprite[searchwall].z = getceilzofslope(searchsector,sprite[searchwall].x,sprite[searchwall].y);
					if (sprite[searchwall].cstat&128) sprite[searchwall].z -= ((tilesizy[sprite[searchwall].picnum]*sprite[searchwall].yrepeat)<<1);
					if ((sprite[searchwall].cstat&48) != 32)
						sprite[searchwall].z += ((tilesizy[sprite[searchwall].picnum]*sprite[searchwall].yrepeat)<<2);
				}
				else
				{
					k = 0;
					if (highlightcnt >= 0)
						for(i=0;i<highlightcnt;i++)
							if (highlight[i] == searchwall+16384)
							{
								k = 1;
								break;
							}

					if (k == 0)
						sprite[searchwall].z -= (4<<8);
					else
					{
						for(i=0;i<highlightcnt;i++)
							if ((highlight[i]&0xc000) == 16384)
								sprite[highlight[i]&16383].z -= (4<<8);
					}
				}
			}
			asksave = true;
			keystatus[0xc9] = 0;
		}
		if (keystatus[0xd1] > 0) // PGDN
		{
			k = 0;
			if (highlightsectorcnt >= 0)
			{
				for(i=0;i<highlightsectorcnt;i++)
					if (highlightsector[i] == searchsector)
					{
						k = 1;
						break;
					}
			}

			if ((searchstat == 0) || (searchstat == 1))
			{
				if (k == 0)
				{
					i = headspritesect[searchsector];
					while (i != -1)
					{
						templong = getceilzofslope(searchsector,sprite[i].x,sprite[i].y);
						if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
						templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
						if (sprite[i].z == templong)
							sprite[i].z += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
						i = nextspritesect[i];
					}
					g_sector[searchsector].ceilingz += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
				}
				else
				{
					for(j=0;j<highlightsectorcnt;j++)
					{
						i = headspritesect[highlightsector[j]];
						while (i != -1)
						{
							templong = getceilzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
							if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
							templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);
							if (sprite[i].z == templong)
								sprite[i].z += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
							i = nextspritesect[i];
						}
						g_sector[highlightsector[j]].ceilingz += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
					}
				}
			}
			if (searchstat == 2)
			{
				if (k == 0)
				{
					i = headspritesect[searchsector];
					while (i != -1)
					{
						templong = getflorzofslope(searchsector,sprite[i].x,sprite[i].y);
						if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
						if (sprite[i].z == templong)
							sprite[i].z += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
						i = nextspritesect[i];
					}
					g_sector[searchsector].floorz += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
				}
				else
				{
					for(j=0;j<highlightsectorcnt;j++)
					{
						i = headspritesect[highlightsector[j]];
						while (i != -1)
						{
							templong = getflorzofslope(highlightsector[j],sprite[i].x,sprite[i].y);
							if (sprite[i].cstat&128) templong += ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
							if (sprite[i].z == templong)
								sprite[i].z += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
							i = nextspritesect[i];
						}
						g_sector[highlightsector[j]].floorz += 1024 << ((keystatus[0x1d]|keystatus[0x9d])<<1);	// JBF 20031128
					}
				}
			}
			if (g_sector[searchsector].ceilingz > g_sector[searchsector].floorz)
				g_sector[searchsector].ceilingz = g_sector[searchsector].floorz;
			if (searchstat == 3)
			{
				if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //CTRL - put sprite on ground
				{
					sprite[searchwall].z = getflorzofslope(searchsector,sprite[searchwall].x,sprite[searchwall].y);
					if (sprite[searchwall].cstat&128) sprite[searchwall].z -= ((tilesizy[sprite[searchwall].picnum]*sprite[searchwall].yrepeat)<<1);
				}
				else
				{
					k = 0;
					if (highlightcnt >= 0)
						for(i=0;i<highlightcnt;i++)
							if (highlight[i] == searchwall+16384)
							{
								k = 1;
								break;
							}

					if (k == 0)
						sprite[searchwall].z += (4<<8);
					else
					{
						for(i=0;i<highlightcnt;i++)
							if ((highlight[i]&0xc000) == 16384)
								sprite[highlight[i]&16383].z += (4<<8);
					}
				}
			}
			asksave = true;
			keystatus[0xd1] = 0;
		}
		if (keystatus[0x0f] > 0)  //TAB
		{
			if (searchstat == 0)
			{
				temppicnum = wall[searchwall].picnum;
				tempshade = wall[searchwall].shade;
				temppal = wall[searchwall].pal;
				tempxrepeat = wall[searchwall].xrepeat;
				tempyrepeat = wall[searchwall].yrepeat;
				tempcstat = wall[searchwall].cstat;
				templotag = wall[searchwall].lotag;
				temphitag = wall[searchwall].hitag;
				tempextra = wall[searchwall].extra;
			}
			if (searchstat == 1)
			{
				temppicnum = g_sector[searchsector].ceilingpicnum;
				tempshade = g_sector[searchsector].ceilingshade;
				temppal = g_sector[searchsector].ceilingpal;
				tempvis = g_sector[searchsector].visibility;
				tempxrepeat = g_sector[searchsector].ceilingxpanning;
				tempyrepeat = g_sector[searchsector].ceilingypanning;
				tempcstat = g_sector[searchsector].ceilingstat;
				templotag = g_sector[searchsector].lotag;
				temphitag = g_sector[searchsector].hitag;
				tempextra = g_sector[searchsector].extra;
			}
			if (searchstat == 2)
			{
				temppicnum = g_sector[searchsector].floorpicnum;
				tempshade = g_sector[searchsector].floorshade;
				temppal = g_sector[searchsector].floorpal;
				tempvis = g_sector[searchsector].visibility;
				tempxrepeat = g_sector[searchsector].floorxpanning;
				tempyrepeat = g_sector[searchsector].floorypanning;
				tempcstat = g_sector[searchsector].floorstat;
				templotag = g_sector[searchsector].lotag;
				temphitag = g_sector[searchsector].hitag;
				tempextra = g_sector[searchsector].extra;
			}
			if (searchstat == 3)
			{
				temppicnum = sprite[searchwall].picnum;
				tempshade = sprite[searchwall].shade;
				temppal = sprite[searchwall].pal;
				tempxrepeat = sprite[searchwall].xrepeat;
				tempyrepeat = sprite[searchwall].yrepeat;
				tempcstat = sprite[searchwall].cstat;
				templotag = sprite[searchwall].lotag;
				temphitag = sprite[searchwall].hitag;
				tempextra = sprite[searchwall].extra;
			}
			if (searchstat == 4)
			{
				temppicnum = wall[searchwall].overpicnum;
				tempshade = wall[searchwall].shade;
				temppal = wall[searchwall].pal;
				tempxrepeat = wall[searchwall].xrepeat;
				tempyrepeat = wall[searchwall].yrepeat;
				tempcstat = wall[searchwall].cstat;
				templotag = wall[searchwall].lotag;
				temphitag = wall[searchwall].hitag;
				tempextra = wall[searchwall].extra;
			}
			somethingintab = searchstat;
			keystatus[0x0f] = 0;
		}
		if (keystatus[0x1c] > 0) //Left ENTER
		{
			if ((keystatus[0x2a]|keystatus[0x36]) > 0)       //Either shift key
			{
				if (((searchstat == 0) || (searchstat == 4)) && ((keystatus[0x1d]|keystatus[0x9d]) > 0))  //Ctrl-shift Enter (auto-shade)
				{
					dashade[0] = 127;
					dashade[1] = -128;
					i = searchwall;
					do
					{
						if ((int)wall[i].shade < dashade[0]) dashade[0] = wall[i].shade;
						if ((int)wall[i].shade > dashade[1]) dashade[1] = wall[i].shade;

						i = wall[i].point2;
					}
					while (i != searchwall);

					daang = getangle(wall[wall[searchwall].point2].pt.x - wall[searchwall].pt.x, wall[wall[searchwall].point2].pt.y - wall[searchwall].pt.y);
					i = searchwall;
					do
					{
						j = getangle(wall[wall[i].point2].pt.x - wall[i].pt.x, wall[wall[i].point2].pt.y - wall[i].pt.y);
						k = ((j+2048-daang)&2047);
						if (k > 1024)
							k = 2048-k;
						wall[i].shade = dashade[0]+mulscalen<10>(k,dashade[1]-dashade[0]);

						i = wall[i].point2;
					}
					while (i != searchwall);
				}
				else if (somethingintab < 255)
				{
					if (searchstat == 0) wall[searchwall].shade = tempshade, wall[searchwall].pal = temppal;
					if (searchstat == 1)
					{
						g_sector[searchsector].ceilingshade = tempshade, g_sector[searchsector].ceilingpal = temppal;
						if ((somethingintab == 1) || (somethingintab == 2))
							g_sector[searchsector].visibility = tempvis;
					}
					if (searchstat == 2)
					{
						g_sector[searchsector].floorshade = tempshade, g_sector[searchsector].floorpal = temppal;
						if ((somethingintab == 1) || (somethingintab == 2))
							g_sector[searchsector].visibility = tempvis;
					}
					if (searchstat == 3) sprite[searchwall].shade = tempshade, sprite[searchwall].pal = temppal;
					if (searchstat == 4) wall[searchwall].shade = tempshade, wall[searchwall].pal = temppal;
				}
			}
			else if (((searchstat == 0) || (searchstat == 4)) && ((keystatus[0x1d]|keystatus[0x9d]) > 0) && (somethingintab < 255))  //Either ctrl key
			{
				i = searchwall;
				do
				{
					wall[i].picnum = temppicnum;
					wall[i].shade = tempshade;
					wall[i].pal = temppal;
					if ((somethingintab == 0) || (somethingintab == 4))
					{
						wall[i].xrepeat = tempxrepeat;
						wall[i].yrepeat = tempyrepeat;
						wall[i].cstat = tempcstat;
					}
					fixrepeats((short)i);
					i = wall[i].point2;
				}
				while (i != searchwall);
			}
			else if (((searchstat == 1) || (searchstat == 2)) && ((keystatus[0x1d]|keystatus[0x9d]) > 0) && (somethingintab < 255))  //Either ctrl key
			{
				clearbuf(&pskysearch[0],(int)((numsectors+3)>>2),0L);
				if (searchstat == 1)
				{
					i = searchsector;
					if ((g_sector[i].ceilingstat&1) > 0)
						pskysearch[i] = 1;

					while (pskysearch[i] == 1)
					{
						g_sector[i].ceilingpicnum = temppicnum;
						g_sector[i].ceilingshade = tempshade;
						g_sector[i].ceilingpal = temppal;
						if ((somethingintab == 1) || (somethingintab == 2))
						{
							g_sector[i].ceilingxpanning = tempxrepeat;
							g_sector[i].ceilingypanning = tempyrepeat;
							g_sector[i].ceilingstat = tempcstat;
						}
						pskysearch[i] = 2;

						startwall = g_sector[i].wallptr;
						endwall = startwall + g_sector[i].wallnum - 1;
						for(j=startwall;j<=endwall;j++)
						{
							k = wall[j].nextsector;
							if (k >= 0)
								if ((g_sector[k].ceilingstat&1) > 0)
									if (pskysearch[k] == 0)
										pskysearch[k] = 1;
						}

						for(j=0;j<numsectors;j++)
							if (pskysearch[j] == 1)
								i = j;
					}
				}
				if (searchstat == 2)
				{
					i = searchsector;
					if ((g_sector[i].floorstat&1) > 0)
						pskysearch[i] = 1;

					while (pskysearch[i] == 1)
					{
						g_sector[i].floorpicnum = temppicnum;
						g_sector[i].floorshade = tempshade;
						g_sector[i].floorpal = temppal;
						if ((somethingintab == 1) || (somethingintab == 2))
						{
							g_sector[i].floorxpanning = tempxrepeat;
							g_sector[i].floorypanning = tempyrepeat;
							g_sector[i].floorstat = tempcstat;
						}
						pskysearch[i] = 2;

						startwall = g_sector[i].wallptr;
						endwall = startwall + g_sector[i].wallnum - 1;
						for(j=startwall;j<=endwall;j++)
						{
							k = wall[j].nextsector;
							if (k >= 0)
								if ((g_sector[k].floorstat&1) > 0)
									if (pskysearch[k] == 0)
										pskysearch[k] = 1;
						}

						for(j=0;j<numsectors;j++)
							if (pskysearch[j] == 1)
								i = j;
					}
				}
			}
			else if (somethingintab < 255)
			{
				if (searchstat == 0)
				{
					wall[searchwall].picnum = temppicnum;
					wall[searchwall].shade = tempshade;
					wall[searchwall].pal = temppal;
					if (somethingintab == 0)
					{
						wall[searchwall].xrepeat = tempxrepeat;
						wall[searchwall].yrepeat = tempyrepeat;
						wall[searchwall].cstat = tempcstat;
						wall[searchwall].lotag = templotag;
						wall[searchwall].hitag = temphitag;
						wall[searchwall].extra = tempextra;
					}
					fixrepeats(searchwall);
				}
				if (searchstat == 1)
				{
					g_sector[searchsector].ceilingpicnum = temppicnum;
					g_sector[searchsector].ceilingshade = tempshade;
					g_sector[searchsector].ceilingpal = temppal;
					if ((somethingintab == 1) || (somethingintab == 2))
					{
						g_sector[searchsector].ceilingxpanning = tempxrepeat;
						g_sector[searchsector].ceilingypanning = tempyrepeat;
						g_sector[searchsector].ceilingstat = tempcstat;
						g_sector[searchsector].visibility = tempvis;
						g_sector[searchsector].lotag = templotag;
						g_sector[searchsector].hitag = temphitag;
						g_sector[searchsector].extra = tempextra;
					}
				}
				if (searchstat == 2)
				{
					g_sector[searchsector].floorpicnum = temppicnum;
					g_sector[searchsector].floorshade = tempshade;
					g_sector[searchsector].floorpal = temppal;
					if ((somethingintab == 1) || (somethingintab == 2))
					{
						g_sector[searchsector].floorxpanning= tempxrepeat;
						g_sector[searchsector].floorypanning= tempyrepeat;
						g_sector[searchsector].floorstat = tempcstat;
						g_sector[searchsector].visibility = tempvis;
						g_sector[searchsector].lotag = templotag;
						g_sector[searchsector].hitag = temphitag;
						g_sector[searchsector].extra = tempextra;
					}
				}
				if (searchstat == 3)
				{
					sprite[searchwall].picnum = temppicnum;
					if ((tilesizx[temppicnum] <= 0) || (tilesizy[temppicnum] <= 0))
					{
						// More aptly named, std::match in this case, as we're looking for the first
						// pair of elements both greater than 0.
						auto tilematch = std::mismatch(tilesizx.begin(), tilesizx.end(), tilesizy.begin(),
							[](auto tilex, auto tiley) { return !((tilex > 0) && (tiley > 0)); }).first;

						sprite[searchwall].picnum = std::distance(tilesizx.begin(), tilematch);
					}
					sprite[searchwall].shade = tempshade;
					sprite[searchwall].pal = temppal;
					if (somethingintab == 3)
					{
						sprite[searchwall].xrepeat = tempxrepeat;
						sprite[searchwall].yrepeat = tempyrepeat;
						if (sprite[searchwall].xrepeat < 1) sprite[searchwall].xrepeat = 1;
						if (sprite[searchwall].yrepeat < 1) sprite[searchwall].yrepeat = 1;
						sprite[searchwall].cstat = tempcstat;
						sprite[searchwall].lotag = templotag;
						sprite[searchwall].hitag = temphitag;
						sprite[searchwall].extra = tempextra;
					}
				}
				if (searchstat == 4)
				{
					wall[searchwall].overpicnum = temppicnum;
					if (wall[searchwall].nextwall >= 0)
						wall[wall[searchwall].nextwall].overpicnum = temppicnum;
					wall[searchwall].shade = tempshade;
					wall[searchwall].pal = temppal;
					if (somethingintab == 4)
					{
						wall[searchwall].xrepeat = tempxrepeat;
						wall[searchwall].yrepeat = tempyrepeat;
						wall[searchwall].cstat = tempcstat;
						wall[searchwall].lotag = templotag;
						wall[searchwall].hitag = temphitag;
						wall[searchwall].extra = tempextra;
					}
					fixrepeats(searchwall);
				}
			}
			asksave = true;
			keystatus[0x1c] = 0;
		}
		if (keystatus[0x2e] > 0)      //C
		{
			keystatus[0x2e] = 0;
			if (keystatus[0x38] > 0)    //Alt-C
			{
				if (somethingintab < 255)
				{
					switch(searchstat)
					{
						case 0:
							j = wall[searchwall].picnum;
							for(i=0;i<numwalls;i++)
								if (wall[i].picnum == j) wall[i].picnum = temppicnum;
							break;
						 case 1:
							j = g_sector[searchsector].ceilingpicnum;
							for(i=0;i<numsectors;i++)
								if (g_sector[i].ceilingpicnum == j) g_sector[i].ceilingpicnum = temppicnum;
							break;
						 case 2:
							j = g_sector[searchsector].floorpicnum;
							for(i=0;i<numsectors;i++)
								if (g_sector[i].floorpicnum == j) g_sector[i].floorpicnum = temppicnum;
							break;
						 case 3:
							 j = sprite[searchwall].picnum;
							 for(i=0;i<MAXSPRITES;i++)
								 if (sprite[i].statnum < MAXSTATUS)
									 if (sprite[i].picnum == j) sprite[i].picnum = temppicnum;
							 break;
						 case 4:
							 j = wall[searchwall].overpicnum;
							 for(i=0;i<numwalls;i++)
								 if (wall[i].overpicnum == j) wall[i].overpicnum = temppicnum;
							 break;
					}
				}
			}
			else    //C
			{
				if (searchstat == 3)
				{
					sprite[searchwall].cstat ^= 128;
					asksave = true;
				}
			}
		}
		if (keystatus[0x2f] > 0)  //V
		{
			if (searchstat == 0) templong = wall[searchwall].picnum;
			if (searchstat == 1) templong = g_sector[searchsector].ceilingpicnum;
			if (searchstat == 2) templong = g_sector[searchsector].floorpicnum;
			if (searchstat == 3) templong = sprite[searchwall].picnum;
			if (searchstat == 4) templong = wall[searchwall].overpicnum;
			templong = gettile(templong);
			if (searchstat == 0) wall[searchwall].picnum = templong;
			if (searchstat == 1) g_sector[searchsector].ceilingpicnum = templong;
			if (searchstat == 2) g_sector[searchsector].floorpicnum = templong;
			if (searchstat == 3) sprite[searchwall].picnum = templong;
			if (searchstat == 4)
			{
				wall[searchwall].overpicnum = templong;
				if (wall[searchwall].nextwall >= 0)
					 wall[wall[searchwall].nextwall].overpicnum = templong;
			}
			asksave = true;
			keystatus[0x2f] = 0;
		}

		if (keystatus[0x1a])  // [
		{
			keystatus[0x1a] = 0;
			if (keystatus[0x38]|keystatus[0xb8])
			{
				i = wall[searchwall].nextsector;
				if (i >= 0)
					switch(searchstat)
					{
						case 0: case 1: case 4:
							alignceilslope(searchsector,wall[searchwall].pt.x, wall[searchwall].pt.y,getceilzofslope(i,wall[searchwall].pt.x,wall[searchwall].pt.y));
							break;
						case 2:
							alignflorslope(searchsector,wall[searchwall].pt.x, wall[searchwall].pt.y, getflorzofslope(i,wall[searchwall].pt.x, wall[searchwall].pt.y));
							break;
					}
			}
			else
			{
				i = 512;
				if (keystatus[0x36]) i = 8;
				if (keystatus[0x2a]) i = 1;

				if (searchstat == 1)
				{
					if (!(g_sector[searchsector].ceilingstat&2))
						g_sector[searchsector].ceilingheinum = 0;
					g_sector[searchsector].ceilingheinum = std::max(static_cast<int>(g_sector[searchsector].ceilingheinum) - i,-32768);
				}
				if (searchstat == 2)
				{
					if (!(g_sector[searchsector].floorstat&2))
						g_sector[searchsector].floorheinum = 0;
					g_sector[searchsector].floorheinum = std::max(static_cast<int>(g_sector[searchsector].floorheinum) - i, -32768);
				}
			}

			if (g_sector[searchsector].ceilingheinum == 0)
				g_sector[searchsector].ceilingstat &= ~2;
			else
				g_sector[searchsector].ceilingstat |= 2;

			if (g_sector[searchsector].floorheinum == 0)
				g_sector[searchsector].floorstat &= ~2;
			else
				g_sector[searchsector].floorstat |= 2;
			asksave = true;
		}
		if (keystatus[0x1b])  // ]
		{
			keystatus[0x1b] = 0;
			if (keystatus[0x38]|keystatus[0xb8])
			{
				i = wall[searchwall].nextsector;
				if (i >= 0)
					switch(searchstat)
					{
						case 1:
							alignceilslope(searchsector, wall[searchwall].pt.x, wall[searchwall].pt.y, getceilzofslope(i,wall[searchwall].pt.x, wall[searchwall].pt.y));
							break;
						case 0: case 2: case 4:
							alignflorslope(searchsector, wall[searchwall].pt.x, wall[searchwall].pt.y,getflorzofslope(i,wall[searchwall].pt.x, wall[searchwall].pt.y));
							break;
					}
			}
			else
			{
				i = 512;
				if (keystatus[0x36]) i = 8;
				if (keystatus[0x2a]) i = 1;

				if (searchstat == 1)
				{
					if (!(g_sector[searchsector].ceilingstat&2))
						g_sector[searchsector].ceilingheinum = 0;
					g_sector[searchsector].ceilingheinum = std::min(static_cast<int>(g_sector[searchsector].ceilingheinum) + i, 32767);
				}
				if (searchstat == 2)
				{
					if (!(g_sector[searchsector].floorstat&2))
						g_sector[searchsector].floorheinum = 0;
					g_sector[searchsector].floorheinum = std::min(static_cast<int>(g_sector[searchsector].floorheinum) + i, 32767);
				}
			}

			if (g_sector[searchsector].ceilingheinum == 0)
				g_sector[searchsector].ceilingstat &= ~2;
			else
				g_sector[searchsector].ceilingstat |= 2;

			if (g_sector[searchsector].floorheinum == 0)
				g_sector[searchsector].floorstat &= ~2;
			else
				g_sector[searchsector].floorstat |= 2;

			asksave = true;
		}

		smooshyalign = keystatus[0x4c];
		repeatpanalign = (keystatus[0x2a]|keystatus[0x36]);
		if ((keystatus[0x4b]|keystatus[0x4d]) > 0)  // 4 & 6 (keypad)
		{
			if ((repeatcountx == 0) || (repeatcountx > 16))
			{
				changedir = 0;
				if (keystatus[0x4b] > 0) changedir = -1;
				if (keystatus[0x4d] > 0) changedir = 1;

				if ((searchstat == 0) || (searchstat == 4))
				{
					if (repeatpanalign == 0)
						wall[searchwall].xrepeat = changechar(wall[searchwall].xrepeat, changedir, smooshyalign, true);
					else
						wall[searchwall].xpanning = changechar(wall[searchwall].xpanning, changedir, smooshyalign, false);
				}
				if ((searchstat == 1) || (searchstat == 2))
				{
					if (searchstat == 1)
						g_sector[searchsector].ceilingxpanning = changechar(g_sector[searchsector].ceilingxpanning, changedir, smooshyalign, false);
					else
						g_sector[searchsector].floorxpanning = changechar(g_sector[searchsector].floorxpanning, changedir, smooshyalign, false);
				}
				if (searchstat == 3)
				{
					sprite[searchwall].xrepeat = changechar(sprite[searchwall].xrepeat, changedir, smooshyalign, true);
					if (sprite[searchwall].xrepeat < 4)
						sprite[searchwall].xrepeat = 4;
				}
				asksave = true;
				repeatcountx = std::max(1, repeatcountx);
			}
			repeatcountx += (synctics>>1);
		}
		else
			repeatcountx = 0;

		if ((keystatus[0x48]|keystatus[0x50]) > 0)  // 2 & 8 (keypad)
		{
			if ((repeatcounty == 0) || (repeatcounty > 16))
			{
				changedir = 0;
				if (keystatus[0x48] > 0) changedir = -1;
				if (keystatus[0x50] > 0) changedir = 1;

				if ((searchstat == 0) || (searchstat == 4))
				{
					if (repeatpanalign == 0)
						wall[searchwall].yrepeat = changechar(wall[searchwall].yrepeat, changedir, smooshyalign, true);
					else
						wall[searchwall].ypanning = changechar(wall[searchwall].ypanning, changedir, smooshyalign, false);
				}
				if ((searchstat == 1) || (searchstat == 2))
				{
					if (searchstat == 1)
						g_sector[searchsector].ceilingypanning = changechar(g_sector[searchsector].ceilingypanning, changedir, smooshyalign, false);
					else
						g_sector[searchsector].floorypanning = changechar(g_sector[searchsector].floorypanning, changedir, smooshyalign, false);
				}
				if (searchstat == 3)
				{
					sprite[searchwall].yrepeat = changechar(sprite[searchwall].yrepeat, changedir, smooshyalign, true);
					if (sprite[searchwall].yrepeat < 4)
						sprite[searchwall].yrepeat = 4;
				}
				asksave = true;
				repeatcounty = std::max(1, repeatcounty);
			}
			repeatcounty += (synctics>>1);
			//}
		}
		else
			repeatcounty = 0;

		if (keystatus[0x33] > 0) // , Search & fix panning to the left (3D)
		{
			if (searchstat == 3)
			{
				i = searchwall;
				if ((keystatus[0x2a]|keystatus[0x36]) > 0)
					sprite[i].ang = ((sprite[i].ang+2048-1)&2047);
				else
				{
					sprite[i].ang = ((sprite[i].ang+2048-128)&2047);
					keystatus[0x33] = 0;
				}
			}
		}
		if (keystatus[0x34] > 0) // . Search & fix panning to the right (3D)
		{
			if ((searchstat == 0) || (searchstat == 4))
			{
				AutoAlignWalls((int)searchwall,0L);

				/*
				int wallfind[2], cnt, daz[2];
				short sectnum, nextsectnum;

				wallfind[0] = searchwall;
				cnt = 4096;
				do
				{
					wallfind[1] = wall[wallfind[0]].point2;
					j = -1;
					if (wall[wallfind[1]].picnum == wall[searchwall].picnum)
						j = wallfind[1];
					k = wallfind[1];

					while ((wall[wallfind[1]].nextwall >= 0) && (wall[wall[wallfind[1]].nextwall].point2 != k))
					{
						i = wall[wall[wallfind[1]].nextwall].point2;   //break if going around in circles on red lines with same picture on both sides
						if (wallfind[1] == wall[wall[i].nextwall].point2)
							break;

						wallfind[1] = wall[wall[wallfind[1]].nextwall].point2;
						if (wall[wallfind[1]].picnum == wall[searchwall].picnum)
							j = wallfind[1];
					}
					wallfind[1] = j;

					if ((j >= 0) && (wallfind[1] != searchwall))
					{
						j = (wall[wallfind[0]].xpanning+(wall[wallfind[0]].xrepeat<<3)) % tilesizx[wall[wallfind[0]].picnum];
						wall[wallfind[1]].cstat &= ~8;    //Set to non-flip
						wall[wallfind[1]].cstat |= 4;     //Set y-orientation
						wall[wallfind[1]].xpanning = j;

						for(k=0;k<2;k++)
						{
							sectnum = sectorofwall((short)wallfind[k]);
							nextsectnum = wall[wallfind[k]].nextsector;

							if (nextsectnum == -1)
							{
								if ((wall[wallfind[k]].cstat&4) == 0)
									daz[k] = g_sector[sectnum].ceilingz;
								else
									daz[k] = g_sector[sectnum].floorz;
							}
							else                                      //topstep
							{
								if (g_sector[nextsectnum].ceilingz > g_sector[sectnum].ceilingz)
									daz[k] = g_sector[nextsectnum].ceilingz;
								else if (g_sector[nextsectnum].floorz < g_sector[sectnum].floorz)
									daz[k] = g_sector[nextsectnum].floorz;
							}
						}

						j = (picsiz[wall[searchwall].picnum]>>4);
						if ((1<<j) != tilesizy[wall[searchwall].picnum]) j++;

						j = ((wall[wallfind[0]].ypanning+(((daz[1]-daz[0])*wall[wallfind[0]].yrepeat)>>(j+3)))&255);
						wall[wallfind[1]].ypanning = j;
						wall[wallfind[1]].yrepeat = wall[wallfind[0]].yrepeat;
						if (nextsectnum >= 0)
							if (g_sector[nextsectnum].ceilingz >= g_sector[sectnum].ceilingz)
								if (g_sector[nextsectnum].floorz <= g_sector[sectnum].floorz)
								{
									if (wall[wall[wallfind[1]].nextwall].picnum == wall[searchwall].picnum)
									{
										wall[wall[wallfind[1]].nextwall].yrepeat = wall[wallfind[0]].yrepeat;
										if ((wall[wall[wallfind[1]].nextwall].cstat&4) == 0)
											daz[1] = g_sector[nextsectnum].floorz;
										else
											daz[1] = g_sector[sectnum].ceilingz;
										wall[wall[wallfind[1]].nextwall].ypanning = j;
									}
								}
					}
					wallfind[0] = wallfind[1];
					cnt--;
				}
				while ((wall[wallfind[0]].picnum == wall[searchwall].picnum) && (wallfind[0] != searchwall) && (cnt > 0));
				*/

				keystatus[0x34] = 0;
			}
			if (searchstat == 3)
			{
				i = searchwall;
				if ((keystatus[0x2a]|keystatus[0x36]) > 0)
					sprite[i].ang = ((sprite[i].ang+2048+1)&2047);
				else
				{
					sprite[i].ang = ((sprite[i].ang+2048+128)&2047);
					keystatus[0x34] = 0;
				}
			}
		}
		if (keystatus[0x35] > 0)  // /?     Reset panning&repeat to 0
		{
			if ((searchstat == 0) || (searchstat == 4))
			{
				wall[searchwall].xpanning = 0;
				wall[searchwall].ypanning = 0;
				wall[searchwall].xrepeat = 8;
				wall[searchwall].yrepeat = 8;
				wall[searchwall].cstat = 0;
				fixrepeats((short)searchwall);
			}
			if (searchstat == 1)
			{
				g_sector[searchsector].ceilingxpanning = 0;
				g_sector[searchsector].ceilingypanning = 0;
				g_sector[searchsector].ceilingstat &= ~2;
				g_sector[searchsector].ceilingheinum = 0;
			}
			if (searchstat == 2)
			{
				g_sector[searchsector].floorxpanning = 0;
				g_sector[searchsector].floorypanning = 0;
				g_sector[searchsector].floorstat &= ~2;
				g_sector[searchsector].floorheinum = 0;
			}
			if (searchstat == 3)
			{
				if ((keystatus[0x2a]|keystatus[0x36]) > 0)
				{
					sprite[searchwall].xrepeat = sprite[searchwall].yrepeat;
				}
				else
				{
					sprite[searchwall].xrepeat = 64;
					sprite[searchwall].yrepeat = 64;
				}
			}
			keystatus[0x35] = 0;
			asksave = true;
		}

		if (keystatus[0x19] > 0)  // P (parallaxing sky)
		{
			if ((keystatus[0x1d]|keystatus[0x9d]) > 0)
			{
				parallaxtype++;
				if (parallaxtype == 3)
					parallaxtype = 0;
			}
			else if ((keystatus[0x38]|keystatus[0xb8]) > 0)
			{
				switch (searchstat)
				{
					case 0: case 4:
						std::strcpy((char *)buffer,"Wall pal: ");
						wall[searchwall].pal = getnumber256((char *)buffer,wall[searchwall].pal,256L,0);
						break;
					case 1:
						std::strcpy((char *)buffer,"Ceiling pal: ");
						g_sector[searchsector].ceilingpal = getnumber256((char *)buffer,g_sector[searchsector].ceilingpal,256L,0);
						break;
					case 2:
						std::strcpy((char *)buffer,"Floor pal: ");
						g_sector[searchsector].floorpal = getnumber256((char *)buffer,g_sector[searchsector].floorpal,256L,0);
						break;
					case 3:
						std::strcpy((char *)buffer,"Sprite pal: ");
						sprite[searchwall].pal = getnumber256((char *)buffer,sprite[searchwall].pal,256L,0);
						break;
				}
			}
			else
			{
				if ((searchstat == 0) || (searchstat == 1) || (searchstat == 4))
				{
					g_sector[searchsector].ceilingstat ^= 1;
					asksave = true;
				}
				else if (searchstat == 2)
				{
					g_sector[searchsector].floorstat ^= 1;
					asksave = true;
				}
			}
			keystatus[0x19] = 0;
		}

		if (keystatus[0x20] != 0)   //Alt-D  (adjust sprite[].clipdist)
		{
			keystatus[0x20] = 0;
			if ((keystatus[0x38]|keystatus[0xb8]) > 0)
			{
				if (searchstat == 3)
				{
					std::strcpy((char *)buffer,"Sprite clipdist: ");
					sprite[searchwall].clipdist = getnumber256((char *)buffer,sprite[searchwall].clipdist,256L,0);
				}
			}
		}

		if (keystatus[0x30] > 0)  // B (clip Blocking xor) (3D)
		{
			if (searchstat == 3)
			{
				sprite[searchwall].cstat ^= 1;
				sprite[searchwall].cstat &= ~256;
				sprite[searchwall].cstat |= ((sprite[searchwall].cstat&1)<<8);
				asksave = true;
			}
			else
			{
				wall[searchwall].cstat ^= 1;
				wall[searchwall].cstat &= ~64;
				if ((wall[searchwall].nextwall >= 0) && ((keystatus[0x2a]|keystatus[0x36]) == 0))
				{
					wall[wall[searchwall].nextwall].cstat &= ~(1+64);
					wall[wall[searchwall].nextwall].cstat |= (wall[searchwall].cstat&1);
				}
				asksave = true;
			}
			keystatus[0x30] = 0;
		}
		if (keystatus[0x14] > 0)  // T (transluscence for sprites/masked walls)
		{
			/*if (searchstat == 1)   //Set masked/transluscent ceilings/floors
			{
				i = (g_sector[searchsector].ceilingstat&(128+256));
				g_sector[searchsector].ceilingstat &= ~(128+256);
				switch(i)
				{
					case 0: g_sector[searchsector].ceilingstat |= 128; break;
					case 128: g_sector[searchsector].ceilingstat |= 256; break;
					case 256: g_sector[searchsector].ceilingstat |= 384; break;
					case 384: g_sector[searchsector].ceilingstat |= 0; break;
				}
				asksave = true;
			}
			if (searchstat == 2)
			{
				i = (g_sector[searchsector].floorstat&(128+256));
				g_sector[searchsector].floorstat &= ~(128+256);
				switch(i)
				{
					case 0: g_sector[searchsector].floorstat |= 128; break;
					case 128: g_sector[searchsector].floorstat |= 256; break;
					case 256: g_sector[searchsector].floorstat |= 384; break;
					case 384: g_sector[searchsector].floorstat |= 0; break;
				}
				asksave = true;
			}*/
			if (searchstat == 3)
			{
				if ((sprite[searchwall].cstat&2) == 0)
					sprite[searchwall].cstat |= 2;
				else if ((sprite[searchwall].cstat&512) == 0)
					sprite[searchwall].cstat |= 512;
				else
					sprite[searchwall].cstat &= ~(2+512);
				asksave = true;
			}
			if (searchstat == 4)
			{
				if ((wall[searchwall].cstat&128) == 0)
					wall[searchwall].cstat |= 128;
				else if ((wall[searchwall].cstat&512) == 0)
					wall[searchwall].cstat |= 512;
				else
					wall[searchwall].cstat &= ~(128+512);

				if (wall[searchwall].nextwall >= 0)
				{
					wall[wall[searchwall].nextwall].cstat &= ~(128+512);
					wall[wall[searchwall].nextwall].cstat |= (wall[searchwall].cstat&(128+512));
				}
				asksave = true;
			}
			keystatus[0x14] = 0;
		}

		if (keystatus[0x2] > 0)  // 1 (make 1-way wall)
		{
			if (searchstat != 3)
			{
				wall[searchwall].cstat ^= 32;
				asksave = true;
			}
			else
			{
				sprite[searchwall].cstat ^= 64;
				i = sprite[searchwall].cstat;
				if ((i&48) == 32)
				{
					sprite[searchwall].cstat &= ~8;
					if ((i&64) > 0)
						if (posz > sprite[searchwall].z)
							sprite[searchwall].cstat |= 8;
				}
				asksave = true;
			}
			keystatus[0x2] = 0;
		}
		if (keystatus[0x3] > 0)  // 2 (bottom wall swapping)
		{
			if (searchstat != 3)
			{
				wall[searchwall].cstat ^= 2;
				asksave = true;
			}
			keystatus[0x3] = 0;
		}
		if (keystatus[0x18] > 0)  // O (top/bottom orientation - for doors)
		{
			if ((searchstat == 0) || (searchstat == 4))
			{
				wall[searchwall].cstat ^= 4;
				asksave = true;
			}
			if (searchstat == 3)   // O (ornament onto wall) (2D)
			{
				asksave = true;
				i = searchwall;

				hitscan(sprite[i].x,sprite[i].y,sprite[i].z,sprite[i].sectnum,
					sintable[(sprite[i].ang+2560+1024)&2047],
					sintable[(sprite[i].ang+2048+1024)&2047],
					0,
					&hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,CLIPMASK1);

				sprite[i].x = hitx;
				sprite[i].y = hity;
				sprite[i].z = hitz;
				changespritesect(i,hitsect);
				if (hitwall >= 0)
					sprite[i].ang = ((getangle(wall[wall[hitwall].point2].pt.x - wall[hitwall].pt.x, wall[wall[hitwall].point2].pt.y - wall[hitwall].pt.y) + 512) & 2047);

					//Make sure sprite's in right sector
				if (inside(sprite[i].x,sprite[i].y,sprite[i].sectnum) == 0)
				{
					j = wall[hitwall].point2;
					sprite[i].x -= ksgn(wall[j].pt.y - wall[hitwall].pt.y);
					sprite[i].y += ksgn(wall[j].pt.x - wall[hitwall].pt.x);
				}
			}
			keystatus[0x18] = 0;
		}
		if (keystatus[0x32] > 0)  // M (masking walls)
		{
			if (searchstat != 3)
			{
				i = wall[searchwall].nextwall;
				templong = (keystatus[0x2a]|keystatus[0x36]);
				if (i >= 0)
				{
					wall[searchwall].cstat ^= 16;
					if ((wall[searchwall].cstat&16) > 0)
					{
						wall[searchwall].cstat &= ~8;
						if (templong == 0)
						{
							wall[i].cstat |= 8;           //auto other-side flip
							wall[i].cstat |= 16;
							wall[i].overpicnum = wall[searchwall].overpicnum;
						}
					}
					else
					{
						wall[searchwall].cstat &= ~8;
						if (templong == 0)
						{
							wall[i].cstat &= ~8;         //auto other-side unflip
							wall[i].cstat &= ~16;
						}
					}
					wall[searchwall].cstat &= ~32;
					if (templong == 0) wall[i].cstat &= ~32;
					asksave = true;
				}
			}
			keystatus[0x32] = 0;
		}
		if (keystatus[0x23] > 0)  // H (hitscan sensitivity)
		{
			if (searchstat == 3)
			{
				sprite[searchwall].cstat ^= 256;
				asksave = true;
			}
			else
			{
				wall[searchwall].cstat ^= 64;
				if ((wall[searchwall].nextwall >= 0) && ((keystatus[0x2a]|keystatus[0x36]) == 0))
				{
					wall[wall[searchwall].nextwall].cstat &= ~64;
					wall[wall[searchwall].nextwall].cstat |= (wall[searchwall].cstat&64);
				}
				asksave = true;
			}
			keystatus[0x23] = 0;
		}
		if (keystatus[0x12] > 0)  // E (expand)
		{
			if (searchstat == 1)
			{
				g_sector[searchsector].ceilingstat ^= 8;
				asksave = true;
			}
			if (searchstat == 2)
			{
				g_sector[searchsector].floorstat ^= 8;
				asksave = true;
			}
			keystatus[0x12] = 0;
		}
		if (keystatus[0x13] > 0)  // R (relative alignment, rotation)
		{
			if (searchstat == 1)
			{
				g_sector[searchsector].ceilingstat ^= 64;
				asksave = true;
			}
			if (searchstat == 2)
			{
				g_sector[searchsector].floorstat ^= 64;
				asksave = true;
			}
			if (searchstat == 3)
			{
				i = sprite[searchwall].cstat;
				if ((i&48) < 32) i += 16; else i &= ~48;
				sprite[searchwall].cstat = i;
				asksave = true;
			}
			keystatus[0x13] = 0;
		}
		if (keystatus[0x21] > 0)  //F (Flip)
		{
			keystatus[0x21] = 0;
			if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT-F (relative alignmment flip)
			{
				if (searchstat != 3)
				{
					setfirstwall(searchsector,searchwall);
					asksave = true;
				}
			}
			else
			{
				if ((searchstat == 0) || (searchstat == 4))
				{
					i = wall[searchwall].cstat;
					i = ((i>>3)&1)+((i>>7)&2);    //3-x,8-y
					switch(i)
					{
						case 0: i = 1; break;
						case 1: i = 3; break;
						case 2: i = 0; break;
						case 3: i = 2; break;
					}
					i = ((i&1)<<3)+((i&2)<<7);
					wall[searchwall].cstat &= ~0x0108;
					wall[searchwall].cstat |= i;
					asksave = true;
				}
				if (searchstat == 1)         //8-way ceiling flipping (bits 2,4,5)
				{
					i = g_sector[searchsector].ceilingstat;
					i = (i&0x4)+((i>>4)&3);
					switch(i)
					{
						case 0: i = 6; break;
						case 6: i = 3; break;
						case 3: i = 5; break;
						case 5: i = 1; break;
						case 1: i = 7; break;
						case 7: i = 2; break;
						case 2: i = 4; break;
						case 4: i = 0; break;
					}
					i = (i&0x4)+((i&3)<<4);
					g_sector[searchsector].ceilingstat &= ~0x34;
					g_sector[searchsector].ceilingstat |= i;
					asksave = true;
				}
				if (searchstat == 2)         //8-way floor flipping (bits 2,4,5)
				{
					i = g_sector[searchsector].floorstat;
					i = (i&0x4)+((i>>4)&3);
					switch(i)
					{
						case 0: i = 6; break;
						case 6: i = 3; break;
						case 3: i = 5; break;
						case 5: i = 1; break;
						case 1: i = 7; break;
						case 7: i = 2; break;
						case 2: i = 4; break;
						case 4: i = 0; break;
					}
					i = (i&0x4)+((i&3)<<4);
					g_sector[searchsector].floorstat &= ~0x34;
					g_sector[searchsector].floorstat |= i;
					asksave = true;
				}
				if (searchstat == 3)
				{
					i = sprite[searchwall].cstat;
					if (((i&48) == 32) && ((i&64) == 0))
					{
						sprite[searchwall].cstat &= ~0xc;
						sprite[searchwall].cstat |= (i&4)^4;
					}
					else
					{
						i = ((i>>2)&3);
						switch(i)
						{
							case 0: i = 1; break;
							case 1: i = 3; break;
							case 2: i = 0; break;
							case 3: i = 2; break;
						}
						i <<= 2;
						sprite[searchwall].cstat &= ~0xc;
						sprite[searchwall].cstat |= i;
					}
					asksave = true;
				}
			}
		}
		if (keystatus[0x1f] > 0)  //S (insert sprite) (3D)
		{
			dax = 16384;
			day = divscalen<14>(searchx-(xdim>>1),xdim>>1);
			rotatepoint(0,0,dax,day,ang,&dax,&day);

			hitscan(posx,posy,posz,cursectnum,               //Start position
				dax,day,(scale(searchy,200,ydim)-horiz)*2000, //vector of 3D ang
				&hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,CLIPMASK1);

			if (hitsect >= 0)
			{
				dax = hitx;
				day = hity;
				if ((gridlock > 0) && (grid > 0))
				{
					if ((searchstat == 0) || (searchstat == 4))
					{
						hitz = (hitz&0xfffffc00);
					}
					else
					{
						dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
						day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
					}
				}

				i = insertsprite(hitsect,0);
				sprite[i].x = dax, sprite[i].y = day;
				sprite[i].cstat = defaultspritecstat;
				sprite[i].shade = 0;
				sprite[i].pal = 0;
				sprite[i].xrepeat = 64, sprite[i].yrepeat = 64;
				sprite[i].xoffset = 0, sprite[i].yoffset = 0;
				sprite[i].ang = 1536;
				sprite[i].xvel = 0; sprite[i].yvel = 0; sprite[i].zvel = 0;
				sprite[i].owner = -1;
				sprite[i].clipdist = 32;
				sprite[i].lotag = 0;
				sprite[i].hitag = 0;
				sprite[i].extra = -1;

				std::ranges::fill(localartfreq, 0);

				for(k=0;k<MAXSPRITES;k++)
					if (sprite[k].statnum < MAXSTATUS)
						localartfreq[sprite[k].picnum]++;
				j = 0;
				for(k=0;k<MAXTILES;k++)
					if (localartfreq[k] > localartfreq[j])
						j = k;
				if (localartfreq[j] > 0)
					sprite[i].picnum = j;
				else
					sprite[i].picnum = 0;

				if (somethingintab == 3)
				{
					sprite[i].picnum = temppicnum;
					if ((tilesizx[temppicnum] <= 0) || (tilesizy[temppicnum] <= 0))
					{
						j = 0;
						for(k=0;k<MAXTILES;k++)
							if ((tilesizx[k] > 0) && (tilesizy[k] > 0))
							{
								j = k;
								break;
							}
						sprite[i].picnum = j;
					}
					sprite[i].shade = tempshade;
					sprite[i].pal = temppal;
					sprite[i].xrepeat = tempxrepeat;
					sprite[i].yrepeat = tempyrepeat;
					if (sprite[i].xrepeat < 1) sprite[i].xrepeat = 1;
					if (sprite[i].yrepeat < 1) sprite[i].yrepeat = 1;
					sprite[i].cstat = tempcstat;
				}

				j = ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
				if ((sprite[i].cstat&128) == 0)
					sprite[i].z = std::min(std::max(hitz, getceilzofslope(hitsect, hitx, hity) + (j << 1)), getflorzofslope(hitsect, hitx, hity));
				else
					sprite[i].z = std::min(std::max(hitz, getceilzofslope(hitsect, hitx, hity) + j), getflorzofslope(hitsect, hitx, hity) - j);

				if ((searchstat == 0) || (searchstat == 4))
				{
					sprite[i].cstat = (sprite[i].cstat&~48)|(16+64);
					if (hitwall >= 0)
						sprite[i].ang = ((getangle(wall[wall[hitwall].point2].pt.x-wall[hitwall].pt.x,wall[wall[hitwall].point2].pt.y-wall[hitwall].pt.y)+512)&2047);

						//Make sure sprite's in right sector
					if (inside(sprite[i].x,sprite[i].y,sprite[i].sectnum) == 0)
					{
						j = wall[hitwall].point2;
						sprite[i].x -= ksgn(wall[j].pt.y-wall[hitwall].pt.y);
						sprite[i].y += ksgn(wall[j].pt.x-wall[hitwall].pt.x);
					}
				}
				else
				{
					if (tilesizy[sprite[i].picnum] >= 32) sprite[i].cstat |= 1;
				}

				updatenumsprites();
				asksave = true;
			}

			keystatus[0x1f] = 0;
		}
		if (keystatus[0xd3] > 0)
		{
			if (searchstat == 3)
			{
				deletesprite(searchwall);
				updatenumsprites();
				asksave = true;
			}
			keystatus[0xd3] = 0;
		}

		if ((keystatus[0x3f]|keystatus[0x40]) > 0)  //F5,F6
		{
			switch(searchstat)
			{
				case 1: case 2: ExtShowSectorData(searchsector); break;
				case 0: case 4: ExtShowWallData(searchwall); break;
				case 3: ExtShowSpriteData(searchwall); break;
			}
			keystatus[0x3f] = 0, keystatus[0x40] = 0;
		}
		if ((keystatus[0x41]|keystatus[0x42]) > 0)  //F7,F8
		{
			switch(searchstat)
			{
				case 1: case 2: ExtEditSectorData(searchsector); break;
				case 0: case 4: ExtEditWallData(searchwall); break;
				case 3: ExtEditSpriteData(searchwall); break;
			}
			keystatus[0x41] = 0, keystatus[0x42] = 0;
		}

	}
	if (keystatus[buildkeys[14]] > 0)  // Enter
	{
		overheadeditor();
		keystatus[buildkeys[14]] = 0;
	}
}

unsigned char changechar(unsigned char dachar, int dadir, unsigned char smooshyalign, bool boundcheck)
{
	if (dadir < 0)
	{
		if ((dachar > 0) || !boundcheck)
		{
			dachar--;
			if (smooshyalign > 0)
				dachar = (dachar&0xf8);
		}
	}
	else if (dadir > 0)
	{
		if ((dachar < 255) || !boundcheck)
		{
			dachar++;
			if (smooshyalign > 0)
			{
				if (dachar >= 256-8) dachar = 255;
				else dachar = ((dachar+7)&0xf8);
			}
		}
	}
	return(dachar);
}

int gettile(int tilenum)
{
	int i;
	int j;
	int temp;
	int templong;

	if (tilenum < 0) tilenum = 0;

	const int xtiles = xdim >> 6;
	const int ytiles = ydim >> 6;
	const int tottiles = xtiles * ytiles;
	const int otilenum{tilenum};

	keystatus[0x2f] = 0;

	std::ranges::fill(localartfreq, 0);
	std::iota(localartlookup.begin(), localartlookup.end(), 0);

	if ((searchstat == 1) || (searchstat == 2))
		for(i=0;i<numsectors;i++)
		{
			localartfreq[g_sector[i].ceilingpicnum]++;
			localartfreq[g_sector[i].floorpicnum]++;
		}
	if (searchstat == 0)
		for(i=0;i<numwalls;i++)
			localartfreq[wall[i].picnum]++;
	if (searchstat == 4)
		for(i=0;i<numwalls;i++)
			localartfreq[wall[i].overpicnum]++;
	if (searchstat == 3)
		for(i=0;i<MAXSPRITES;i++)
			if (sprite[i].statnum < MAXSTATUS)
				localartfreq[sprite[i].picnum]++;
	
	int gap = (MAXTILES>>1);
	
	do
	{
		for(i=0;i<MAXTILES-gap;i++)
		{
			temp = i;
			while ((localartfreq[temp] < localartfreq[temp+gap]) && (temp >= 0))
			{
				templong = localartfreq[temp];
				localartfreq[temp] = localartfreq[temp+gap];
				localartfreq[temp+gap] = templong;
				templong = localartlookup[temp];
				localartlookup[temp] = localartlookup[temp+gap];
				localartlookup[temp+gap] = templong;

				if (tilenum == temp)
					tilenum = temp+gap;
				else if (tilenum == temp+gap)
					tilenum = temp;

				temp -= gap;
			}
		}
		gap >>= 1;
	} while (gap > 0);

	localartlookupnum = 0;
	while (localartfreq[localartlookupnum] > 0)
		localartlookupnum++;

	if (localartfreq[0] == 0)
	{
		tilenum = otilenum;
		localartlookupnum = MAXTILES;
		std::iota(localartlookup.begin(), localartlookup.end(), 0);
	}

	int topleft = ((tilenum/(xtiles<<gettilezoom))*(xtiles<<gettilezoom))-(xtiles<<gettilezoom);
	if (topleft < 0) topleft = 0;
	if (topleft > MAXTILES-(tottiles<<(gettilezoom<<1))) topleft = MAXTILES-(tottiles<<(gettilezoom<<1));
	while ((keystatus[0x1c]|keystatus[1]) == 0)
	{
		drawtilescreen(topleft,tilenum);
		OSD_Draw();
		showframe();

		if (handleevents()) {
			if (quitevent) quitevent = false;
		}

		synctics = totalclock-lockclock;
		lockclock += synctics;

		if ((keystatus[0x37] > 0) && (gettilezoom < 2))
		{
			gettilezoom++;
			topleft = ((tilenum/(xtiles<<gettilezoom))*(xtiles<<gettilezoom))-(xtiles<<gettilezoom);
			if (topleft < 0) topleft = 0;
			if (topleft > MAXTILES-(tottiles<<(gettilezoom<<1))) topleft = MAXTILES-(tottiles<<(gettilezoom<<1));
			keystatus[0x37] = 0;
		}
		if ((keystatus[0xb5] > 0) && (gettilezoom > 0))
		{
			gettilezoom--;
			topleft = ((tilenum/(xtiles<<gettilezoom))*(xtiles<<gettilezoom))-(xtiles<<gettilezoom);
			if (topleft < 0) topleft = 0;
			if (topleft > MAXTILES-(tottiles<<(gettilezoom<<1))) topleft = MAXTILES-(tottiles<<(gettilezoom<<1));
			keystatus[0xb5] = 0;
		}
		if ((keystatus[0xcb] > 0) && (tilenum > 0))
			tilenum--, keystatus[0xcb] = 0;
		if ((keystatus[0xcd] > 0) && (tilenum < MAXTILES-1))
			tilenum++, keystatus[0xcd] = 0;
		if ((keystatus[0xc8] > 0) && (tilenum >= (xtiles<<gettilezoom)))
			tilenum-=(xtiles<<gettilezoom), keystatus[0xc8] = 0;
		if ((keystatus[0xd0] > 0) && (tilenum < MAXTILES-(xtiles<<gettilezoom)))
			tilenum+=(xtiles<<gettilezoom), keystatus[0xd0] = 0;
		if ((keystatus[0xc9] > 0) && (tilenum >= (xtiles<<gettilezoom)))
		{
			tilenum-=(tottiles<<(gettilezoom<<1));
			if (tilenum < 0) tilenum = 0;
			keystatus[0xc9] = 0;
		}
		if ((keystatus[0xd1] > 0) && (tilenum < MAXTILES-(xtiles<<gettilezoom)))
		{
			tilenum+=(tottiles<<(gettilezoom<<1));
			if (tilenum >= MAXTILES) tilenum = MAXTILES-1;
			keystatus[0xd1] = 0;
		}
		if (keystatus[0x2f] > 0)   //V
		{
			keystatus[0x2f] = 0;
			if (tilenum < localartlookupnum)
				tilenum = localartlookup[tilenum];
			else
				tilenum = 0;
			localartlookupnum = MAXTILES;
			std::iota(localartlookup.begin(), localartlookup.end(), 0);
		}
		if (keystatus[0x22] > 0)       //G (goto)
		{
			if (tilenum < localartlookupnum)         //Automatically press 'V'
				tilenum = localartlookup[tilenum];
			else
				tilenum = 0;
			localartlookupnum = MAXTILES;
			std::iota(localartlookup.begin(), localartlookup.end(), 0);

			keystatus[0x22] = 0;
			bflushchars();

			j = tilenum;
			while (keystatus[1] == 0) {
				if (handleevents()) {
					if (quitevent) quitevent = false;
				}

				const auto ch = bgetchar();

				//drawtilescreen(topleft,tilenum);
				auto snotbuf = fmt::format("Goto tile: {}_ ", j);
				printext256(0,0,whitecol,blackcol, snotbuf, 0);
				showframe();

				if (ch >= '0' && ch <= '9') {
					i = (j*10)+(ch-'0');
					if (i < MAXTILES) j = i;
				} else if (ch == 8) {
					j /= 10;
				} else if (ch == 13) {
					tilenum = j;
					break;
				}
			}

			clearkeys();
		}
		while (tilenum < topleft) topleft -= (xtiles<<gettilezoom);
		while (tilenum >= topleft+(tottiles<<(gettilezoom<<1))) topleft += (xtiles<<gettilezoom);
		if (topleft < 0) topleft = 0;
		if (topleft > MAXTILES-(tottiles<<(gettilezoom<<1))) topleft = MAXTILES-(tottiles<<(gettilezoom<<1));
	}

	if (keystatus[0x1c] == 0)
	{
		tilenum = otilenum;
	}
	else
	{
		if (tilenum < localartlookupnum)
		{
			tilenum = localartlookup[tilenum];
			if ((tilesizx[tilenum] == 0) || (tilesizy[tilenum] == 0))
				tilenum = otilenum;
		}
		else
			tilenum = otilenum;
	}
	keystatus[0x1] = 0;
	keystatus[0x1c] = 0;
	
	return tilenum;
}

void drawtilescreen(int pictopleft, int picbox)
{
	intptr_t vidpos;
	intptr_t vidpos2;
	int i;
	int j;
	int wallnum;
	int xdime;
	int ydime;
	int cnt;
	int pinc;
	int dax;
	int day;
	int scaledown;
	unsigned char *picptr;

	const int xtiles = xdim >> 6;
	const int ytiles = ydim >> 6;
	const int tottiles = xtiles * ytiles;

#if USE_POLYMOST && USE_OPENGL
	setpolymost2dview();	// JBF 20040205: set to 2d rendering
#endif

	pinc = ylookup[1];
	clearview(blackcol);
	for(cnt=0;cnt<(tottiles<<(gettilezoom<<1));cnt++)         //draw the 5*3 grid of tiles
	{
		wallnum = cnt+pictopleft;
		if (wallnum < localartlookupnum)
		{
			wallnum = localartlookup[wallnum];
			if ((tilesizx[wallnum] != 0) && (tilesizy[wallnum] != 0))
			{
				if (waloff[wallnum] == 0) loadtile(wallnum);
				picptr = (unsigned char *)(waloff[wallnum]);
				xdime = tilesizx[wallnum];
				ydime = tilesizy[wallnum];

				dax = ((cnt%(xtiles<<gettilezoom))<<(6-gettilezoom));
				day = ((cnt/(xtiles<<gettilezoom))<<(6-gettilezoom));
#if USE_POLYMOST && USE_OPENGL
				if (polymost_drawtilescreen(dax, day, wallnum, 64>>gettilezoom))
#endif
				{
					vidpos = ylookup[day]+dax+frameplace;
					if ((xdime <= (64>>gettilezoom)) && (ydime <= (64>>gettilezoom)))
					{
						for(i=0;i<xdime;i++)
						{
							vidpos2 = vidpos+i;
							for(j=0;j<ydime;j++)
							{
								*(unsigned char *)vidpos2 = *picptr++;
								vidpos2 += pinc;
							}
						}
					}
					else                          //if 1 dimension > 64
					{
						if (xdime > ydime)
							scaledown = ((xdime+(63>>gettilezoom))>>(6-gettilezoom));
						else
							scaledown = ((ydime+(63>>gettilezoom))>>(6-gettilezoom));

						for(i=0;i<xdime;i+=scaledown)
						{
							if (waloff[wallnum] == 0) loadtile(wallnum);
							picptr = (unsigned char *)(waloff[wallnum]) + ydime*i;
							vidpos2 = vidpos;
							for(j=0;j<ydime;j+=scaledown)
							{
								*(unsigned char *)vidpos2 = *picptr;
								picptr += scaledown;
								vidpos2 += pinc;
							}
							vidpos++;
						}
					}
				}
				if (localartlookupnum < MAXTILES)
				{
					dax = ((cnt%(xtiles<<gettilezoom))<<(6-gettilezoom));
					day = ((cnt/(xtiles<<gettilezoom))<<(6-gettilezoom));
					auto snotbuf = fmt::format("{}", localartfreq[cnt + pictopleft]);
					printext256(dax,day,whitecol,-1, snotbuf, 1);
				}
			}
		}
	}

	cnt = picbox-pictopleft;    //draw open white box
	dax = ((cnt%(xtiles<<gettilezoom))<<(6-gettilezoom));
	day = ((cnt/(xtiles<<gettilezoom))<<(6-gettilezoom));

	i = (63>>gettilezoom);
	drawline256((dax  )<<12, (day  )<<12, (dax+i)<<12, (day  )<<12, whitecol);
	drawline256((dax+i)<<12, (day  )<<12, (dax+i)<<12, (day+i)<<12, whitecol);
	drawline256((dax+i)<<12, (day+i)<<12, (dax  )<<12, (day+i)<<12, whitecol);
	drawline256((dax  )<<12, (day+i)<<12, (dax  )<<12, (day  )<<12, whitecol);

	i = localartlookup[picbox];
	auto artlook = fmt::format("{}", i);
	printext256(0L, ydim - 8, whitecol, -1, artlook, 0);
	printext256(xdim-((int)std::strlen(names[i])<<3),ydim-8,whitecol,-1,names[i],0);

	auto tilesizes = fmt::format("{}x{}", tilesizx[i], tilesizy[i]);
	printext256(xdim >> 2, ydim - 8, whitecol, -1, tilesizes, 0);
}

void overheadeditor()
{
	char buffer[80];
	const char *dabuffer;
	int i;
	int j;
	int k;
	int m=0;
	int mousxplc;
	int mousyplc;
	int firstx=0;
	int firsty=0;
	int oposz;
	int col;
	int ch;
	int sl;
	int templong;
	int templong1;
	int templong2;
	int doubvel;
	int startwall;
	int endwall;
	int dax;
	int day;
	int daz;
	int x1;
	int y1;
	int x2;
	int y2;
	int x3;
	int y3;
	int x4;
	int y4;
	int highlightx1{0};
	int highlighty1{0};
	int highlightx2{0};
	int highlighty2{0};
	int xvect;
	int yvect;
	short suckwall=0;
	short sucksect;
	short newnumwalls;
	short newnumsectors;
	short split=0;
	short bad;
	short splitsect=0;
	short danumwalls;
	short secondstartwall;
	short joinsectnum;
	std::array<short, 2> joinsector;
	short splitstartwall=0;
	short splitendwall;
	short loopnum;
	int mousx;
	int mousy;
	int bstatus;
	int circlerad;
	short circlewall;
	short circlepoints;
	short circleang1;
	short circleang2;
	int sectorhighlightx=0;
	int sectorhighlighty=0;
	short cursectorhighlight;
	short sectorhighlightstat;
	short hitsect;
	short hitwall;
	short hitsprite;
	int hitx;
	int hity;
	int hitz;
	walltype *wal;

	qsetmodeany(xdim2d,ydim2d);
	xdim2d = xdim;
	ydim2d = ydim;

	searchx = scale(searchx,xdim2d,xdimgame);
	searchy = scale(searchy,ydim2d-STATUS2DSIZ,ydimgame);
	oposz = posz;

	setstatusbarviewport();
	drawline16(0,0,xdim-1,0,7);
	drawline16(0,ydim16-1,xdim-1,ydim16-1,7);
	drawline16(0,0,0,ydim16-1,7);
	drawline16(xdim-1,0,xdim-1,ydim16-1,7);
	drawline16(0,24,xdim-1,24,7);
	drawline16(192,0,192,24,7);
	printext16(9L,9L,4,-1, kensig, 0);
	printext16(8L,8L,12,-1, kensig, 0);
	fmt::format_to(buffer, "Version: {}", build_version);
	printmessage16(buffer);
	drawline16(0,ydim16-1-24,xdim-1,ydim16-1-24,7);
	drawline16(256,ydim16-1-24,256,ydim16-1,7);

	highlightcnt = -1;
	cursectorhighlight = -1;

		//White out all bordering lines of grab that are
		//not highlighted on both sides
	for(i=highlightsectorcnt-1;i>=0;i--)
	{
		startwall = g_sector[highlightsector[i]].wallptr;
		endwall = startwall + g_sector[highlightsector[i]].wallnum;
		for(j=startwall;j<endwall;j++)
		{
			if (wall[j].nextwall >= 0)
			{
				for(k=highlightsectorcnt-1;k>=0;k--)
					if (highlightsector[k] == wall[j].nextsector)
						break;
				if (k < 0)
				{
					wall[wall[j].nextwall].nextwall = -1;
					wall[wall[j].nextwall].nextsector = -1;
					wall[j].nextwall = -1;
					wall[j].nextsector = -1;
				}
			}
		}
	}

	std::ranges::fill(show2dwall, 0);  // Clear all highlights
	std::ranges::fill(show2dsprite, 0);

	sectorhighlightstat = -1;
	newnumwalls = -1;
	joinsector[0] = -1;
	circlewall = -1;
	circlepoints = 7;
	bstatus = 0;
	keystatus[buildkeys[14]] = 0;
	while ((keystatus[buildkeys[14]]>>1) == 0)
	{
		if (handleevents()) {
			if (quitevent) {
				keystatus[1] = 1;
				quitevent = false;
			}
		}

		OSD_DispatchQueued();

		oldmousebstatus = bstatus;
		getmousevalues(&mousx,&mousy,&bstatus);
		{
		  const div_t ldx = div(mulscalen<16>(mousx<<16, msens) + mousexsurp, (1<<16)); mousx = ldx.quot; mousexsurp = ldx.rem;
		  const div_t ldy = div(mulscalen<16>(mousy<<16, msens) + mouseysurp, (1<<16)); mousy = ldy.quot; mouseysurp = ldy.rem;
		}
		searchx += mousx;
		searchy += mousy;
		if (searchx < 8) searchx = 8;
		if (searchx > xdim-8-1) searchx = xdim-8-1;
		if (searchy < 8) searchy = 8;
		if (searchy > ydim-8-1) searchy = ydim-8-1;

		if (keystatus[0x3b] > 0) posx--, keystatus[0x3b] = 0;
		if (keystatus[0x3c] > 0) posx++, keystatus[0x3c] = 0;
		if (keystatus[0x3d] > 0) posy--, keystatus[0x3d] = 0;
		if (keystatus[0x3e] > 0) posy++, keystatus[0x3e] = 0;
		if (keystatus[0x43] > 0) ang--, keystatus[0x43] = 0;
		if (keystatus[0x44] > 0) ang++, keystatus[0x44] = 0;
		if (angvel != 0)          //ang += angvel * constant
		{                         //ENGINE calculates angvel for you
			doubvel = synctics;
			if (keystatus[buildkeys[4]] > 0)  //Lt. shift makes turn velocity 50% faster
				doubvel += (synctics>>1);
			ang += ((angvel*doubvel)>>4);
			ang = (ang+2048)&2047;
		}
		if ((vel|svel) != 0)
		{
			doubvel = synctics;
			if (keystatus[buildkeys[4]] > 0)     //Lt. shift doubles forward velocity
				doubvel += synctics;
			xvect = 0, yvect = 0;
			if (vel != 0)
			{
				xvect += ((vel*doubvel*(int)sintable[(ang+2560)&2047])>>3);
				yvect += ((vel*doubvel*(int)sintable[(ang+2048)&2047])>>3);
			}
			if (svel != 0)
			{
				xvect += ((svel*doubvel*(int)sintable[(ang+2048)&2047])>>3);
				yvect += ((svel*doubvel*(int)sintable[(ang+1536)&2047])>>3);
			}
			clipmove(&posx,&posy,&posz,&cursectnum,xvect,yvect,kenswalldist,4L<<8,4L<<8,CLIPMASK0);
		}

		getpoint(searchx,searchy,&mousxplc,&mousyplc);
		linehighlight = getlinehighlight(mousxplc, mousyplc);

		if (newnumwalls >= numwalls)
		{
			dax = mousxplc;
			day = mousyplc;
			adjustmark(&dax,&day,newnumwalls);
			wall[newnumwalls].pt.x = dax;
			wall[newnumwalls].pt.y = day;
		}

		templong = numwalls;
		numwalls = newnumwalls;
		if (numwalls < 0) numwalls = templong;

		setoverheadviewport();
		clear2dscreen();
		draw2dgrid(posx,posy,ang,zoom,grid);

		x2 = mulscalen<14>(startposx-posx,zoom);          //Draw brown arrow (start)
		y2 = mulscalen<14>(startposy-posy,zoom);
		if (((halfxdim16+x2) >= 2) && ((halfxdim16+x2) <= xdim-3))
			if (((midydim16+y2) >= 2) && ((midydim16+y2) <= ydim16-3))
			{
				x1 = mulscalen<11>(sintable[(startang+2560)&2047],zoom) / 768;
				y1 = mulscalen<11>(sintable[(startang+2048)&2047],zoom) / 768;
				drawline16((halfxdim16+x2)+x1,(midydim16+y2)+y1,(halfxdim16+x2)-x1,(midydim16+y2)-y1,6);
				drawline16((halfxdim16+x2)+x1,(midydim16+y2)+y1,(halfxdim16+x2)+y1,(midydim16+y2)-x1,6);
				drawline16((halfxdim16+x2)+x1,(midydim16+y2)+y1,(halfxdim16+x2)-y1,(midydim16+y2)+x1,6);
			}

		draw2dscreen(posx,posy,ang,zoom,grid);

		if (showtags && (zoom >= 768))
		{
			for(i=0;i<numsectors;i++)
			{
				dabuffer = ExtGetSectorCaption(i);
				if (dabuffer[0] != 0)
				{
					dax = 0;   //Get average point of sector
					day = 0;
					startwall = g_sector[i].wallptr;
					endwall = startwall + g_sector[i].wallnum - 1;
					for(j=startwall;j<=endwall;j++)
					{
						dax += wall[j].pt.x;
						day += wall[j].pt.y;
					}
					if (endwall > startwall)
					{
						dax /= (endwall-startwall+1);
						day /= (endwall-startwall+1);
					}

					dax = mulscalen<14>(dax-posx,zoom);
					day = mulscalen<14>(day-posy,zoom);

					sl = (int)std::strlen(dabuffer);
					x1 = halfxdim16+dax-(sl<<1);
					y1 = midydim16+day-4;
					x2 = x1 + (sl<<2)+2;
					y2 = y1 + 7;
					if ((x1 >= 0) && (x2 < xdim) && (y1 >= 0) && (y2 < ydim16))
						printext16(x1, y1, 0, 7, dabuffer, 1);
				}
			}

			x3 = divscalen<14>(-halfxdim16,zoom)+posx;
			y3 = divscalen<14>(-(midydim16-4),zoom)+posy;
			x4 = divscalen<14>(halfxdim16,zoom)+posx;
			y4 = divscalen<14>(ydim16-(midydim16-4),zoom)+posy;

			for(i=numwalls-1,wal=&wall[i];i>=0;i--,wal--)
			{
					//Get average point of wall
				dax = ((wal->pt.x + wall[wal->point2].pt.x)>>1);
				day = ((wal->pt.y + wall[wal->point2].pt.y)>>1);
				if ((dax > x3) && (dax < x4) && (day > y3) && (day < y4))
				{
					dabuffer = ExtGetWallCaption(i);
					if (dabuffer[0] != 0)
					{
						dax = mulscalen<14>(dax-posx,zoom);
						day = mulscalen<14>(day-posy,zoom);
						sl = (int)std::strlen(dabuffer);
						x1 = halfxdim16+dax-(sl<<1);
						y1 = midydim16+day-4;
						x2 = x1 + (sl<<2)+2;
						y2 = y1 + 7;
						if ((x1 >= 0) && (x2 < xdim) && (y1 >= 0) && (y2 < ydim16))
							printext16(x1,y1,0,4,dabuffer,1);
					}
				}
			}

			i = 0; j = numsprites;
			while ((j > 0) && (i < MAXSPRITES))
			{
				if (sprite[i].statnum < MAXSTATUS)
				{
					dabuffer = ExtGetSpriteCaption(i);
					if (dabuffer[0] != 0)
					{
							//Get average point of sprite
						dax = sprite[i].x;
						day = sprite[i].y;

						dax = mulscalen<14>(dax-posx,zoom);
						day = mulscalen<14>(day-posy,zoom);

						sl = (int)std::strlen(dabuffer);
						x1 = halfxdim16+dax-(sl<<1);
						y1 = midydim16+day-4;
						x2 = x1 + (sl<<2)+2;
						y2 = y1 + 7;
						if ((x1 >= 0) && (x2 < xdim) && (y1 >= 0) && (y2 < ydim16))
						{
							if ((sprite[i].cstat&1) == 0) col = 3; else col = 5;
							printext16(x1,y1,0,col,dabuffer,1);
						}
					}
					j--;
				}
				i++;
			}
		}

		printcoords16(posx,posy,ang);

		numwalls = templong;

		if (highlightsectorcnt > 0)
			for(i=0;i<highlightsectorcnt;i++)
				fillsector(highlightsector[i],2);

		col = 15-((gridlock<<1)+gridlock);
		drawline16(searchx,searchy-8,searchx,searchy-1,col);
		drawline16(searchx+1,searchy-8,searchx+1,searchy-1,col);
		drawline16(searchx,searchy+2,searchx,searchy+9,col);
		drawline16(searchx+1,searchy+2,searchx+1,searchy+9,col);
		drawline16(searchx-8,searchy,searchx-1,searchy,col);
		drawline16(searchx-8,searchy+1,searchx-1,searchy+1,col);
		drawline16(searchx+2,searchy,searchx+9,searchy,col);
		drawline16(searchx+2,searchy+1,searchx+9,searchy+1,col);

			//Draw the white pixel closest to mouse cursor on linehighlight
		getclosestpointonwall(mousxplc,mousyplc,(int)linehighlight,&dax,&day);
		x2 = mulscalen<14>(dax-posx,zoom);
		y2 = mulscalen<14>(day-posy,zoom);
		if (wall[linehighlight].nextsector >= 0)
			drawline16(halfxdim16+x2,midydim16+y2,halfxdim16+x2,midydim16+y2,15);
		else
			drawline16(halfxdim16+x2,midydim16+y2,halfxdim16+x2,midydim16+y2,5);

		OSD_Draw();

		if (keystatus[88] > 0)   //F12
		{
			keystatus[88] = 0;
			/*
			j = ydim16; ydim16 = ydim;
			clear2dscreen();
			draw2dgrid(posx,posy,ang,zoom,grid);
			draw2dscreen(posx,posy,ang,zoom,grid);
			*/

			screencapture("captxxxx.tga",keystatus[0x2a]|keystatus[0x36]);

			/*
			ydim16 = j;
			clear2dscreen();
			draw2dgrid(posx,posy,ang,zoom,grid);
			draw2dscreen(posx,posy,ang,zoom,grid);
			*/
			showframe();
		}
		if (keystatus[0x30] > 0)  // B (clip Blocking xor) (2D)
		{
			pointhighlight = getpointhighlight(mousxplc, mousyplc);
			linehighlight = getlinehighlight(mousxplc, mousyplc);

			if ((pointhighlight&0xc000) == 16384)
			{
				sprite[pointhighlight&16383].cstat ^= 1;
				sprite[pointhighlight&16383].cstat &= ~256;
				sprite[pointhighlight&16383].cstat |= ((sprite[pointhighlight&16383].cstat&1)<<8);
				asksave = true;
			}
			else if (linehighlight >= 0)
			{
				wall[linehighlight].cstat ^= 1;
				wall[linehighlight].cstat &= ~64;
				if ((wall[linehighlight].nextwall >= 0) && ((keystatus[0x2a]|keystatus[0x36]) == 0))
				{
					wall[wall[linehighlight].nextwall].cstat &= ~(1+64);
					wall[wall[linehighlight].nextwall].cstat |= (wall[linehighlight].cstat&1);
				}
				asksave = true;
			}
			keystatus[0x30] = 0;
		}
		if (keystatus[0x21] > 0)  //F (F alone does nothing in 2D right now)
		{
			keystatus[0x21] = 0;
			if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT-F (relative alignmment flip)
			{
				linehighlight = getlinehighlight(mousxplc, mousyplc);
				if (linehighlight >= 0)
				{
					setfirstwall(sectorofwall(linehighlight),linehighlight);
					asksave = true;
					printmessage16("This wall now sector's first wall (g_sector[].wallptr)");
				}
			}
		}

		if (keystatus[0x18] > 0)  // O (ornament onto wall) (2D)
		{
			keystatus[0x18] = 0;
			if ((pointhighlight&0xc000) == 16384)
			{
				asksave = true;
				i = (pointhighlight&16383);

				hitscan(sprite[i].x,sprite[i].y,sprite[i].z,sprite[i].sectnum,
					sintable[(sprite[i].ang+2560+1024)&2047],
					sintable[(sprite[i].ang+2048+1024)&2047],
					0,
					&hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,CLIPMASK1);

				sprite[i].x = hitx;
				sprite[i].y = hity;
				sprite[i].z = hitz;
				changespritesect(i,hitsect);
				if (hitwall >= 0)
					sprite[i].ang = ((getangle(wall[wall[hitwall].point2].pt.x - wall[hitwall].pt.x, wall[wall[hitwall].point2].pt.y - wall[hitwall].pt.y) + 512) & 2047);

					//Make sure sprite's in right sector
				if (inside(sprite[i].x,sprite[i].y,sprite[i].sectnum) == 0)
				{
					j = wall[hitwall].point2;
					sprite[i].x -= ksgn(wall[j].pt.y-wall[hitwall].pt.y);
					sprite[i].y += ksgn(wall[j].pt.x-wall[hitwall].pt.x);
				}
			}
		}

		if (keystatus[0x33] > 0)  // , (2D)
		{
			if (highlightsectorcnt > 0)
			{
				k = 0;
				dax = 0;
				day = 0;
				for(i=0;i<highlightsectorcnt;i++)
				{
					startwall = g_sector[highlightsector[i]].wallptr;
					endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
					for(j=startwall;j<=endwall;j++)
					{
						dax += wall[j].pt.x;
						day += wall[j].pt.y;
						k++;
					}
				}
				if (k > 0)
				{
					dax /= k;
					day /= k;
				}

				k = (keystatus[0x2a]|keystatus[0x36]);

				if (k == 0)
				{
					if ((gridlock > 0) && (grid > 0))
					{
						dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
						day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
					}
				}

				for(i=0;i<highlightsectorcnt;i++)
				{
					startwall = g_sector[highlightsector[i]].wallptr;
					endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
					for(j=startwall;j<=endwall;j++)
					{
						if (k == 0)
						{
							x3 = wall[j].pt.x;
							y3 = wall[j].pt.y;
							wall[j].pt.x = dax+day-y3;
							wall[j].pt.y = day+x3-dax;
						}
						else
						{
							rotatepoint(dax,day,wall[j].pt.x, wall[j].pt.y, 1, &wall[j].pt.x, &wall[j].pt.y);
						}
					}

					j = headspritesect[highlightsector[i]];
					while (j != -1)
					{
						if (k == 0)
						{
							x3 = sprite[j].x;
							y3 = sprite[j].y;
							sprite[j].x = dax+day-y3;
							sprite[j].y = day+x3-dax;
							sprite[j].ang = ((sprite[j].ang+512)&2047);
						}
						else
						{
							rotatepoint(dax,day,sprite[j].x,sprite[j].y,1,&sprite[j].x,&sprite[j].y);
							sprite[j].ang = ((sprite[j].ang+1)&2047);
						}

						j = nextspritesect[j];
					}
				}
				if (k == 0) keystatus[0x33] = 0;
				asksave = true;
			}
			else
			{
				if (pointhighlight >= 16384)
				{
					i = pointhighlight-16384;
					if ((keystatus[0x2a]|keystatus[0x36]) > 0)
						sprite[i].ang = ((sprite[i].ang+2048-1)&2047);
					else
					{
						sprite[i].ang = ((sprite[i].ang+2048-128)&2047);
						keystatus[0x33] = 0;
					}

					clearmidstatbar16();
					showspritedata((short)pointhighlight-16384);
				}
			}
		}
		if (keystatus[0x34] > 0)  // .  (2D)
		{
			if (highlightsectorcnt > 0)
			{
				k = 0;
				dax = 0;
				day = 0;
				for(i=0;i<highlightsectorcnt;i++)
				{
					startwall = g_sector[highlightsector[i]].wallptr;
					endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
					for(j=startwall;j<=endwall;j++)
					{
						dax += wall[j].pt.x;
						day += wall[j].pt.y;
						k++;
					}
				}
				if (k > 0)
				{
					dax /= k;
					day /= k;
				}

				k = (keystatus[0x2a]|keystatus[0x36]);

				if (k == 0)
				{
					if ((gridlock > 0) && (grid > 0))
					{
						dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
						day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
					}
				}

				for(i=0;i<highlightsectorcnt;i++)
				{
					startwall = g_sector[highlightsector[i]].wallptr;
					endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
					for(j=startwall;j<=endwall;j++)
					{
						if (k == 0)
						{
							x3 = wall[j].pt.x;
							y3 = wall[j].pt.y;
							wall[j].pt.x = dax+y3-day;
							wall[j].pt.y = day+dax-x3;
						}
						else
						{
							rotatepoint(dax,day,wall[j].pt.x,wall[j].pt.y,2047,&wall[j].pt.x,&wall[j].pt.y);
						}
					}

					j = headspritesect[highlightsector[i]];
					while (j != -1)
					{
						if (k == 0)
						{
							x3 = sprite[j].x;
							y3 = sprite[j].y;
							sprite[j].x = dax+y3-day;
							sprite[j].y = day+dax-x3;
							sprite[j].ang = ((sprite[j].ang+1536)&2047);
						}
						else
						{
							rotatepoint(dax,day,sprite[j].x,sprite[j].y,2047,&sprite[j].x,&sprite[j].y);
							sprite[j].ang = ((sprite[j].ang+2047)&2047);
						}

						j = nextspritesect[j];
					}
				}
				if (k == 0) keystatus[0x34] = 0;
				asksave = true;
			}
			else
			{
				if (pointhighlight >= 16384)
				{
					i = pointhighlight-16384;
					if ((keystatus[0x2a]|keystatus[0x36]) > 0)
						sprite[i].ang = ((sprite[i].ang+2048+1)&2047);
					else
					{
						sprite[i].ang = ((sprite[i].ang+2048+128)&2047);
						keystatus[0x34] = 0;
					}

					clearmidstatbar16();
					showspritedata((short)pointhighlight-16384);
				}
			}
		}
		if (keystatus[0x46] > 0)  //Scroll lock (set starting position)
		{
			startposx = posx;
			startposy = posy;
			startposz = posz;
			startang = ang;
			startsectnum = cursectnum;
			keystatus[0x46] = 0;
			asksave = true;
		}

		setstatusbarviewport();

		if (keystatus[0x3f] > 0)  //F5
		{
			keystatus[0x3f] = 0;

			for (i=0;i<numsectors;i++)
				if (inside(mousxplc,mousyplc,i) == 1)
				{
					ExtShowSectorData((short)i);
					break;
				}
		}
		if (keystatus[0x40] > 0)  //F6
		{
			keystatus[0x40] = 0;

			if (pointhighlight >= 16384)
			{
				i = pointhighlight-16384;
				ExtShowSpriteData((short)i);
			}
			else if (linehighlight >= 0)
			{
				i = linehighlight;
				ExtShowWallData((short)i);
			}
		}
		if (keystatus[0x41] > 0)  //F7
		{
			keystatus[0x41] = 0;

			for (i=0;i<numsectors;i++)
				if (inside(mousxplc,mousyplc,i) == 1)
				{
					ExtEditSectorData((short)i);
					break;
				}
		}
		if (keystatus[0x42] > 0)  //F8
		{
			keystatus[0x42] = 0;

			if (pointhighlight >= 16384)
			{
				i = pointhighlight-16384;
				ExtEditSpriteData((short)i);
			}
			else if (linehighlight >= 0)
			{
				i = linehighlight;
				ExtEditWallData((short)i);
			}
		}

		if (keystatus[0x14] > 0)  // T (tag)
		{
			keystatus[0x14] = 0;
			if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //Ctrl-T
			{
				showtags = !showtags;
				if (!showtags)
					printmessage16("Show tags OFF");
				else
					printmessage16("Show tags ON");
			}
			else if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
			{
				if (pointhighlight >= 16384)
				{
					i = pointhighlight-16384;
					fmt::format_to(buffer, "Sprite ({}) Lo-tag: ", i);
					sprite[i].lotag = getnumber16(buffer,sprite[i].lotag,65536L,0);
					clearmidstatbar16();
					showspritedata((short)i);
				}
				else if (linehighlight >= 0)
				{
					i = linehighlight;
					fmt::format_to(buffer, "Wall ({}) Lo-tag: ", i);
					wall[i].lotag = getnumber16(buffer,wall[i].lotag,65536L,0);
					clearmidstatbar16();
					showwalldata((short)i);
				}
				printmessage16("");
			}
			else
			{
				for (i=0;i<numsectors;i++)
					if (inside(mousxplc,mousyplc,i) == 1)
					{
						fmt::format_to(buffer, "Sector ({}) Lo-tag: ", i);
						g_sector[i].lotag = getnumber16(buffer,g_sector[i].lotag,65536L,0);
						clearmidstatbar16();
						showsectordata((short)i);
						break;
					}
				printmessage16("");
			}
		}
		if (keystatus[0x23] > 0)  //H (Hi 16 bits of tag)
		{
			keystatus[0x23] = 0;
			if ((keystatus[0x1d]|keystatus[0x9d]) > 0)  //Ctrl-H
			{
				pointhighlight = getpointhighlight(mousxplc, mousyplc);
				linehighlight = getlinehighlight(mousxplc, mousyplc);

				if ((pointhighlight&0xc000) == 16384)
				{
					sprite[pointhighlight&16383].cstat ^= 256;
					asksave = true;
				}
				else if (linehighlight >= 0)
				{
					wall[linehighlight].cstat ^= 64;
					if ((wall[linehighlight].nextwall >= 0) && ((keystatus[0x2a]|keystatus[0x36]) == 0))
					{
						wall[wall[linehighlight].nextwall].cstat &= ~64;
						wall[wall[linehighlight].nextwall].cstat |= (wall[linehighlight].cstat&64);
					}
					asksave = true;
				}
			}
			else if ((keystatus[0x38]|keystatus[0xb8]) > 0)  //ALT
			{
				if (pointhighlight >= 16384)
				{
					i = pointhighlight-16384;
					fmt::format_to(buffer, "Sprite ({}) Hi-tag: ", i);
					sprite[i].hitag = getnumber16(buffer,sprite[i].hitag,65536L,0);
					clearmidstatbar16();
					showspritedata((short)i);
				}
				else if (linehighlight >= 0)
				{
					i = linehighlight;
					fmt::format_to(buffer, "Wall ({}) Hi-tag: ", i);
					wall[i].hitag = getnumber16(buffer,wall[i].hitag,65536L,0);
					clearmidstatbar16();
					showwalldata((short)i);
				}
			}
			else
			{
				for (i=0;i<numsectors;i++)
					if (inside(mousxplc,mousyplc,i) == 1)
					{
						fmt::format_to(buffer, "Sector ({}) Hi-tag: ", i);
						g_sector[i].hitag = getnumber16(buffer,g_sector[i].hitag,65536L,0);
						clearmidstatbar16();
						showsectordata((short)i);
						break;
					}
			}
			printmessage16("");
		}
		if (keystatus[0x19] > 0)  // P (palookup #)
		{
			keystatus[0x19] = 0;

			for (i=0;i<numsectors;i++)
				if (inside(mousxplc,mousyplc,i) == 1)
				{
					fmt::format_to(buffer, "Sector ({}) Ceilingpal: ", i);
					g_sector[i].ceilingpal = getnumber16(buffer,g_sector[i].ceilingpal,256L,0);
					clearmidstatbar16();
					showsectordata((short)i);

					fmt::format_to(buffer, "Sector ({}) Floorpal: ", i);
					g_sector[i].floorpal = getnumber16(buffer,g_sector[i].floorpal,256L,0);
					clearmidstatbar16();
					showsectordata((short)i);

					printmessage16("");
					break;
				}
		}
		if (keystatus[0x12] > 0)  // E (status list)
		{
			if (pointhighlight >= 16384)
			{
				i = pointhighlight-16384;
				fmt::format_to(buffer, "Sprite ({}) Status list: ", i);
				changespritestat(i,getnumber16(buffer,sprite[i].statnum,65536L,0));
				clearmidstatbar16();
				showspritedata((short)i);
			}

			printmessage16("");

			keystatus[0x12] = 0;
		}

		if (keystatus[0x0f] > 0)  //TAB
		{
			clearmidstatbar16();

			if ((keystatus[0x38]|keystatus[0xb8]|keystatus[0x1d]|keystatus[0x9d]) > 0)  //ALT or CTRL
			{
				if (pointhighlight >= 16384)
					showspritedata((short)pointhighlight-16384);
				else if (linehighlight >= 0)
					showwalldata((short)linehighlight);
			}
			else
			{
				for (i=0;i<numsectors;i++)
					if (inside(mousxplc,mousyplc,i) == 1)
					{
						showsectordata((short)i);
						break;
					}
			}
			keystatus[0x0f] = 0;
		}

		setoverheadviewport();

		if (highlightsectorcnt < 0)
		{
			if (keystatus[0x36] > 0)  //Right shift (point highlighting)
			{
				if (highlightcnt == 0)
				{
					highlightx2 = searchx, highlighty2 = searchy;
					drawline16(highlightx2,highlighty1,highlightx1,highlighty1,5);
					drawline16(highlightx2,highlighty2,highlightx1,highlighty2,5);
					drawline16(highlightx1,highlighty2,highlightx1,highlighty1,5);
					drawline16(highlightx2,highlighty2,highlightx2,highlighty1,5);
				}
				if (highlightcnt != 0)
				{
					highlightx1 = searchx;
					highlighty1 = searchy;
					highlightx2 = searchx;
					highlighty2 = searchx;
					highlightcnt = 0;

					std::ranges::fill(show2dwall, 0);  // Clear all highlights
					std::ranges::fill(show2dsprite, 0);
				}
			}
			else
			{
				if (highlightcnt == 0)
				{
					getpoint(highlightx1,highlighty1,&highlightx1,&highlighty1);
					getpoint(highlightx2,highlighty2,&highlightx2,&highlighty2);
					if (highlightx1 > highlightx2)
					{
						std::swap(highlightx1, highlightx2);
					}
					if (highlighty1 > highlighty2)
					{
						std::swap(highlighty1, highlighty2);
					}

					if ((keystatus[0x1d]|keystatus[0x9d]) > 0)
					{
						if ((linehighlight >= 0) && (linehighlight < MAXWALLS))
						{
							i = linehighlight;
							do
							{
								highlight[highlightcnt++] = i;
								show2dwall[i>>3] |= (1<<(i&7));

								for(j=0;j<numwalls;j++)
									if (wall[j].pt.x == wall[i].pt.x)
										if (wall[j].pt.y == wall[i].pt.y)
											if (i != j)
											{
												highlight[highlightcnt++] = j;
												show2dwall[j>>3] |= (1<<(j&7));
											}

								i = wall[i].point2;
							}
							while (i != linehighlight);
						}
					}
					else
					{
						for(i=0;i<numwalls;i++)
							if ((wall[i].pt.x >= highlightx1) && (wall[i].pt.x <= highlightx2))
								if ((wall[i].pt.y >= highlighty1) && (wall[i].pt.y <= highlighty2))
								{
									highlight[highlightcnt++] = i;
									show2dwall[i>>3] |= (1<<(i&7));
								}
						for(i=0;i<MAXSPRITES;i++)
							if (sprite[i].statnum < MAXSTATUS)
								if ((sprite[i].x >= highlightx1) && (sprite[i].x <= highlightx2))
									if ((sprite[i].y >= highlighty1) && (sprite[i].y <= highlighty2))
									{
										highlight[highlightcnt++] = i+16384;
										show2dsprite[i>>3] |= (1<<(i&7));
									}
					}

					if (highlightcnt <= 0)
						highlightcnt = -1;
				}
			}
		}
		if (highlightcnt < 0)
		{
			if (keystatus[0xb8] > 0)  //Right alt (sector highlighting)
			{
				if (highlightsectorcnt == 0)
				{
					highlightx2 = searchx, highlighty2 = searchy;
					drawline16(highlightx2,highlighty1,highlightx1,highlighty1,10);
					drawline16(highlightx2,highlighty2,highlightx1,highlighty2,10);
					drawline16(highlightx1,highlighty2,highlightx1,highlighty1,10);
					drawline16(highlightx2,highlighty2,highlightx2,highlighty1,10);
				}
				if (highlightsectorcnt != 0)
				{
					for(i=0;i<highlightsectorcnt;i++)
					{
						startwall = g_sector[highlightsector[i]].wallptr;
						endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
						for(j=startwall;j<=endwall;j++)
						{
							if (wall[j].nextwall >= 0)
								checksectorpointer(wall[j].nextwall,wall[j].nextsector);
							checksectorpointer((short)j,highlightsector[i]);
						}
					}
					highlightx1 = searchx;
					highlighty1 = searchy;
					highlightx2 = searchx;
					highlighty2 = searchx;
					highlightsectorcnt = 0;
				}
			}
			else
			{
				if (highlightsectorcnt == 0)
				{
					getpoint(highlightx1,highlighty1,&highlightx1,&highlighty1);
					getpoint(highlightx2,highlighty2,&highlightx2,&highlighty2);
					if (highlightx1 > highlightx2)
					{
						std::swap(highlightx1, highlightx2);
					}
					if (highlighty1 > highlighty2)
					{
						std::swap(highlighty1, highlighty2);
					}

					for(i=0;i<numsectors;i++)
					{
						startwall = g_sector[i].wallptr;
						endwall = startwall + g_sector[i].wallnum;
						bad = 0;
						for(j=startwall;j<endwall;j++)
						{
							if (wall[j].pt.x < highlightx1) bad = 1;
							if (wall[j].pt.x > highlightx2) bad = 1;
							if (wall[j].pt.y < highlighty1) bad = 1;
							if (wall[j].pt.y > highlighty2) bad = 1;
							if (bad == 1) break;
						}
						if (bad == 0)
							highlightsector[highlightsectorcnt++] = i;
					}
					if (highlightsectorcnt <= 0)
						highlightsectorcnt = -1;

						//White out all bordering lines of grab that are
						//not highlighted on both sides
					for(i=highlightsectorcnt-1;i>=0;i--)
					{
						startwall = g_sector[highlightsector[i]].wallptr;
						endwall = startwall + g_sector[highlightsector[i]].wallnum;
						for(j=startwall;j<endwall;j++)
						{
							if (wall[j].nextwall >= 0)
							{
								for(k=highlightsectorcnt-1;k>=0;k--)
									if (highlightsector[k] == wall[j].nextsector)
										break;
								if (k < 0)
								{
									wall[wall[j].nextwall].nextwall = -1;
									wall[wall[j].nextwall].nextsector = -1;
									wall[j].nextwall = -1;
									wall[j].nextsector = -1;
								}
							}
						}
					}

				}
			}
		}

		if (((bstatus&1) < (oldmousebstatus&1)) && (highlightsectorcnt < 0))  //after dragging
		{
			j = 1;
			if (highlightcnt > 0)
				for (i=0;i<highlightcnt;i++)
					if (pointhighlight == highlight[i])
					{
						j = 0;
						break;
					}

			if (j == 0)
			{
				for(i=0;i<highlightcnt;i++)
				{
					if ((highlight[i]&0xc000) == 16384)
					{
						j = (highlight[i]&16383);

						setsprite(j,sprite[j].x,sprite[j].y,sprite[j].z);

						templong = ((tilesizy[sprite[j].picnum]*sprite[j].yrepeat)<<2);
						sprite[j].z = std::max(sprite[j].z, getceilzofslope(sprite[j].sectnum,sprite[j].x, sprite[j].y) + templong);
						sprite[j].z = std::min(sprite[j].z, getflorzofslope(sprite[j].sectnum,sprite[j].x, sprite[j].y));
					}
				}
			}
			else if ((pointhighlight&0xc000) == 16384)
			{
				j = (pointhighlight&16383);

				setsprite(j,sprite[j].x,sprite[j].y,sprite[j].z);

				templong = ((tilesizy[sprite[j].picnum]*sprite[j].yrepeat)<<2);

				sprite[j].z = std::max(sprite[j].z, getceilzofslope(sprite[j].sectnum,sprite[j].x, sprite[j].y) + templong);
				sprite[j].z = std::min(sprite[j].z, getflorzofslope(sprite[j].sectnum,sprite[j].x, sprite[j].y));
			}

			if ((pointhighlight&0xc000) == 0)
			{
				dax = wall[pointhighlight].pt.x;
				day = wall[pointhighlight].pt.y;
			}
			else if ((pointhighlight&0xc000) == 16384)
			{
				dax = sprite[pointhighlight&16383].x;
				day = sprite[pointhighlight&16383].y;
			}

			for(i=numwalls-1;i>=0;i--)     //delete points
			{
				if (wall[i].pt.x == wall[wall[i].point2].pt.x)
					if (wall[i].pt.y == wall[wall[i].point2].pt.y)
					{
						deletepoint((short)i);
						printmessage16("Point deleted.");
						asksave = true;
					}
			}
			for(i=0;i<numwalls;i++)        //make new red lines?
			{
				if ((wall[i].pt.x == dax) && (wall[i].pt.y == day))
				{
					checksectorpointer((short)i,sectorofwall((short)i));
					fixrepeats((short)i);
					asksave = true;
				}
				else if ((wall[wall[i].point2].pt.x == dax) && (wall[wall[i].point2].pt.y == day))
				{
					checksectorpointer((short)i,sectorofwall((short)i));
					fixrepeats((short)i);
					asksave = true;
				}
			}

		}

		if ((bstatus&1) > 0)                //drag points
		{
			if (highlightsectorcnt > 0)
			{
				if ((bstatus&1) > (oldmousebstatus&1))
				{
					newnumwalls = -1;
					sectorhighlightstat = -1;
					updatesector(mousxplc,mousyplc,&cursectorhighlight);

					if ((cursectorhighlight >= 0) && (cursectorhighlight < numsectors))
					{
						for (i=0;i<highlightsectorcnt;i++)
							if (cursectorhighlight == highlightsector[i])
							{
									//You clicked inside one of the flashing sectors!
								sectorhighlightstat = 1;

								dax = mousxplc;
								day = mousyplc;
								if ((gridlock > 0) && (grid > 0))
								{
									dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
									day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
								}
								sectorhighlightx = dax;
								sectorhighlighty = day;
								break;
							}
					}
				}
				else if (sectorhighlightstat == 1)
				{
					dax = mousxplc;
					day = mousyplc;
					if ((gridlock > 0) && (grid > 0))
					{
						dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
						day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
					}

					dax -= sectorhighlightx;
					day -= sectorhighlighty;
					sectorhighlightx += dax;
					sectorhighlighty += day;

					for(i=0;i<highlightsectorcnt;i++)
					{
						startwall = g_sector[highlightsector[i]].wallptr;
						endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
						for(j=startwall;j<=endwall;j++)
							{ wall[j].pt.x += dax; wall[j].pt.y += day; }

						for(j=headspritesect[highlightsector[i]];j>=0;j=nextspritesect[j])
							{ sprite[j].x += dax; sprite[j].y += day; }
					}

					//for(i=0;i<highlightsectorcnt;i++)
					//{
					//   startwall = g_sector[highlightsector[i]].wallptr;
					//   endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
					//   for(j=startwall;j<=endwall;j++)
					//   {
					//      if (wall[j].nextwall >= 0)
					//         checksectorpointer(wall[j].nextwall,wall[j].nextsector);
					//     checksectorpointer((short)j,highlightsector[i]);
					//   }
					//}
					asksave = true;
				}

			}
			else
			{
				if ((bstatus&1) > (oldmousebstatus&1))
					pointhighlight = getpointhighlight(mousxplc, mousyplc);

				if (pointhighlight >= 0)
				{
					dax = mousxplc;
					day = mousyplc;
					if ((gridlock > 0) && (grid > 0))
					{
						dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
						day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
					}

					j = 1;
					if (highlightcnt > 0)
						for (i=0;i<highlightcnt;i++)
							if (pointhighlight == highlight[i])
								{ j = 0; break; }

					if (j == 0)
					{
						if ((pointhighlight&0xc000) == 0)
						{
							dax -= wall[pointhighlight].pt.x;
							day -= wall[pointhighlight].pt.y;
						}
						else
						{
							dax -= sprite[pointhighlight&16383].x;
							day -= sprite[pointhighlight&16383].y;
						}
						for(i=0;i<highlightcnt;i++)
						{
							if ((highlight[i]&0xc000) == 0)
							{
								wall[highlight[i]].pt.x += dax;
								wall[highlight[i]].pt.y += day;
							}
							else
							{
								sprite[highlight[i]&16383].x += dax;
								sprite[highlight[i]&16383].y += day;
							}
						}
					}
					else
					{
						if ((pointhighlight&0xc000) == 0)
							dragpoint(pointhighlight,dax,day);
						else if ((pointhighlight&0xc000) == 16384)
						{
							daz = ((tilesizy[sprite[pointhighlight&16383].picnum]*sprite[pointhighlight&16383].yrepeat)<<2);

							for(i=0;i<numsectors;i++)
								if (inside(dax,day,i) == 1)
									if (sprite[pointhighlight&16383].z >= getceilzofslope(i,dax,day))
										if (sprite[pointhighlight&16383].z-daz <= getflorzofslope(i,dax,day))
										{
											sprite[pointhighlight&16383].x = dax;
											sprite[pointhighlight&16383].y = day;
											if (sprite[pointhighlight&16383].sectnum != i)
												changespritesect(pointhighlight&16383,(short)i);
											break;
										}
						}
					}
					asksave = true;
				}
			}
		}
		else
		{
			pointhighlight = getpointhighlight(mousxplc, mousyplc);
			sectorhighlightstat = -1;
		}

		if ((bstatus&6) > 0)
		{
			searchx = halfxdim16;
			searchy = midydim16;
			posx = mousxplc;
			posy = mousyplc;
		}

		if (((keystatus[buildkeys[8]] > 0) || (bstatus&16)) && (zoom < 16384)) zoom += synctics*(zoom>>4);
		if (((keystatus[buildkeys[9]] > 0) || (bstatus&32)) && (zoom > 24)) zoom -= synctics*(zoom>>4);

		if (keystatus[0x22] > 0)  // G (grid on/off)
		{
			grid++;
			if (grid == 7) grid = 0;
			keystatus[0x22] = 0;
		}
		if (keystatus[0x26] > 0)  // L (grid lock)
		{
			gridlock = 1-gridlock, keystatus[0x26] = 0;
			if (gridlock == 0)
				printmessage16("Grid locking OFF");
			else
				printmessage16("Grid locking ON");
		}

		if (keystatus[0x24] > 0)  // J (join sectors)
		{
			if (joinsector[0] >= 0)
			{
				joinsector[1] = -1;
				for(i=0;i<numsectors;i++)
					if (inside(mousxplc,mousyplc,i) == 1)
					{
						joinsector[1] = i;
						break;
					}
				if ((joinsector[1] >= 0) && (joinsector[0] != joinsector[1]))
				{
					newnumwalls = numwalls;

					for(k=0;k<2;k++)
					{
						startwall = g_sector[joinsector[k]].wallptr;
						endwall = startwall + g_sector[joinsector[k]].wallnum - 1;
						for(j=startwall;j<=endwall;j++)
						{
							if (wall[j].cstat == 255)
								continue;
							joinsectnum = k;
							if (wall[j].nextsector == joinsector[1-joinsectnum])
							{
								wall[j].cstat = 255;
								continue;
							}

							i = j;
							m = newnumwalls;
							do
							{
								std::memcpy(&wall[newnumwalls],&wall[i],sizeof(walltype));
								wall[newnumwalls].point2 = newnumwalls+1;
								newnumwalls++;
								wall[i].cstat = 255;

								i = wall[i].point2;
								if (wall[i].nextsector == joinsector[1-joinsectnum])
								{
									i = wall[wall[i].nextwall].point2;
									joinsectnum = 1 - joinsectnum;
								}
							}
							while ((wall[i].cstat != 255) && (wall[i].nextsector != joinsector[1-joinsectnum]));
							wall[newnumwalls-1].point2 = m;
						}
					}

					if (newnumwalls > numwalls)
					{
						std::memcpy(&g_sector[numsectors],&g_sector[joinsector[0]],sizeof(sectortype));
						g_sector[numsectors].wallptr = numwalls;
						g_sector[numsectors].wallnum = newnumwalls-numwalls;

						//fix sprites
						for(i=0;i<2;i++)
						{
							j = headspritesect[joinsector[i]];
							while (j != -1)
							{
								k = nextspritesect[j];
								changespritesect(j,numsectors);
								j = k;
							}
						}

						numsectors++;

						for(i=numwalls;i<newnumwalls;i++)
						{
							if (wall[i].nextwall >= 0)
							{
								wall[wall[i].nextwall].nextwall = i;
								wall[wall[i].nextwall].nextsector = numsectors-1;
							}
						}

						numwalls = newnumwalls;
						newnumwalls = -1;

						for(k=0;k<2;k++)
						{
							startwall = g_sector[joinsector[k]].wallptr;
							endwall = startwall + g_sector[joinsector[k]].wallnum - 1;
							for(j=startwall;j<=endwall;j++)
							{
								wall[j].nextwall = -1;
								wall[j].nextsector = -1;
							}
						}

						deletesector((short)joinsector[0]);
						if (joinsector[0] < joinsector[1])
							joinsector[1]--;
						deletesector((short)joinsector[1]);
						printmessage16("Sectors joined.");
					}
				}
				joinsector[0] = -1;
			}
			else
			{
				joinsector[0] = -1;
				for(i=0;i<numsectors;i++)
					if (inside(mousxplc,mousyplc,i) == 1)
					{
						joinsector[0] = i;
						printmessage16("Join sector - press J again on sector to join with.");
						break;
					}
			}
			keystatus[0x24] = 0;
		}

		if (((keystatus[0x38]|keystatus[0xb8])&keystatus[0x1f]) > 0) //ALT-S
		{
			if ((linehighlight >= 0) && (wall[linehighlight].nextwall == -1))
			{
				if ((newnumwalls = whitelinescan(linehighlight)) < numwalls)
				{
					printmessage16("Can't make a sector out there.");
				}
				else
				{
					for(i=numwalls;i<newnumwalls;i++)
					{
						wall[wall[i].nextwall].nextwall = i;
						wall[wall[i].nextwall].nextsector = numsectors;
					}
					numwalls = newnumwalls;
					newnumwalls = -1;
					numsectors++;
					printmessage16("Inner loop made into new sector.");
				}
			}
			keystatus[0x1f] = 0;
		}
		else if (keystatus[0x1f] > 0)  //S
		{
			sucksect = -1;
			for(i=0;i<numsectors;i++)
				if (inside(mousxplc,mousyplc,i) == 1)
				{
					sucksect = i;
					break;
				}

			if (sucksect >= 0)
			{
				dax = mousxplc;
				day = mousyplc;
				if ((gridlock > 0) && (grid > 0))
				{
					dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
					day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
				}

				i = insertsprite(sucksect,0);
				sprite[i].x = dax, sprite[i].y = day;
				sprite[i].cstat = defaultspritecstat;
				sprite[i].shade = 0;
				sprite[i].pal = 0;
				sprite[i].xrepeat = 64, sprite[i].yrepeat = 64;
				sprite[i].xoffset = 0, sprite[i].yoffset = 0;
				sprite[i].ang = 1536;
				sprite[i].xvel = 0; sprite[i].yvel = 0; sprite[i].zvel = 0;
				sprite[i].owner = -1;
				sprite[i].clipdist = 32;
				sprite[i].lotag = 0;
				sprite[i].hitag = 0;
				sprite[i].extra = -1;

				sprite[i].z = getflorzofslope(sucksect,dax,day);
				if ((sprite[i].cstat&128) != 0)
					sprite[i].z -= ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);

				std::ranges::fill(localartfreq, 0);

				for(k=0;k<MAXSPRITES;k++)
					if (sprite[k].statnum < MAXSTATUS)
						localartfreq[sprite[k].picnum]++;
				j = 0;
				for(k=0;k<MAXTILES;k++)
					if (localartfreq[k] > localartfreq[j])
						j = k;
				if (localartfreq[j] > 0)
					sprite[i].picnum = j;
				else
					sprite[i].picnum = 0;

				if (somethingintab == 3)
				{
					sprite[i].picnum = temppicnum;
					if ((tilesizx[temppicnum] <= 0) || (tilesizy[temppicnum] <= 0))
					{
						j = 0;
						for(k=0;k<MAXTILES;k++)
							if ((tilesizx[k] > 0) && (tilesizy[k] > 0))
							{
								j = k;
								break;
							}
						sprite[i].picnum = j;
					}
					sprite[i].shade = tempshade;
					sprite[i].pal = temppal;
					sprite[i].xrepeat = tempxrepeat;
					sprite[i].yrepeat = tempyrepeat;
					if (sprite[i].xrepeat < 1) sprite[i].xrepeat = 1;
					if (sprite[i].yrepeat < 1) sprite[i].yrepeat = 1;
					sprite[i].cstat = tempcstat;
				}

				if (tilesizy[sprite[i].picnum] >= 32)
					sprite[i].cstat |= 1;

				printmessage16("Sprite inserted.");
				updatenumsprites();
				asksave = true;
			}

			keystatus[0x1f] = 0;
		}

		if (keystatus[0x2e] > 0)  // C (make circle of points)
		{
			if (circlewall >= 0)
			{
				circlewall = -1;
			}
			else
			{
				if (linehighlight >= 0)
					circlewall = linehighlight;
			}
			keystatus[0x2e] = 0;
		}
		if (keystatus[0x4a] > 0)  // -
		{
			if (circlepoints > 1)
				circlepoints--;
			keystatus[0x4a] = 0;
		}
		if (keystatus[0x4e] > 0)  // +
		{
			if (circlepoints < 63)
				circlepoints++;
			keystatus[0x4e] = 0;
		}

		bad = (keystatus[0x39] > 0);  //Gotta do this to save lots of 3 spaces!

		if (circlewall >= 0)
		{
			x1 = wall[circlewall].pt.x;
			y1 = wall[circlewall].pt.y;
			x2 = wall[wall[circlewall].point2].pt.x;
			y2 = wall[wall[circlewall].point2].pt.y;
			x3 = mousxplc;
			y3 = mousyplc;
			adjustmark(&x3,&y3,newnumwalls);
			templong1 = dmulscalen<4>(x3-x2,x1-x3,y1-y3,y3-y2);
			templong2 = dmulscalen<4>(y1-y2,x1-x3,y1-y3,x2-x1);
			if (templong2 != 0)
			{
				const int centerx = (((x1+x2) + scale(y1-y2,templong1,templong2))>>1);
				const int centery = (((y1+y2) + scale(x2-x1,templong1,templong2))>>1);

				dax = mulscalen<14>(centerx-posx,zoom);
				day = mulscalen<14>(centery-posy,zoom);
				drawline16(halfxdim16+dax-2,midydim16+day-2,halfxdim16+dax+2,midydim16+day+2,14);
				drawline16(halfxdim16+dax-2,midydim16+day+2,halfxdim16+dax+2,midydim16+day-2,14);

				circleang1 = getangle(x1-centerx,y1-centery);
				circleang2 = getangle(x2-centerx,y2-centery);

				k = ((circleang2-circleang1)&2047);
				if (mulscalen<4>(x3-x1,y2-y1) < mulscalen<4>(x2-x1,y3-y1))
				{
					k = -((circleang1-circleang2)&2047);
				}

				circlerad = static_cast<int>(std::sqrt(dmulscalen<4>(centerx-x1,centerx-x1,centery-y1,centery-y1))) << 2;

				for(i=circlepoints;i>0;i--)
				{
					j = ((circleang1 + scale(i,k,circlepoints+1))&2047);
					dax = centerx+mulscalen<14>(sintable[(j+512)&2047],circlerad);
					day = centery+mulscalen<14>(sintable[j],circlerad);

					if (dax <= -editorgridextent) dax = -editorgridextent;
					if (dax >= editorgridextent) dax = editorgridextent;
					if (day <= -editorgridextent) day = -editorgridextent;
					if (day >= editorgridextent) day = editorgridextent;

					if (bad > 0)
					{
						m = 0;
						if (wall[circlewall].nextwall >= 0)
							if (wall[circlewall].nextwall < circlewall) m = 1;
						insertpoint(circlewall,dax,day);
						circlewall += m;
					}
					dax = mulscalen<14>(dax-posx,zoom);
					day = mulscalen<14>(day-posy,zoom);
					drawline16(halfxdim16+dax-2,midydim16+day-2,halfxdim16+dax+2,midydim16+day-2,14);
					drawline16(halfxdim16+dax+2,midydim16+day-2,halfxdim16+dax+2,midydim16+day+2,14);
					drawline16(halfxdim16+dax+2,midydim16+day+2,halfxdim16+dax-2,midydim16+day+2,14);
					drawline16(halfxdim16+dax-2,midydim16+day+2,halfxdim16+dax-2,midydim16+day-2,14);
				}
				if (bad > 0)
				{
					bad = 0;
					keystatus[0x39] = 0;
					asksave = true;
					printmessage16("Circle points inserted.");
					circlewall = -1;
				}
			}
		}

		if (bad > 0)   //Space bar test
		{
			keystatus[0x39] = 0;
			adjustmark(&mousxplc,&mousyplc,newnumwalls);
			if (checkautoinsert(mousxplc,mousyplc,newnumwalls))
			{
				printmessage16("You must insert a point there first.");
				bad = 0;
			}
		}

		if (bad > 0)  //Space
		{
			if ((newnumwalls < numwalls) && (numwalls < MAXWALLS-1))
			{
				firstx = mousxplc, firsty = mousyplc;  //Make first point
				newnumwalls = numwalls;
				suckwall = -1;
				split = 0;

				//clearbufbyte(&wall[newnumwalls],sizeof(walltype),0L);
				std::memset(&wall[newnumwalls],0,sizeof(walltype));
				wall[newnumwalls].extra = -1;

				wall[newnumwalls].pt.x = mousxplc;
				wall[newnumwalls].pt.y = mousyplc;
				wall[newnumwalls].nextsector = -1;
				wall[newnumwalls].nextwall = -1;
				for(i=0;i<numwalls;i++)
					if ((wall[i].pt.x == mousxplc) && (wall[i].pt.y == mousyplc))
						suckwall = i;
				wall[newnumwalls].point2 = newnumwalls+1;
				printmessage16("Sector drawing started.");
				newnumwalls++;
			}
			else
			{  //if not back to first point
				if ((firstx != mousxplc) || (firsty != mousyplc))  //nextpoint
				{
					j = 0;
					for(i=numwalls;i<newnumwalls;i++)
						if ((mousxplc == wall[i].pt.x) && (mousyplc == wall[i].pt.y))
							j = 1;
					if (j == 0)
					{
							//check if starting to split a sector
						if (newnumwalls == numwalls+1)
						{
							dax = ((wall[numwalls].pt.x + mousxplc)>>1);
							day = ((wall[numwalls].pt.y + mousyplc)>>1);
							for(i=0;i<numsectors;i++)
								if (inside(dax,day,i) == 1)
								{    //check if first point at point of sector
									m = -1;
									startwall = g_sector[i].wallptr;
									endwall = startwall + g_sector[i].wallnum - 1;
									for(k=startwall;k<=endwall;k++)
										if (wall[k].pt.x == wall[numwalls].pt.x)
											if (wall[k].pt.y == wall[numwalls].pt.y)
											{
												m = k;
												break;
											}
									if (m >= 0)
										if ((wall[wall[k].point2].pt.x != mousxplc) || (wall[wall[k].point2].pt.y != mousyplc))
											if ((wall[lastwall((short)k)].pt.x != mousxplc) || (wall[lastwall((short)k)].pt.y != mousyplc))
											{
												split = 1;
												splitsect = i;
												splitstartwall = m;
												break;
											}
								}
						}

							//make new point

						//make sure not drawing over old red line
						bad = 0;
						for(i=0;i<numwalls;i++)
						{
							if (wall[i].nextwall >= 0)
							{
								if ((wall[i].pt.x == mousxplc) && (wall[i].pt.y == mousyplc))
									if ((wall[wall[i].point2].pt.x == wall[newnumwalls-1].pt.x) && (wall[wall[i].point2].pt.y == wall[newnumwalls-1].pt.y))
										bad = 1;
								if ((wall[i].pt.x == wall[newnumwalls-1].pt.x) && (wall[i].pt.y == wall[newnumwalls-1].pt.y))
									if ((wall[wall[i].point2].pt.x == mousxplc) && (wall[wall[i].point2].pt.y == mousyplc))
										bad = 1;
							}
						}

						if (bad == 0)
						{
							//clearbufbyte(&wall[newnumwalls],sizeof(walltype),0L);
							std::memset(&wall[newnumwalls], 0, sizeof(walltype));
							wall[newnumwalls].extra = -1;

							wall[newnumwalls].pt.x = mousxplc;
							wall[newnumwalls].pt.y = mousyplc;
							wall[newnumwalls].nextsector = -1;
							wall[newnumwalls].nextwall = -1;
							for(i=0;i<numwalls;i++)
								if ((wall[i].pt.x == mousxplc) && (wall[i].pt.y == mousyplc))
									suckwall = i;
							wall[newnumwalls].point2 = newnumwalls+1;
							newnumwalls++;
						}
						else
						{
							printmessage16("You can't draw new lines over red lines.");
						}
					}
				}

					//if not split and back to first point
				if ((split == 0) && (firstx == mousxplc) && (firsty == mousyplc) && (newnumwalls >= numwalls+3))
				{
					wall[newnumwalls-1].point2 = numwalls;

					if (suckwall == -1)  //if no connections to other sectors
					{
						k = -1;
						for(i=0;i<numsectors;i++)
							if (inside(firstx,firsty,i) == 1)
								k = i;
						if (k == -1)   //if not inside another sector either
						{              //add island sector
							if (clockdir(numwalls) == ClockDir_t::CCW)
								flipwalls(numwalls,newnumwalls);

							//clearbufbyte(&g_sector[numsectors],sizeof(sectortype),0L);
							std::memset(&g_sector[numsectors], 0, sizeof(sectortype));
							g_sector[numsectors].extra = -1;

							g_sector[numsectors].wallptr = numwalls;
							g_sector[numsectors].wallnum = newnumwalls-numwalls;
							g_sector[numsectors].ceilingz = -(32<<8);
							g_sector[numsectors].floorz = (32<<8);
							for(i=numwalls;i<newnumwalls;i++)
							{
								wall[i].cstat = 0;
								wall[i].shade = 0;
								wall[i].yrepeat = 8;
								fixrepeats((short)i);
								wall[i].picnum = 0;
								wall[i].overpicnum = 0;
								wall[i].nextsector = -1;
								wall[i].nextwall = -1;
							}
							headspritesect[numsectors] = -1;
							numsectors++;
						}
						else       //else add loop to sector
						{
							if (clockdir(numwalls) == ClockDir_t::CW)
								flipwalls(numwalls,newnumwalls);

							j = newnumwalls-numwalls;

							g_sector[k].wallnum += j;
							for(i=k+1;i<numsectors;i++)
								g_sector[i].wallptr += j;
							suckwall = g_sector[k].wallptr;

							for(i=0;i<numwalls;i++)
							{
								if (wall[i].nextwall >= suckwall)
									wall[i].nextwall += j;
								if (wall[i].point2 >= suckwall)
									wall[i].point2 += j;
							}

							for(i=newnumwalls-1;i>=suckwall;i--)
								std::memcpy(&wall[i+j],&wall[i],sizeof(walltype));
							for(i=0;i<j;i++)
								std::memcpy(&wall[i+suckwall],&wall[i+newnumwalls],sizeof(walltype));

							for(i=suckwall;i<suckwall+j;i++)
							{
								wall[i].point2 += (suckwall-numwalls);

								wall[i].cstat = wall[suckwall+j].cstat;
								wall[i].shade = wall[suckwall+j].shade;
								wall[i].yrepeat = wall[suckwall+j].yrepeat;
								fixrepeats((short)i);
								wall[i].picnum = wall[suckwall+j].picnum;
								wall[i].overpicnum = wall[suckwall+j].overpicnum;

								wall[i].nextsector = -1;
								wall[i].nextwall = -1;
							}
						}
					}
					else
					{
						  //add new sector with connections
						if (clockdir(numwalls) == ClockDir_t::CCW)
							flipwalls(numwalls,newnumwalls);

						//clearbufbyte(&g_sector[numsectors],sizeof(sectortype),0L);
						std::memset(&g_sector[numsectors],0,sizeof(sectortype));
						g_sector[numsectors].extra = -1;

						g_sector[numsectors].wallptr = numwalls;
						g_sector[numsectors].wallnum = newnumwalls-numwalls;
						sucksect = sectorofwall(suckwall);
						g_sector[numsectors].ceilingstat = g_sector[sucksect].ceilingstat;
						g_sector[numsectors].floorstat = g_sector[sucksect].floorstat;
						g_sector[numsectors].ceilingxpanning = g_sector[sucksect].ceilingxpanning;
						g_sector[numsectors].floorxpanning = g_sector[sucksect].floorxpanning;
						g_sector[numsectors].ceilingshade = g_sector[sucksect].ceilingshade;
						g_sector[numsectors].floorshade = g_sector[sucksect].floorshade;
						g_sector[numsectors].ceilingz = g_sector[sucksect].ceilingz;
						g_sector[numsectors].floorz = g_sector[sucksect].floorz;
						g_sector[numsectors].ceilingpicnum = g_sector[sucksect].ceilingpicnum;
						g_sector[numsectors].floorpicnum = g_sector[sucksect].floorpicnum;
						g_sector[numsectors].ceilingheinum = g_sector[sucksect].ceilingheinum;
						g_sector[numsectors].floorheinum = g_sector[sucksect].floorheinum;
						for(i=numwalls;i<newnumwalls;i++)
						{
							wall[i].cstat = wall[suckwall].cstat;
							wall[i].shade = wall[suckwall].shade;
							wall[i].yrepeat = wall[suckwall].yrepeat;
							fixrepeats((short)i);
							wall[i].picnum = wall[suckwall].picnum;
							wall[i].overpicnum = wall[suckwall].overpicnum;
							checksectorpointer((short)i,(short)numsectors);
						}
						headspritesect[numsectors] = -1;
						numsectors++;
					}
					numwalls = newnumwalls;
					newnumwalls = -1;
					asksave = true;
				}
				if (split == 1)
				{
						 //split sector
					startwall = g_sector[splitsect].wallptr;
					endwall = startwall + g_sector[splitsect].wallnum - 1;
					for(k=startwall;k<=endwall;k++)
						if (wall[k].pt.x == wall[newnumwalls-1].pt.x)
							if (wall[k].pt.y == wall[newnumwalls-1].pt.y)
							{
								bad = 0;
								if (loopnumofsector(splitsect,splitstartwall) != loopnumofsector(splitsect,(short)k))
									bad = 1;

								if (bad == 0)
								{
									//SPLIT IT!
									//Split splitsect given: startwall,
									//   new points: numwalls to newnumwalls-2

									splitendwall = k;
									newnumwalls--;  //first fix up the new walls
									for(i=numwalls;i<newnumwalls;i++)
									{
										wall[i].cstat = wall[startwall].cstat;
										wall[i].shade = wall[startwall].shade;
										wall[i].yrepeat = wall[startwall].yrepeat;
										fixrepeats((short)i);
										wall[i].picnum = wall[startwall].picnum;
										wall[i].overpicnum = wall[startwall].overpicnum;

										wall[i].nextwall = -1;
										wall[i].nextsector = -1;
										wall[i].point2 = i+1;
									}

									danumwalls = newnumwalls;  //where to add more walls
									m = splitendwall;          //copy rest of loop next
									while (m != splitstartwall)
									{
										std::memcpy(&wall[danumwalls],&wall[m],sizeof(walltype));
										wall[danumwalls].point2 = danumwalls+1;
										danumwalls++;
										m = wall[m].point2;
									}
									wall[danumwalls-1].point2 = numwalls;

										//Add other loops for 1st sector
									loopnum = loopnumofsector(splitsect,splitstartwall);
									i = loopnum;
									for(j=startwall;j<=endwall;j++)
									{
										k = loopnumofsector(splitsect,(short)j);
										if ((k != i) && (k != loopnum))
										{
											i = k;
											if (loopinside(wall[j].pt.x,wall[j].pt.y,numwalls) == 1)
											{
												m = j;          //copy loop
												k = danumwalls;
												do
												{
													std::memcpy(&wall[danumwalls],&wall[m],sizeof(walltype));
													wall[danumwalls].point2 = danumwalls+1;
													danumwalls++;
													m = wall[m].point2;
												}
												while (m != j);
												wall[danumwalls-1].point2 = k;
											}
										}
									}

									secondstartwall = danumwalls;
										//copy split points for other sector backwards
									for(j=newnumwalls;j>numwalls;j--)
									{
										std::memcpy(&wall[danumwalls],&wall[j],sizeof(walltype));
										wall[danumwalls].nextwall = -1;
										wall[danumwalls].nextsector = -1;
										wall[danumwalls].point2 = danumwalls+1;
										danumwalls++;
									}
									m = splitstartwall;     //copy rest of loop next
									while (m != splitendwall)
									{
										std::memcpy(&wall[danumwalls],&wall[m],sizeof(walltype));
										wall[danumwalls].point2 = danumwalls+1;
										danumwalls++;
										m = wall[m].point2;
									}
									wall[danumwalls-1].point2 = secondstartwall;

										//Add other loops for 2nd sector
									loopnum = loopnumofsector(splitsect,splitstartwall);
									i = loopnum;
									for(j=startwall;j<=endwall;j++)
									{
										k = loopnumofsector(splitsect,(short)j);
										if ((k != i) && (k != loopnum))
										{
											i = k;
											if (loopinside(wall[j].pt.x,wall[j].pt.y,secondstartwall) == 1)
											{
												m = j;          //copy loop
												k = danumwalls;
												do
												{
													std::memcpy(&wall[danumwalls],&wall[m],sizeof(walltype));
													wall[danumwalls].point2 = danumwalls+1;
													danumwalls++;
													m = wall[m].point2;
												}
												while (m != j);
												wall[danumwalls-1].point2 = k;
											}
										}
									}

										//fix all next pointers on old sector line
									for(j=numwalls;j<danumwalls;j++)
									{
										if (wall[j].nextwall >= 0)
										{
											wall[wall[j].nextwall].nextwall = j;
											if (j < secondstartwall)
												wall[wall[j].nextwall].nextsector = numsectors;
											else
												wall[wall[j].nextwall].nextsector = numsectors+1;
										}
									}
										//set all next pointers on split
									for(j=numwalls;j<newnumwalls;j++)
									{
										m = secondstartwall+(newnumwalls-1-j);
										wall[j].nextwall = m;
										wall[j].nextsector = numsectors+1;
										wall[m].nextwall = j;
										wall[m].nextsector = numsectors;
									}
										//copy sector attributes & fix wall pointers
									std::memcpy(&g_sector[numsectors],&g_sector[splitsect],sizeof(sectortype));
									std::memcpy(&g_sector[numsectors+1],&g_sector[splitsect],sizeof(sectortype));
									g_sector[numsectors].wallptr = numwalls;
									g_sector[numsectors].wallnum = secondstartwall-numwalls;
									g_sector[numsectors+1].wallptr = secondstartwall;
									g_sector[numsectors+1].wallnum = danumwalls-secondstartwall;

										//fix sprites
									j = headspritesect[splitsect];
									while (j != -1)
									{
										k = nextspritesect[j];
										if (loopinside(sprite[j].x,sprite[j].y,numwalls) == 1)
											changespritesect(j,numsectors);
										//else if (loopinside(sprite[j].x,sprite[j].y,secondstartwall) == 1)
										else  //Make sure no sprites get left out & deleted!
											changespritesect(j,numsectors+1);
										j = k;
									}

									numsectors+=2;

										//Back of number of walls of new sector for later
									k = danumwalls-numwalls;

										//clear out old sector's next pointers for clean deletesector
									numwalls = danumwalls;
									for(j=startwall;j<=endwall;j++)
									{
										wall[j].nextwall = -1;
										wall[j].nextsector = -1;
									}
									deletesector(splitsect);

										//Check pointers
									for(j=numwalls-k;j<numwalls;j++)
									{
										if (wall[j].nextwall >= 0)
											checksectorpointer(wall[j].nextwall,wall[j].nextsector);
										checksectorpointer((short)j,sectorofwall((short)j));
									}

										//k now safe to use as temp

									for(m=numsectors-2;m<numsectors;m++)
									{
										j = headspritesect[m];
										while (j != -1)
										{
											k = nextspritesect[j];
											setsprite(j,sprite[j].x,sprite[j].y,sprite[j].z);
											j = k;
										}
									}

									newnumwalls = -1;
									printmessage16("Sector split.");
									break;
								}
								else
								{
										//Sector split - actually loop joining

									splitendwall = k;
									newnumwalls--;  //first fix up the new walls
									for(i=numwalls;i<newnumwalls;i++)
									{
										wall[i].cstat = wall[startwall].cstat;
										wall[i].shade = wall[startwall].shade;
										wall[i].yrepeat = wall[startwall].yrepeat;
										fixrepeats((short)i);
										wall[i].picnum = wall[startwall].picnum;
										wall[i].overpicnum = wall[startwall].overpicnum;

										wall[i].nextwall = -1;
										wall[i].nextsector = -1;
										wall[i].point2 = i+1;
									}

									danumwalls = newnumwalls;  //where to add more walls
									m = splitendwall;          //copy rest of loop next
									do
									{
										std::memcpy(&wall[danumwalls],&wall[m],sizeof(walltype));
										wall[danumwalls].point2 = danumwalls+1;
										danumwalls++;
										m = wall[m].point2;
									} while (m != splitendwall);

									//copy split points for other sector backwards
									for(j=newnumwalls;j>numwalls;j--)
									{
										std::memcpy(&wall[danumwalls],&wall[j],sizeof(walltype));
										wall[danumwalls].nextwall = -1;
										wall[danumwalls].nextsector = -1;
										wall[danumwalls].point2 = danumwalls+1;
										danumwalls++;
									}

									m = splitstartwall;     //copy rest of loop next
									do
									{
										std::memcpy(&wall[danumwalls],&wall[m],sizeof(walltype));
										wall[danumwalls].point2 = danumwalls+1;
										danumwalls++;
										m = wall[m].point2;
									} while (m != splitstartwall);
									wall[danumwalls-1].point2 = numwalls;

										//Add other loops to sector
									loopnum = loopnumofsector(splitsect,splitstartwall);
									i = loopnum;
									for(j=startwall;j<=endwall;j++)
									{
										k = loopnumofsector(splitsect,(short)j);
										if ((k != i) && (k != loopnumofsector(splitsect,splitstartwall)) && (k != loopnumofsector(splitsect,splitendwall)))
										{
											i = k;
											m = j; k = danumwalls;     //copy loop
											do
											{
												std::memcpy(&wall[danumwalls],&wall[m],sizeof(walltype));
												wall[danumwalls].point2 = danumwalls+1;
												danumwalls++;
												m = wall[m].point2;
											} while (m != j);
											wall[danumwalls-1].point2 = k;
										}
									}

										//fix all next pointers on old sector line
									for(j=numwalls;j<danumwalls;j++)
									{
										if (wall[j].nextwall >= 0)
										{
											wall[wall[j].nextwall].nextwall = j;
											wall[wall[j].nextwall].nextsector = numsectors;
										}
									}

										//copy sector attributes & fix wall pointers
									std::memcpy(&g_sector[numsectors], &g_sector[splitsect], sizeof(sectortype));
									g_sector[numsectors].wallptr = numwalls;
									g_sector[numsectors].wallnum = danumwalls-numwalls;

										//fix sprites
									j = headspritesect[splitsect];
									while (j != -1)
									{
										k = nextspritesect[j];
										changespritesect(j,numsectors);
										j = k;
									}

									numsectors++;

										//Back of number of walls of new sector for later
									k = danumwalls-numwalls;

										//clear out old sector's next pointers for clean deletesector
									numwalls = danumwalls;
									for(j=startwall;j<=endwall;j++)
									{
										wall[j].nextwall = -1;
										wall[j].nextsector = -1;
									}
									deletesector(splitsect);

										//Check pointers
									for(j=numwalls-k;j<numwalls;j++)
									{
										if (wall[j].nextwall >= 0)
											checksectorpointer(wall[j].nextwall,wall[j].nextsector);
										checksectorpointer((short)j,numsectors-1);
									}

									newnumwalls = -1;
									printmessage16("Loops joined.");
									break;
								}
							}
				}
			}
		}

		if (keystatus[0x1c] > 0) //Left Enter
		{
			keystatus[0x1c] = 0;
			if (keystatus[0x2a]&keystatus[0x1d])
			{
				printmessage16("CHECKING ALL POINTERS!");
				for(i=0;i<numsectors;i++)
				{
					startwall = g_sector[i].wallptr;
					for(j=startwall;j<numwalls;j++)
						if (wall[j].point2 < startwall) startwall = wall[j].point2;
					g_sector[i].wallptr = startwall;
				}
				for(i=numsectors-2;i>=0;i--)
					g_sector[i].wallnum = g_sector[i+1].wallptr-g_sector[i].wallptr;
				g_sector[numsectors-1].wallnum = numwalls-g_sector[numsectors-1].wallptr;

				for(i=0;i<numwalls;i++)
				{
					wall[i].nextsector = -1;
					wall[i].nextwall = -1;
				}
				for(i=0;i<numsectors;i++)
				{
					startwall = g_sector[i].wallptr;
					endwall = startwall + g_sector[i].wallnum;
					for(j=startwall;j<endwall;j++)
						checksectorpointer((short)j,(short)i);
				}
				printmessage16("ALL POINTERS CHECKED!");
				asksave = true;
			}
			else
			{
				if (linehighlight >= 0)
				{
					checksectorpointer(linehighlight,sectorofwall(linehighlight));
					printmessage16("Highlighted line pointers checked.");
					asksave = true;
				}
			}
		}

		if ((keystatus[0x0e] > 0) && (newnumwalls >= numwalls)) //Backspace
		{
			if (newnumwalls > numwalls)
			{
				newnumwalls--;
				asksave = true;
				keystatus[0x0e] = 0;
			}
			if (newnumwalls == numwalls)
			{
				newnumwalls = -1;
				asksave = true;
				keystatus[0x0e] = 0;
			}
		}

		if ((keystatus[0xd3] > 0) && (keystatus[0x9d] > 0) && (numwalls >= 0))
		{                                                      //sector delete
			keystatus[0xd3] = 0;

			sucksect = -1;
			for(i=0;i<numsectors;i++)
				if (inside(mousxplc,mousyplc,i) == 1)
				{
					k = 0;
					if (highlightsectorcnt >= 0)
						for(j=0;j<highlightsectorcnt;j++)
							if (highlightsector[j] == i)
							{
								for(j=highlightsectorcnt-1;j>=0;j--)
								{
									deletesector(highlightsector[j]);
									for(k=j-1;k>=0;k--)
										if (highlightsector[k] >= highlightsector[j])
											highlightsector[k]--;
								}
								printmessage16("Highlighted sectors deleted.");
								newnumwalls = -1;
								k = 1;
								highlightsectorcnt = -1;
								break;
							}
					if (k == 0)
					{
						deletesector((short)i);
						highlightsectorcnt = -1;
						printmessage16("Sector deleted.");
					}
					newnumwalls = -1;
					asksave = true;
					break;
				}
		}

		if ((keystatus[0xd3] > 0) && (pointhighlight >= 0))
		{
			if ((pointhighlight&0xc000) == 16384)   //Sprite Delete
			{
				deletesprite(pointhighlight&16383);
				printmessage16("Sprite deleted.");
				updatenumsprites();
				asksave = true;
			}
			keystatus[0xd3] = 0;
		}

		if (keystatus[0xd2] > 0)  //InsertPoint
		{
			if (highlightsectorcnt >= 0)
			{
				newnumsectors = numsectors;
				newnumwalls = numwalls;
				for(i=0;i<highlightsectorcnt;i++)
				{
					copysector(highlightsector[i], newnumsectors, newnumwalls, true);
					newnumsectors++;
					newnumwalls += g_sector[highlightsector[i]].wallnum;
				}

				for(i=0;i<highlightsectorcnt;i++)
				{
					startwall = g_sector[highlightsector[i]].wallptr;
					endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
					for(j=startwall;j<=endwall;j++)
					{
						if (wall[j].nextwall >= 0)
							checksectorpointer(wall[j].nextwall,wall[j].nextsector);
						checksectorpointer((short)j,highlightsector[i]);
					}
					highlightsector[i] = numsectors+i;
				}
				numsectors = newnumsectors;
				numwalls = newnumwalls;

				newnumwalls = -1;
				newnumsectors = -1;

				updatenumsprites();
				printmessage16("Sectors duplicated and stamped.");
				asksave = true;
			}
			else if (highlightcnt >= 0)
			{
				for(i=0;i<highlightcnt;i++)
					if ((highlight[i]&0xc000) == 16384)
					{
							//duplicate sprite
						k = (highlight[i]&16383);
						j = insertsprite(sprite[k].sectnum,sprite[k].statnum);
						std::memcpy(&sprite[j],&sprite[k],sizeof(spritetype));
						sprite[j].sectnum = sprite[k].sectnum;   //Don't let memcpy overwrite sector!
						setsprite(j,sprite[j].x,sprite[j].y,sprite[j].z);
					}
				updatenumsprites();
				printmessage16("Sprites duplicated and stamped.");
				asksave = true;
			}
			else if (linehighlight >= 0)
			{
				getclosestpointonwall(mousxplc,mousyplc,(int)linehighlight,&dax,&day);
				adjustmark(&dax,&day,newnumwalls);
				insertpoint(linehighlight,dax,day);
				printmessage16("Point inserted.");

				j = 0;
					//Check to see if point was inserted over another point
				for(i=numwalls-1;i>=0;i--)     //delete points
					if (wall[i].pt.x == wall[wall[i].point2].pt.x)
						if (wall[i].pt.y == wall[wall[i].point2].pt.y)
						{
							deletepoint((short)i);
							j++;
						}
				for(i=0;i<numwalls;i++)        //make new red lines?
				{
					if ((wall[i].pt.x == dax) && (wall[i].pt.y == day))
					{
						checksectorpointer((short)i,sectorofwall((short)i));
						fixrepeats((short)i);
					}
					else if ((wall[wall[i].point2].pt.x == dax) && (wall[wall[i].point2].pt.y == day))
					{
						checksectorpointer((short)i,sectorofwall((short)i));
						fixrepeats((short)i);
					}
				}
				//if (j != 0)
				//{
				//   dax = ((wall[linehighlight].pt.x + wall[wall[linehighlight].point2].pt.x)>>1);
				//   day = ((wall[linehighlight].pt.y + wall[wall[linehighlight].point2].pt.y)>>1);
				//   if ((dax != wall[linehighlight].pt.x) || (day != wall[linehighlight].pt.y))
				//      if ((dax != wall[wall[linehighlight].point2].pt.x) || (day != wall[wall[linehighlight].point2].pt.y))
				//      {
				//         insertpoint(linehighlight,dax,day);
				//         printmessage16("Point inserted at midpoint.");
				//      }
				//}

				asksave = true;
			}
			keystatus[0xd2] = 0;
		}

		ExtCheckKeys();

		/*j = 0;
		for(i=22-1;i>=0;i--) updatecrc16(j,kensig[i]);
		if ((j&0xffff) != 0xebf)
		{
			std::printf("Don't screw with my name.\n");
			std::exit(0);
		}*/
		//printext16(9L,336+9L,4,-1,kensig,0);
		//printext16(8L,336+8L,12,-1,kensig,0);

		showframe();
		synctics = totalclock-lockclock;
		lockclock += synctics;

		if (keystatus[buildkeys[14]] > 0)
		{
			updatesector(posx,posy,&cursectnum);
			if (cursectnum >= 0)
				keystatus[buildkeys[14]] = 2;
			else
				printmessage16("Arrow must be inside a sector before entering 3D mode.");
		}
		if (keystatus[1] > 0)
		{
			keystatus[1] = 0;
			printmessage16("(N)ew, (L)oad/from (G)RP, (S)ave, save (A)s, (Q)uit");
			showframe();
			bflushchars();
			bad = 1;
			while (bad == 1)
			{
				if (handleevents()) {
					if (quitevent) {
						quitevent = false;
					}
				}

				ch = bgetchar();

				if (keystatus[1] > 0)
				{
					keystatus[1] = 0;
					bad = 0;
					printmessage16("");
				}
				else if (ch == 'n' || ch == 'N')  //N
				{
					bad = 0;
					printmessage16("Are you sure you want to start a new board? (Y/N)");
					showframe();
					bflushchars(); ch = 0;
					while (keystatus[1] == 0)
					{
						if (handleevents()) {
							if (quitevent) {
								quitevent = false;
							}
						}

						ch = bgetchar();

						if (ch == 'Y' || ch == 'y')
						{
							highlightsectorcnt = -1;
							highlightcnt = -1;

							std::ranges::fill(show2dwall, 0); //Clear all highlights
							std::ranges::fill(show2dsprite, 0);

							for(auto& aSector : g_sector) {
								aSector.extra = -1;
							}

							for(auto& aWall : wall) {
								aWall.extra = -1;
							}

							for(auto& aSprite : sprite) {
								aSprite.extra = -1;
							}

							sectorhighlightstat = -1;
							newnumwalls = -1;
							joinsector[0] = -1;
							circlewall = -1;
							circlepoints = 7;

							posx = 32768;          //new board!
							posy = 32768;
							posz = 0;
							ang = 1536;
							numsectors = 0;
							numwalls = 0;
							cursectnum = -1;
							initspritelists();
							boardfilename = "newboard.map";
							mapversion = 7;

							wm_setwindowtitle("(new board)");
							break;
						} else if (ch == 'N' || ch == 'n' || ch == 13 || ch == ' ') {
							break;
						}
					}
					printmessage16("");
					showframe();
				}
				else if (ch == 'l' || ch == 'L' || ch == 'g' || ch == 'G')  //L and G
				{
					bad = 0;
					printmessage16("Load board...");
					showframe();

					selectedboardfilename = boardfilename;
					if (ch == 'g' || ch == 'G') {
						i = menuselect(PATHSEARCH_GAME);
					} else {
						std::string filename;
						std::string initialdir;
						int filer;

						std::string initialfile = findfilename(&selectedboardfilename[0]);
						if (pathsearchmode == PATHSEARCH_GAME || initialfile == selectedboardfilename) {
							initialdir = "";
						} else {
							initialdir = selectedboardfilename;
						}
						while (1) {
							filer = wm_filechooser(initialdir, initialfile.c_str(), "map", 1, filename);
							if (filer >= 0) {
								if (!filename.empty() && filename.length() + 1 > sizeof(selectedboardfilename)) {
									printmessage16("File path is too long.");
									showframe();
									continue;
								}

								if (filer == 0 || filename.empty()) {
									i = -1;
								} else {
									selectedboardfilename = filename;
									i = 0;
									pathsearchmode = PATHSEARCH_SYSTEM;
								}
							} else {
								// Fallback behaviour.
								selectedboardfilename = boardfilename;
								i = menuselect(pathsearchmode);
							}
							break;
						}
					}

					if (i < 0)
					{
						if (i == -2)
							printmessage16("No .MAP files found.");
					}
					else
					{
						boardfilename = selectedboardfilename;

						if (highlightsectorcnt >= 0)
						{
							j = 0; k = 0;
							for(i=0;i<highlightsectorcnt;i++)
							{
								j += g_sector[highlightsector[i]].wallnum;

								m = headspritesect[highlightsector[i]];
								while (m != -1)
								{
									k++;
									m = nextspritesect[m];
								}
							}

							updatenumsprites();
							if ((numsectors+highlightsectorcnt > MAXSECTORS) || (numwalls+j > MAXWALLS) || (numsprites+k > MAXSPRITES))
							{
								highlightsectorcnt = -1;
							}
							else
							{
									//Put sectors&walls to end of lists
								j = MAXWALLS;
								for(i=0;i<highlightsectorcnt;i++)
								{
									j -= g_sector[highlightsector[i]].wallnum;
									copysector(highlightsector[i], (short)(MAXSECTORS - highlightsectorcnt + i), (short)j, false);
								}

									//Put sprites to end of list
									//DONT USE m BETWEEN HERE AND SPRITE RE-ATTACHING!
								m = MAXSPRITES;
								for(i=MAXSPRITES-1;i>=0;i--)
									if (sprite[i].statnum < MAXSTATUS)
									{
										k = sprite[i].sectnum;
										for(j=0;j<highlightsectorcnt;j++)
											if (highlightsector[j] == k)
											{
												m--;
												if (i != m)
													std::memcpy(&sprite[m],&sprite[i],sizeof(spritetype));

													//HACK - THESE 2 buffers back up .sectnum and .statnum
													//for initspritelists() inside the loadboard call
												//tsprite[m].picnum = MAXSECTORS-highlightsectorcnt+j;
												//tsprite[m].owner = sprite[i].statnum;

												// JBF: I see your hack and raise you another
												spriteext[m].mdanimcur = MAXSECTORS-highlightsectorcnt+j;
												spriteext[m].angoff = sprite[i].statnum;

												break;
											}
									}
							}
						}

						highlightcnt = -1;
						sectorhighlightstat = -1;
						newnumwalls = -1;
						joinsector[0] = -1;
						circlewall = -1;
						circlepoints = 7;

						for(auto& aSector : g_sector) {
							aSector.extra = -1;
						}

						for(auto& aWall : wall) {
							aWall.extra = -1;
						}

						for(auto& aSprite : sprite) {
							aSprite.extra = -1;
						}

						ExtPreLoadMap();
						j = pathsearchmode == PATHSEARCH_GAME && grponlymode ? KOPEN4LOAD_ANYGRP : KOPEN4LOAD_ANY;
						i = loadboard(boardfilename, j, &posx, &posy, &posz, &ang, &cursectnum);
						if (i == -2) i = loadoldboard(boardfilename, j, &posx, &posy, &posz, &ang, &cursectnum);
						if (i < 0)
						{
							printmessage16("Invalid map format.");
						}
						else
						{
							ExtLoadMap(boardfilename);

							if (highlightsectorcnt >= 0)
							{
								if ((numsectors+highlightsectorcnt > MAXSECTORS) || (g_sector[MAXSECTORS-highlightsectorcnt].wallptr < numwalls))
								{
									highlightsectorcnt = -1;
								}
								else
								{
										//Re-attach sectors&walls
									for(i=0;i<highlightsectorcnt;i++)
									{
										copysector((short)(MAXSECTORS - highlightsectorcnt + i), numsectors, numwalls, false);
										highlightsector[i] = numsectors;
										numwalls += g_sector[numsectors].wallnum;
										numsectors++;
									}
										//Re-attach sprites
									while (m < MAXSPRITES)
									{
										//HACK - THESE 2 buffers back up .sectnum and .statnum
										//for initspritelists() inside the loadboard call
										//tsprite[m].picnum = sprite[i].sectnum;
										//tsprite[m].owner = sprite[i].statnum;

										j = insertsprite(tsprite[m].picnum+(numsectors-MAXSECTORS),tsprite[m].owner);
										std::memcpy(&sprite[j],&sprite[m],sizeof(spritetype));
										//sprite[j].sectnum = tsprite[m].picnum+(numsectors-MAXSECTORS);
										//sprite[j].statnum = tsprite[m].owner;

										// JBF: I see your hack and raise you another
										sprite[j].sectnum = spriteext[m].mdanimcur+(numsectors-MAXSECTORS);
										sprite[j].statnum = spriteext[m].angoff;
										spriteext[m].mdanimcur = spriteext[m].angoff = 0;

										m++;
									}

									for(i=0;i<highlightsectorcnt;i++)
									{
										startwall = g_sector[highlightsector[i]].wallptr;
										endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
										for(j=startwall;j<=endwall;j++)
										{
											if (wall[j].nextwall >= 0)
												checksectorpointer(wall[j].nextwall,wall[j].nextsector);
											checksectorpointer((short)j,highlightsector[i]);
										}
									}

								}
							}

							if (mapversion < 7) {
								char buf[82];
								fmt::format_to(buf, "Old map (v{}) loaded successfully.", mapversion);
								printmessage16(buf);
							} else {
								printmessage16("Map loaded successfully.");
							}
						}
						updatenumsprites();
						startposx = posx;      //this is same
						startposy = posy;
						startposz = posz;
						startang = ang;
						startsectnum = cursectnum;
					}
					showframe();
					keystatus[0x1c] = 0;
				}
				else if (ch == 'a' || ch == 'A')  //A
				{
					std::string filename;
					std::string initialdir;
					std::string initialfile;
					char *curs;
					int filer;

					bad = 0;
					printmessage16("Save board as...");
					showframe();

					selectedboardfilename = boardfilename;
					initialfile = findfilename(&selectedboardfilename[0]);
					if (pathsearchmode == PATHSEARCH_GAME || initialfile == &selectedboardfilename[0]) {
						initialdir = "";
					}
					else {
						initialdir = selectedboardfilename;
					}

					while (1) {
						filer = wm_filechooser(initialdir, initialfile.c_str(), "map", 0, filename);
						if (filer >= 0) {
							if (!filename.empty() && filename.length() + 1 > sizeof(selectedboardfilename)) {
								printmessage16("File path is too long.");
								showframe();
								continue;
							}

							if (filer == 0 || filename.empty()) {
								bad = 1;	// Cancel.
							} else {
								selectedboardfilename = filename;
								bad = 2;	// OK.
							}
						}
						else {
							// Fallback behaviour.
							if (pathsearchmode == PATHSEARCH_SYSTEM) {
								selectedboardfilename = boardfilename;
							} else {
								filename = findfilename(boardfilename);
								selectedboardfilename = filename;
								Bcanonicalisefilename(&selectedboardfilename[0], 0);
							}
							bad = 0;
						}
						break;
					}

					// Find where the filename starts on the path.
					filename = findfilename(&selectedboardfilename[0]);

					// Find the end of the filename and cut off any .map extension.
					curs = std::strrchr(&filename[0], 0);
					if (curs - &filename[0] >= 4 && IsSameAsNoCase(curs - 4, ".map")) { curs -= 4; *curs = 0; }

					bflushchars();
					while (bad == 0)
					{
						fmt::format_to(buffer, "Save as: {}_", filename);
						printmessage16(buffer);
						showframe();

						if (handleevents()) {
							if (quitevent) quitevent = false;
						}

						ch = bgetchar();

						if (keystatus[1] > 0) bad = 1;
						else if (ch == 13) bad = 2;
						else if (ch > 0) {
							if (curs - &filename[0] > 0 && (ch == 8 || ch == 127)) {
								*(--curs) = 0;
							}
							else if (std::strlen(&selectedboardfilename[0]) < sizeof(selectedboardfilename)-5
								&& ch > 32 && ch < 128)
							{
								*(curs++) = ch;
								*curs = 0;
							}
						}
					}
					if (bad == 1)
					{
						keystatus[1] = 0;
						printmessage16("Operation cancelled");
						showframe();
					}
					if (bad == 2)
					{
						keystatus[0x1c] = 0;

						filename.append(".map");
						fmt::format_to(buffer, "Saving to {}...", filename);
						printmessage16(buffer);
						showframe();

						fixspritesectors();   //Do this before saving!
						updatesector(startposx,startposy,&startsectnum);
						ExtPreSaveMap();
						if (mapversion < 7) {
							bad = saveoldboard(&selectedboardfilename[0],
							                   &startposx, &startposy,
											   &startposz, &startang,
											   &startsectnum);
						} else {
							bad = saveboard(&selectedboardfilename[0],
							                &startposx, &startposy,
											&startposz, &startang,
											&startsectnum);
						}
						if (!bad) {
							ExtSaveMap(&selectedboardfilename[0]);
							printmessage16("Board saved.");
							boardfilename = selectedboardfilename;
							pathsearchmode = PATHSEARCH_SYSTEM;
							asksave = false;
						} else {
							printmessage16("Board NOT saved!");
						}
						showframe();
					}
					bad = 0;
				}
				else if (ch == 's' || ch == 'S')  //S
				{
					bad = 0;
					printmessage16("Saving board...");
					showframe();
					std::string filename = boardfilename;
					if (pathsearchmode == PATHSEARCH_GAME) {
						filename = findfilename(filename);
					}
					fixspritesectors();   //Do this before saving!
					updatesector(startposx,startposy,&startsectnum);
					ExtPreSaveMap();
					if (mapversion < 7) {
						bad = saveoldboard(filename.c_str(), &startposx, &startposy, &startposz, &startang, &startsectnum);
					} else {
						bad = saveboard(filename, &startposx, &startposy, &startposz, &startang, &startsectnum);
					}
					if (!bad) {
						ExtSaveMap(filename.c_str());
						printmessage16("Board saved.");
						asksave = false;
					} else {
						printmessage16("Board NOT saved!");
					}
					showframe();
				}
				else if (ch == 'q' || ch == 'Q')  //Q
				{
					bad = 0;
					printmessage16("Are you sure you want to quit?");
					showframe();
					bflushchars();
					while (keystatus[1] == 0)
					{
						if (handleevents()) {
							if (quitevent) quitevent = false;
						}

						ch = bgetchar();

						if (ch == 'y' || ch == 'Y')
						{
							//QUIT!
							if (asksave) {
								printmessage16("Save changes?");
								showframe();
								while (keystatus[1] == 0)
								{
									if (handleevents()) {
										if (quitevent) break;	// like saying no
									}

									ch = bgetchar();

									if (ch == 'y' || ch == 'Y')
									{
										std::string filename = boardfilename;
										if (pathsearchmode == PATHSEARCH_GAME) {
											filename = findfilename(filename.c_str());
										}
										fixspritesectors();   //Do this before saving!
										updatesector(startposx,startposy,&startsectnum);
										ExtPreSaveMap();
										if (mapversion < 7) {
											bad = saveoldboard(filename.c_str(), &startposx, &startposy, &startposz, &startang, &startsectnum);
										} else {
											bad = saveboard(filename, &startposx, &startposy, &startposz, &startang, &startsectnum);
										}
										if (!bad) {
											ExtSaveMap(filename.c_str());
										}
										break;
									} else if (ch == 'n' || ch == 'N' || ch == 13 || ch == ' ') {
										break;
									}
								}
							}
							clearfilenames();
							ExtUnInit();
							uninitengine();
							fmt::print("Memory status: {}({}) bytes\n", cachesize, artsize);
							fmt::print("{}\n", kensig);
							std::exit(0);
						} else if (ch == 'n' || ch == 'N' || ch == 13 || ch == ' ') {
							break;
						}
					}
					printmessage16("");
					showframe();
				}
			}
			clearkeys();
		}

		//nextpage();
	}

	for(i=0;i<highlightsectorcnt;i++)
	{
		startwall = g_sector[highlightsector[i]].wallptr;
		endwall = startwall+g_sector[highlightsector[i]].wallnum-1;
		for(j=startwall;j<=endwall;j++)
		{
			if (wall[j].nextwall >= 0)
				checksectorpointer(wall[j].nextwall,wall[j].nextsector);
			checksectorpointer((short)j,highlightsector[i]);
		}
	}

	fixspritesectors();

	if (setgamemode(fullscreen,xdimgame,ydimgame,bppgame) < 0)
	{
		ExtUnInit();
		uninitengine();
		clearfilenames();
		fmt::print(stderr, "{} * {} not supported in this graphics mode\n",xdim,ydim);
		std::exit(0);
	}

	posz = oposz;
	searchx = scale(searchx,xdimgame,xdim2d);
	searchy = scale(searchy,ydimgame,ydim2d-STATUS2DSIZ);
}

void getpoint(int searchxe, int searchye, int *x, int *y)
{
	if (posx <= -editorgridextent) posx = -editorgridextent;
	if (posx >= editorgridextent) posx = editorgridextent;
	if (posy <= -editorgridextent) posy = -editorgridextent;
	if (posy >= editorgridextent) posy = editorgridextent;

	*x = posx + divscalen<14>(searchxe-halfxdim16,zoom);
	*y = posy + divscalen<14>(searchye-midydim16,zoom);

	if (*x <= -editorgridextent) *x = -editorgridextent;
	if (*x >= editorgridextent) *x = editorgridextent;
	if (*y <= -editorgridextent) *y = -editorgridextent;
	if (*y >= editorgridextent) *y = editorgridextent;
}

int getlinehighlight(int xplc, int yplc)
{
	if (numwalls == 0)
		return(-1);
	int dist{0x7fffffff};
	int closest = numwalls - 1;
	
	for(int i{0}; i < numwalls; ++i)
	{
		int nx{0};
		int ny{0};

		getclosestpointonwall(xplc, yplc, i, &nx, &ny);
		const int dst = std::abs(xplc - nx) + std::abs(yplc - ny);

		if (dst <= dist) {
			dist = dst;
			closest = i;
		}
	}

	if (wall[closest].nextwall >= 0)
	{    //if red line, allow highlighting of both sides
		const int x1 = wall[closest].pt.x;
		const int y1 = wall[closest].pt.y;
		const int x2 = wall[wall[closest].point2].pt.x;
		const int y2 = wall[wall[closest].point2].pt.y;

		if (dmulscalen<32>(xplc - x1, y2 - y1, -(x2 - x1), yplc - y1) >= 0)
			closest = wall[closest].nextwall;
	}

	return closest;
}

int getpointhighlight(int xplc, int yplc)
{
	if (numwalls == 0)
		return(-1);

	int dist{0};
	if (grid > 0)
		dist = 1024;

	int closest{-1};
	
	for(int i{0}; i < numwalls; ++i)
	{
		const int dst = std::abs(xplc-wall[i].pt.x) + std::abs(yplc-wall[i].pt.y);
		if (dst <= dist) {
			dist = dst;
			closest = i;
		}
	}

	for(int i{0}; i < MAXSPRITES; ++i) {
		if (sprite[i].statnum < MAXSTATUS)
		{
			const int dst = std::abs(xplc-sprite[i].x) + std::abs(yplc-sprite[i].y);
			
			if (dst <= dist) {
				dist = dst;
				closest = i + 16384;
			}
		}
	}

	return closest;
}

void adjustmark(int *xplc, int *yplc, short danumwalls)
{
	if (danumwalls < 0)
		danumwalls = numwalls;

	int pointlockdist{0};
	if ((grid > 0) && (gridlock > 0))
		pointlockdist = (128>>grid);

	int dist = pointlockdist;
	int dax = *xplc;
	int day = *yplc;
	
	for(int i{0}; i < danumwalls; ++i)
	{
		const int dst = std::abs((*xplc)-wall[i].pt.x) + std::abs((*yplc)-wall[i].pt.y);
		
		if (dst < dist)
		{
			dist = dst;
			dax = wall[i].pt.x;
			day = wall[i].pt.y;
		}
	}

	if (dist == pointlockdist)
		if ((gridlock > 0) && (grid > 0))
		{
			dax = ((dax+(1024>>grid))&(0xffffffff<<(11-grid)));
			day = ((day+(1024>>grid))&(0xffffffff<<(11-grid)));
		}

	*xplc = dax;
	*yplc = day;
}

bool checkautoinsert(int dax, int day, short danumwalls)
{
	if (danumwalls < 0)
		danumwalls = numwalls;

	for(int i{0}; i < danumwalls; ++i)       // Check if a point should be inserted
	{
		const int x1 = wall[i].pt.x;
		const int y1 = wall[i].pt.y;
		const int x2 = wall[wall[i].point2].pt.x;
		const int y2 = wall[wall[i].point2].pt.y;

		// FIXME: Fastest way to check this?
		if ((x1 != dax) || (y1 != day))
			if ((x2 != dax) || (y2 != day))
				if (((x1 <= dax) && (dax <= x2)) || ((x2 <= dax) && (dax <= x1)))
					if (((y1 <= day) && (day <= y2)) || ((y2 <= day) && (day <= y1)))
						if ((dax-x1)*(y2-y1) == (day-y1)*(x2-x1))
							return true;          //insertpoint((short)i,dax,day);
	}

	return false;
}

ClockDir_t clockdir(short wallstart)   //Returns: 0 is CW, 1 is CCW
{
	short i{wallstart - 1};
	short themin{-1};
	int minx{0x7FFFFFFF};

	do
	{
		i++;
		if (wall[wall[i].point2].pt.x < minx)
		{
			minx = wall[wall[i].point2].pt.x;
			themin = i;
		}
	} while ((wall[i].point2 != wallstart) && (i < MAXWALLS));

	const int x0 = wall[themin].pt.x;
	const int y0 = wall[themin].pt.y;
	const int x1 = wall[wall[themin].point2].pt.x;
	const int y1 = wall[wall[themin].point2].pt.y;
	const int x2 = wall[wall[wall[themin].point2].point2].pt.x;
	const int y2 = wall[wall[wall[themin].point2].point2].pt.y;

	if ((y1 >= y2) && (y1 <= y0)) return ClockDir_t::CW;
	if ((y1 >= y0) && (y1 <= y2)) return ClockDir_t::CCW;

	const int templong = (x0 - x1) * (y2 - y1) - (x2 - x1) * (y0 - y1);

	if (templong < 0)
		return ClockDir_t::CW;
	else
		return ClockDir_t::CCW;
}

void flipwalls(short numwalls, short newnumwalls)
{
	const int nume = newnumwalls - numwalls;

	for(int i{numwalls}; i < numwalls + (nume >> 1); ++i)
	{
		const int j = numwalls + newnumwalls - i - 1;
		std::swap(wall[i].pt.x, wall[j].pt.x);
		std::swap(wall[i].pt.y, wall[j].pt.y);
	}
}

void insertpoint(short linehighlight, int dax, int day)
{
	int j = linehighlight;
	short sucksect = sectorofwall((short)j);

	++g_sector[sucksect].wallnum;

	for(int i = sucksect + 1; i < numsectors; ++i) {
		++g_sector[i].wallptr;
	}

	movewalls((int)j+1,+1L);
	std::memcpy(&wall[j+1],&wall[j],sizeof(walltype));

	wall[j].point2 = j+1;
	wall[j+1].pt.x = dax;
	wall[j+1].pt.y = day;
	fixrepeats((short)j);
	fixrepeats((short)j+1);

	if (wall[j].nextwall >= 0)
	{
		const short k = wall[j].nextwall;

		sucksect = sectorofwall((short)k);

		g_sector[sucksect].wallnum++;
		for(int i = sucksect + 1; i < numsectors; ++i) {
			++g_sector[i].wallptr;
		}

		movewalls((int)k+1,+1L);
		std::memcpy(&wall[k+1],&wall[k],sizeof(walltype));

		wall[k].point2 = k+1;
		wall[k+1].pt.x = dax;
		wall[k+1].pt.y = day;
		fixrepeats((short)k);
		fixrepeats((short)k+1);

		j = wall[k].nextwall;
		wall[j].nextwall = k+1;
		wall[j+1].nextwall = k;
		wall[k].nextwall = j+1;
		wall[k+1].nextwall = j;
	}
}

void deletepoint(short point)
{
	const int sucksect = sectorofwall(point);

	--g_sector[sucksect].wallnum;

	for(int i = sucksect + 1; i < numsectors; ++i) {
		--g_sector[i].wallptr;
	}

	const int j = lastwall(point);
	const int k = wall[point].point2;
	wall[j].point2 = k;

	if (wall[j].nextwall >= 0)
	{
		wall[wall[j].nextwall].nextwall = -1;
		wall[wall[j].nextwall].nextsector = -1;
	}

	if (wall[point].nextwall >= 0)
	{
		wall[wall[point].nextwall].nextwall = -1;
		wall[wall[point].nextwall].nextsector = -1;
	}

	movewalls((int)point, -1L);

	checksectorpointer((short)j, (short)sucksect);
}

void deletesector(short sucksect)
{
	while (headspritesect[sucksect] >= 0) {
		deletesprite(headspritesect[sucksect]);
	}

	updatenumsprites();

	const int startwall = g_sector[sucksect].wallptr;
	const int endwall = startwall + g_sector[sucksect].wallnum - 1;
	int j = g_sector[sucksect].wallnum;

	for(int i{sucksect}; i < numsectors - 1; ++i)
	{
		int k = headspritesect[i + 1];
		
		while (k != -1)
		{
			const int nextk = nextspritesect[k];
			changespritesect((short)k,(short)i);
			k = nextk;
		}

		std::memcpy(&g_sector[i],&g_sector[i+1],sizeof(sectortype));
		g_sector[i].wallptr -= j;
	}
	
	numsectors--;

	j = endwall-startwall+1;
	
	for (int i{startwall}; i <= endwall; ++i) {
		if (wall[i].nextwall != -1)
		{
			wall[wall[i].nextwall].nextwall = -1;
			wall[wall[i].nextwall].nextsector = -1;
		}
	}

	movewalls(startwall, -j);
	
	for(int i{0}; i < numwalls; ++i) {
		if (wall[i].nextwall >= startwall)
			wall[i].nextsector--;
	}
}

void fixspritesectors()
{
	for(int i = numsectors - 1; i >= 0; --i) {
		if ((g_sector[i].wallnum <= 0) || (g_sector[i].wallptr >= numwalls))
			deletesector((short)i);
	}

	for(int i{0}; i < MAXSPRITES; ++i) {
		if (sprite[i].statnum < MAXSTATUS)
		{
			const int dax = sprite[i].x;
			const int day = sprite[i].y;

			if (inside(dax, day, sprite[i].sectnum) != 1)
			{
				const int daz = ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<2);

				for(int j{0}; j < numsectors; ++j) {
					if (inside(dax, day, (short)j) == 1) {
						if (sprite[i].z >= getceilzofslope(j, dax, day)) {
							if (sprite[i].z - daz <= getflorzofslope(j, dax, day))
							{
								changespritesect((short)i,(short)j);
								break;
							}
						}
					}
				}
			}
		}
	}
}

void movewalls(int start, int offs)
{
	if (offs < 0)  //Delete
	{
		for(int i{start}; i < numwalls + offs; ++i)
			std::memcpy(&wall[i], &wall[i - offs], sizeof(walltype));
	}
	else if (offs > 0)  //Insert
	{
		for(int i = numwalls + offs - 1; i >= start + offs; --i)
			std::memcpy(&wall[i],&wall[i-offs],sizeof(walltype));
	}
	
	numwalls += offs;

	for(int i{0}; i < numwalls; ++i)
	{
		if (wall[i].nextwall >= start)
			wall[i].nextwall += offs;

		if (wall[i].point2 >= start)
			wall[i].point2 += offs;
	}
}

void checksectorpointer(short i, short sectnum)
{
	const int x1 = wall[i].pt.x;
	const int y1 = wall[i].pt.y;
	const int x2 = wall[wall[i].point2].pt.x;
	const int y2 = wall[wall[i].point2].pt.y;

	if (wall[i].nextwall >= 0)          //Check for early exit
	{
		const int k = wall[i].nextwall;
		if ((wall[k].pt.x == x2) && (wall[k].pt.y == y2))
			if ((wall[wall[k].point2].pt.x == x1) && (wall[wall[k].point2].pt.y == y1))
				return;
	}

	wall[i].nextsector = -1;
	wall[i].nextwall = -1;

	for(int j{0}; j < numsectors; ++j)
	{
		const int startwall = g_sector[j].wallptr;
		const int endwall = startwall + g_sector[j].wallnum - 1;

		for(int k{startwall}; k <= endwall; ++k)
		{
			if ((wall[k].pt.x == x2) && (wall[k].pt.y == y2))
				if ((wall[wall[k].point2].pt.x == x1) && (wall[wall[k].point2].pt.y == y1))
					if (j != sectnum)
					{
						wall[i].nextsector = j;
						wall[i].nextwall = k;
						wall[k].nextsector = sectnum;
						wall[k].nextwall = i;
					}
		}
	}
}

void fixrepeats(short i)
{
	int dax = wall[wall[i].point2].pt.x - wall[i].pt.x;
	int day = wall[wall[i].point2].pt.y - wall[i].pt.y;
	const int dist = static_cast<int>(std::hypot(dax, day));
	dax = wall[i].xrepeat; // TODO: Why set this again?
	day = wall[i].yrepeat;
	wall[i].xrepeat = static_cast<unsigned char>(std::min(std::max(mulscalen<10>(dist, day), 1), 255));
}

void clearmidstatbar16()
{
	setstatusbarviewport();
	clearbuf((unsigned char *)(frameplace + (bytesperline*(ytop16+25L))),(bytesperline*(ydim16-1-(25<<1))) >> 2, 0x08080808L);
	drawline16(0,0,0,ydim16-1,7);
	drawline16(xdim-1,0,xdim-1,ydim16-1,7);
	restoreviewport();
}

short loopinside(int x, int y, short startwall)
{
	short direc = static_cast<int>(clockdir(startwall));
	short i{startwall};

	do
	{
		int x1 = wall[i].pt.x;
		int x2 = wall[wall[i].point2].pt.x;

		if ((x1 >= x) || (x2 >= x))
		{
			int y1 = wall[i].pt.y;
			int y2 = wall[wall[i].point2].pt.y;

			if (y1 > y2)
			{
				std::swap(x1, x2);
				std::swap(y1, y2);
			}

			if ((y1 <= y) && (y2 > y))
				if (x1*(y-y2)+x2*(y1-y) <= x*(y1-y2))
					direc ^= 1;
		}

		i = wall[i].point2;
	} while (i != startwall);
	
	return direc;
}

int numloopsofsector(short sectnum)
{
	int numloops{0};
	const int startwall = g_sector[sectnum].wallptr;
	const int endwall = startwall + g_sector[sectnum].wallnum;

	for(int i{startwall}; i < endwall; ++i)
		if (wall[i].point2 < i)
			++numloops;

	return numloops;
}

short getnumber16(char *namestart, short num, int maxnumber, char sign)
{
	char buffer[80];

	int danum = static_cast<int>(num);
	int oldnum = danum;
	bflushchars();

	while (keystatus[0x1] == 0)
	{
		if (handleevents()) {
			if (quitevent) quitevent = false;
		}

		const auto ch = bgetchar();

		fmt::format_to(buffer, "{}{}_ ", namestart, danum);
		printmessage16(buffer);
		showframe();

		if (ch >= '0' && ch <= '9') {
			int n{0};
			if (sign && danum<0)
				n = (danum*10)-(ch-'0');
			else
				n = (danum*10)+(ch-'0');
			if (n < maxnumber) danum = n;
		} else if (ch == 8 || ch == 127) {	// backspace
			danum /= 10;
		} else if (ch == 13) {
			oldnum = danum;
			asksave = true;
			break;
		} else if (ch == '-' && sign) {	// negate
			danum = -danum;
		}
	}
	
	clearkeys();

	return((short)oldnum);
}

short getnumber256(char *namestart, short num, int maxnumber, char sign)
{
	char buffer[80];

	int danum = static_cast<int>(num);
	int oldnum = danum;
	bflushchars();

	while (keystatus[0x1] == 0)
	{
		if (handleevents()) {
			if (quitevent) quitevent = false;
		}

		drawrooms(posx,posy,posz,ang,horiz,cursectnum);
		ExtAnalyzeSprites();
		drawmasks();

		const auto ch = bgetchar();

		fmt::format_to(buffer, "{}{}_ ", namestart, danum);
		printmessage256(buffer);
		showframe();

		if (ch >= '0' && ch <= '9') {
			int n{0};
			if (sign && danum<0)
				n = (danum*10)-(ch-'0');
			else
				n = (danum*10)+(ch-'0');
			if (n < maxnumber)
				danum = n;
		}
		else if (ch == 8 || ch == 127) {	// backspace
			danum /= 10;
		}
		else if (ch == 13) {
			oldnum = danum;
			asksave = true;
			break;
		}
		else if (ch == '-' && sign) {	// negate
			danum = -danum;
		}
	}
	clearkeys();

	lockclock = totalclock;  //Reset timing

	return((short)oldnum);
}

void clearfilenames()
{
	klistfree(finddirs);
	klistfree(findfiles);
	finddirs = findfiles = nullptr;
	numfiles = numdirs = 0;
}

void getfilenames(const std::string& path, const std::string& kind)
{
	CACHE1D_FIND_REC *r;
	int type = 0;

	if (pathsearchmode == PATHSEARCH_GAME && grponlymode) {
		type = CACHE1D_OPT_NOSTACK;
	}

	clearfilenames();
	finddirs = klistpath(path.c_str(), "*", CACHE1D_FIND_DIR|CACHE1D_FIND_DRIVE|type);
	findfiles = klistpath(path.c_str(), kind.c_str(), CACHE1D_FIND_FILE | type);
	for (r = finddirs; r; r=r->next) numdirs++;
	for (r = findfiles; r; r=r->next) numfiles++;

	finddirshigh = finddirs;
	findfileshigh = findfiles;
	currentlist = 0;
	if (findfileshigh) currentlist = 1;
}

std::string findfilename(const std::string& path)
{
	std::string filename = AfterLast(path, '/');
#ifdef _WIN32
	filename = std::max(filename.c_str(), std::strrchr(path.data(), '\\')); // FIXME: Hmm..
#endif
	if (!filename.empty()) {
		filename = AfterFirst(filename, '/'); // FIXME: Hmm..
	} else {
		filename = path;
	}

	return filename;
}

int menuselect(int newpathmode)
{
	int i;
	char ch;
	std::array<char, 90> buffer;
	CACHE1D_FIND_REC *dir;

	const int bakpathsearchmode{ pathsearchmode };

	const int listsize = (ydim16 - 32) / 8;

	if (newpathmode != pathsearchmode) {
		selectedboardfilename.clear();
		pathsearchmode = newpathmode;
	}
	if (pathsearchmode == PATHSEARCH_SYSTEM) {
		Bcanonicalisefilename(&selectedboardfilename[0], 1);		// clips off the last token and compresses relative path
	} else {
		Bcorrectfilename(&selectedboardfilename[0], 1);
	}

	getfilenames(&selectedboardfilename[0], "*.map");

	printmessage16("Select .MAP file with arrows&enter.");

	do {
		clearbuf((unsigned char *)frameplace, (bytesperline*ydim16) >> 2, 0L);

		if (pathsearchmode == PATHSEARCH_SYSTEM) {
			std::strcpy(&buffer[0],"Local filesystem mode. Press F for game filesystem.");
		} else {
			std::strcpy(&buffer[0], "Game filesystem");
			if (grponlymode) std::strcat(&buffer[0], " GRP-only");
			std::strcat(&buffer[0], " mode. Press F for local filesystem, G for ");
			if (grponlymode) std::strcat(&buffer[0], "all files.");
			else std::strcat(&buffer[0], "GRP files only.");
		}
		printext16(halfxdim16-(8*(int)std::strlen(&buffer[0])/2), 4, 14, 0, &buffer[0], 0);

		fmt::format_to(&buffer[0],"({} dirs, {} files) {}", numdirs, numfiles, &selectedboardfilename[0]);
		printext16(1,ydim16-8-1,8,0, &buffer[0], 0);

		if (finddirshigh) {
			dir = finddirshigh;
			for(i=listsize/2-1; i>=0; i--) if (!dir->prev) break; else dir=dir->prev;
			for(i=0; i<listsize && dir; i++, dir=dir->next) {
				const int c = dir->type == CACHE1D_FIND_DIR ? 4 : 3;
				std::ranges::fill(buffer, 0);
				dir->name.copy(&buffer[0], 25);
				if (std::strlen(&buffer[0]) == 25)
					buffer[21] = buffer[22] = buffer[23] = '.', buffer[24] = 0;
				if (dir == finddirshigh) {
					if (currentlist == 0) printext16(8,16+8*i,c|8,0,"->",0);
					printext16(32, 16 + 8 * i, c | 8, 0, &buffer[0], 0);
				} else {
					printext16(32, 16 + 8 * i, c, 0, &buffer[0], 0);
				}
			}
		}

		if (findfileshigh) {
			dir = findfileshigh;
			for(i=listsize/2-1; i>=0; i--) if (!dir->prev) break; else dir=dir->prev;
			for(i=0; i<listsize && dir; i++, dir=dir->next) {
				if (dir == findfileshigh) {
					if (currentlist == 1) printext16(240,16+8*i,7|8,0,"->",0);
					printext16(240+24,16+8*i,7|8,0,dir->name,0);
				} else {
					printext16(240+24,16+8*i,7,0,dir->name,0);
				}
			}
		}
		showframe();

		keystatus[0xcb] = 0;
		keystatus[0xcd] = 0;
		keystatus[0xc8] = 0;
		keystatus[0xd0] = 0;
		keystatus[0x1c] = 0;	//enter
		keystatus[0xf] = 0;		//tab
		keystatus[1] = 0;		//esc
		ch = 0;                      //Interesting fakery of ch = getch()
		while (ch == 0)
		{
			if (handleevents()) {
				if (quitevent) {
					keystatus[1] = 1;
					quitevent = false;
				}
			}
			ch = bgetchar();
			if (keystatus[0xcb] > 0) ch = 9;		// left arr
			if (keystatus[0xcd] > 0) ch = 9;		// right arr
			if (keystatus[0xc8] > 0) ch = 72;		// up arr
			if (keystatus[0xd0] > 0) ch = 80;		// down arr

		}

		if (ch == 'f' || ch == 'F') {
			currentlist = 0;
			pathsearchmode = 1-pathsearchmode;
			if (pathsearchmode == PATHSEARCH_SYSTEM) {
				selectedboardfilename.clear();
				Bcanonicalisefilename(&selectedboardfilename[0], 1);
			}
			else
				selectedboardfilename = "/";

			getfilenames(&selectedboardfilename[0], "*.map");
		} else if (ch == 'g' || ch == 'G') {
			if (pathsearchmode == PATHSEARCH_GAME) {
				grponlymode = 1-grponlymode;
				getfilenames(&selectedboardfilename[0], "*.map");
			}
		} else if (ch == 9) {
			if ((currentlist == 0 && findfiles) || (currentlist == 1 && finddirs))
				currentlist = 1-currentlist;
		} else if ((ch == 75) || (ch == 72)) {
			if (currentlist == 0) {
				if (finddirshigh && finddirshigh->prev) finddirshigh = finddirshigh->prev;
			} else {
				if (findfileshigh && findfileshigh->prev) findfileshigh = findfileshigh->prev;
			}
		} else if ((ch == 77) || (ch == 80)) {
			if (currentlist == 0) {
				if (finddirshigh && finddirshigh->next) finddirshigh = finddirshigh->next;
			} else {
				if (findfileshigh && findfileshigh->next) findfileshigh = findfileshigh->next;
			}
		} else if ((ch == 13) && (currentlist == 0) && finddirshigh) {
			if (finddirshigh->type == CACHE1D_FIND_DRIVE) {
				selectedboardfilename = finddirshigh->name;
			} else {
				selectedboardfilename.append(finddirshigh->name);
			}
			selectedboardfilename.append("/");
			if (pathsearchmode == PATHSEARCH_SYSTEM)
				Bcanonicalisefilename(&selectedboardfilename[0], 1);
			else
				Bcorrectfilename(&selectedboardfilename[0], 1);

			//std::printf("Changing directories to: {}\n", selectedboardfilename);

			getfilenames(&selectedboardfilename[0], "*.map");
			ch = 0;

			clearbuf((unsigned char *)frameplace, (bytesperline*ydim16) >> 2, 0L);
			showframe();
		}
		if (ch == 13 && !findfileshigh) ch = 0;
	}
	while ((ch != 13) && (ch != 27));
	if (ch == 13 && findfileshigh)
	{
		selectedboardfilename.append(findfileshigh->name);
		//std::printf("Selected file: {}\n", selectedboardfilename);

		return(0);
	}
	pathsearchmode = bakpathsearchmode;
	return(-1);
}

void fillsector(short sectnum, unsigned char fillcolor)
{
	const int lborder{ 0 };
	const int rborder{ xdim };
	constexpr int uborder{ 0 };
	const int dborder{ ydim16 };

	if (sectnum == -1) {
		return;
	}

	int miny = dborder - 1;
	int maxy{ uborder };

	const short startwall = g_sector[sectnum].wallptr;
	const short endwall = startwall + g_sector[sectnum].wallnum - 1;

	for(short z{startwall}; z <= endwall; ++z)
	{
		const short y1 = (((wall[z].pt.y-posy)*zoom)>>14)+midydim16;
		const short y2 = (((wall[wall[z].point2].pt.y-posy)*zoom)>>14)+midydim16;

		if (y1 < miny)
			miny = y1;

		if (y2 < miny)
			miny = y2;

		if (y1 > maxy)
			maxy = y1;

		if (y2 > maxy)
			maxy = y2;
	}

	if (miny < uborder)
		miny = uborder;

	if (maxy >= dborder)
		maxy = dborder - 1;

	for(int sy = miny + ((totalclock >> 2) & 3); sy <= maxy; sy += 3)	// JBF 20040116: numframes%3 -> (totalclock>>2)&3
	{
		const int y = posy+(((sy-midydim16)<<14)/zoom);

		fillist[0] = lborder;
		short fillcnt{1};

		for(short z{startwall}; z <= endwall; ++z)
		{
			int x1 = wall[z].pt.x;
			int x2 = wall[wall[z].point2].pt.x;
			int y1 = wall[z].pt.y;
			int y2 = wall[wall[z].point2].pt.y;
			
			if (y1 > y2)
			{
				std::swap(x1, x2);
				std::swap(y1, y2);
			}

			if ((y1 <= y) && (y2 > y)) {
				//if (x1*(y-y2) + x2*(y1-y) <= 0)
				int dax = x1+scale(y-y1,x2-x1,y2-y1);
				dax = (((dax-posx)*zoom)>>14)+halfxdim16;
				if (dax >= lborder)
					fillist[fillcnt++] = dax;
			}
		}

		if (fillcnt > 0)
		{
			for(short z{1}; z < fillcnt; ++z) {
				for (int zz{0}; zz < z; ++zz) {
					if (fillist[z] < fillist[zz])
					{
						std::swap(fillist[z], fillist[zz]);
					}
				}
			}

			for (short z = fillcnt & 1; z < fillcnt - 1; z += 2)
			{
				if (fillist[z] > rborder)
					break;

				if (fillist[z+1] > rborder)
					fillist[z+1] = rborder;

				drawline16(fillist[z], sy, fillist[z + 1], sy, fillcolor);
			}
		}
	}
}

short whitelinescan(short dalinehighlight)
{
	const short sucksect = sectorofwall(dalinehighlight);

	std::memcpy(&g_sector[numsectors],&g_sector[sucksect],sizeof(sectortype));
	g_sector[numsectors].wallptr = numwalls;
	g_sector[numsectors].wallnum = 0;
	
	int i = dalinehighlight;

	short newnumwalls = numwalls;

	do
	{
		int j = lastwall((short)i);
		if (wall[j].nextwall >= 0)
		{
			j = wall[j].point2;
			for(int k{0}; k < numwalls; ++k)
			{
				if (wall[wall[k].point2].pt.x == wall[j].pt.x)
					if (wall[wall[k].point2].pt.y == wall[j].pt.y)
						if (wall[k].nextwall == -1)
						{
							j = k;
							break;
						}
			}
		}

		std::memcpy(&wall[newnumwalls],&wall[i],sizeof(walltype));

		wall[newnumwalls].nextwall = j;
		wall[newnumwalls].nextsector = sectorofwall((short)j);

		newnumwalls++;
		g_sector[numsectors].wallnum++;

		i = j;
	} while (i != dalinehighlight);

	for(i=numwalls;i<newnumwalls-1;i++)
		wall[i].point2 = i+1;
	wall[newnumwalls-1].point2 = numwalls;

	if (clockdir(numwalls) == ClockDir_t::CCW) {
		return -1;
	}
	else {
		return newnumwalls;
	}
}

// FIXME: Returning unused values and values that aren't as clear
// as they should be.
int loadnames()
{
	std::array<char, 1024> buffer;
	char* p;
	int syms{ 0 };
	int line{ 0 };

	std::FILE* fp = fopenfrompath("NAMES.HPP", "r");

	if (!fp) {
		if ((fp = fopenfrompath("names.hpp","r")) == nullptr) {
			buildprintf("Failed to open NAMES.H\n");
			return -1;
		}
	}

	//clearbufbyte(names, sizeof(names), 0);
	std::memset(names,0,sizeof(names));

	buildprintf("Loading NAMES.H\n");

	while (std::fgets(&buffer[0], 1024, fp)) {
		const int a = (int)std::strlen(&buffer[0]);
		if (a >= 1) {
			if (a > 1)
				if (buffer[a-2] == '\r') buffer[a-2] = 0;
			if (buffer[a-1] == '\n') buffer[a-1] = 0;
		}

		p = &buffer[0];
		line++;
		while (*p == 32) p++;
		if (*p == 0) continue;	// blank line

		if (*p == '#') {
			p++;
			while (*p == 32) p++;
			if (*p == 0) continue;	// null directive

			if (!std::strncmp(p, "define ", 7)) {
				// #define_...
				p += 7;
				while (*p == 32) p++;
				if (*p == 0) {
					buildprintf("Error: Malformed #define at line {}\n", line-1);
					continue;
				}

				char* name = p;
				while (*p != 32 && *p != 0) p++;
				if (*p == 32) {
					*(p++) = 0;
					while (*p == 32) p++;
					if (*p == 0) {	// #define_NAME with no number
						buildprintf("Error: No number given for name \"{}\" (line {})\n", name, line-1);
						continue;
					}

					const char* number = p;
					while (*p != 0) p++;
					if (*p != 0) *p = 0;

					// add to list
					char* endptr{nullptr};
					const int num = (int)strtol(number, &endptr, 10);
					if (*endptr != 0) {
						p = endptr;
						goto badline;
					}
					if (num < 0 || num >= MAXTILES) {
						buildprintf("Error: Constant {} for name \"{}\" out of range (line {})\n", num, name, line-1);
						continue;
					}

					if (std::strlen(name) > 24)
						buildprintf("Warning: Name \"{}\" longer than 24 characters (line {}). Truncating.\n", name, line-1);

					std::strncpy(names[num], name, 24);
					names[num][24] = 0;

					syms++;

					continue;

				} else {	// #define_NAME with no number
					buildprintf("Error: No number given for name \"{}\" (line {})\n", name, line-1);
					continue;
				}
			} else goto badline;
		} else if (*p == '/') {
			if (*(p+1) == '/') continue;	// comment
		}
badline:
		buildprintf("Error: Invalid statement found at character {} on line {}\n", (int)(p - &buffer[0]), line-1);
	}
	buildprintf("Read {} lines, loaded {} names.\n", line, syms);

	std::fclose(fp);
	return 0;
}


//
// drawline16
//
// JBF: Had to add extra tests to make sure x-coordinates weren't winding up -'ve
//   after clipping or crashes would ensue

void drawline16(int x1, int y1, int x2, int y2, unsigned char col)
{
	int i;
	int pinc;
	int d;
	intptr_t p;
	unsigned int patc=0;

	int dx = x2 - x1;
	int dy = y2 - y1;

	if (dx >= 0)
	{
		if ((x1 >= xres) || (x2 < 0))
			return;
		if (x1 < 0) { if (dy) y1 += scale(0-x1,dy,dx); x1 = 0; }
		if (x2 >= xres) { if (dy) y2 += scale(xres-1-x2,dy,dx); x2 = xres-1; }
	}
	else
	{
		if ((x2 >= xres) || (x1 < 0)) return;
		if (x2 < 0) { if (dy) y2 += scale(0-x2,dy,dx); x2 = 0; }
		if (x1 >= xres) { if (dy) y1 += scale(xres-1-x1,dy,dx); x1 = xres-1; }
	}
	if (dy >= 0)
	{
		if ((y1 >= ydim16) || (y2 < 0)) return;
		if (y1 < 0) { if (dx) x1 += scale(0-y1,dx,dy); y1 = 0; if (x1 < 0) x1 = 0; }
		if (y2 >= ydim16) { if (dx) x2 += scale(ydim16-1-y2,dx,dy); y2 = ydim16-1; if (x2 < 0) x2 = 0; }
	}
	else
	{
		if ((y2 >= ydim16) || (y1 < 0)) return;
		if (y2 < 0) { if (dx) x2 += scale(0-y2,dx,dy); y2 = 0; if (x2 < 0) x2 = 0; }
		if (y1 >= ydim16) { if (dx) x1 += scale(ydim16-1-y1,dx,dy); y1 = ydim16-1; if (x1 < 0) x1 = 0; }
	}

	dx = std::abs(x2 - x1) + 1;
	dy = std::abs(y2 - y1) + 1;
	if (dx >= dy)
	{
		if (x2 < x1)
		{
			std::swap(x1, x2);
			std::swap(y1, y2);
		}
		d = 0;
		if (y2 > y1) pinc = bytesperline; else pinc = -bytesperline;

		p = ((ytop16+y1)*bytesperline)+x1+frameplace;
		if (dy == 0 && drawlinepat == 0xffffffff) {
			i = ((int)col<<24)|((int)col<<16)|((int)col<<8)|col;
			clearbufbyte((void *)p, dx, i);
		} else
		for(i=dx;i>0;i--)
		{
			if (drawlinepat & pow2long[(patc++) & 31])
				drawpixel((void *)p, col);
			d += dy;
			if (d >= dx) { d -= dx; p += pinc; }
			p++;
		}
		return;
	}

	if (y2 < y1)
	{
		std::swap(x1, x2);
		std::swap(y1, y2);
	}

	d = 0;
	
	if (x2 > x1)
		pinc = 1;
	else
		pinc = -1;

	p = ((ytop16+y1)*bytesperline)+x1+frameplace;
	for(i=dy;i>0;i--)
	{
		if (drawlinepat & pow2long[(patc++) & 31])
			drawpixel((void *)p, col);
		d += dx;
		if (d >= dy) { d -= dy; p += pinc; }
		p += bytesperline;
	}
}

void drawcircle16(int x1, int y1, int r, unsigned char col)
{
	if (r < 0) r = -r;
	if (x1+r < 0 || x1-r >= xres) return;
	if (y1+r < 0 || y1-r >= ydim16) return;

	/*
	 *      d
	 *    6 | 7
	 *   \  |  /
	 *  5  \|/  8
	 * c----+----a
	 *  4  /|\  1
	 *   /  |  \
	 *    3 | 2
	 *      b
	 */

	int xp{0};
	int yp{r};
	int d = 1 - r;
	int de = 2;
	int dse = 5 - (r << 1);

	intptr_t p = ((ytop16 + y1) * bytesperline) + x1 + frameplace;

	int patc{0};
	if (drawlinepat & pow2long[(patc++) & 31]) {
		if ((unsigned int)y1 < (unsigned int)ydim16 && (unsigned int)(x1+r) < (unsigned int)xres  )
			drawpixel((void *)(p+r), col);			// a
		if ((unsigned int)x1 < (unsigned int)xres   && (unsigned int)(y1+r) < (unsigned int)ydim16)
			drawpixel((void *)(p+(r*bytesperline)), col);	// b
		if ((unsigned int)y1 < (unsigned int)ydim16 && (unsigned int)(x1-r) < (unsigned int)xres  )
			drawpixel((void *)(p-r), col);			// c
		if ((unsigned int)x1 < (unsigned int)xres   && (unsigned int)(y1-r) < (unsigned int)ydim16)
			drawpixel((void *)(p-(r*bytesperline)), col);	// d
	}

	while (yp > xp) {
		if (d < 0) {
			d += de;
			de += 2;
			dse += 2;
			xp++;
		} else {
			d += dse;
			de += 2;
			dse += 4;
			xp++;
			yp--;
		}

		const int ypbpl = yp * bytesperline;
		const int xpbpl = xp * bytesperline;
		
		if (drawlinepat & pow2long[(patc++) & 31]) {
			if ((unsigned int)(x1+yp) < (unsigned int)xres && (unsigned int)(y1+xp) < (unsigned int)ydim16)
				drawpixel((void *)(p+yp+xpbpl), col);	// 1
			if ((unsigned int)(x1+xp) < (unsigned int)xres && (unsigned int)(y1+yp) < (unsigned int)ydim16)
				drawpixel((void *)(p+xp+ypbpl), col);	// 2
			if ((unsigned int)(x1-xp) < (unsigned int)xres && (unsigned int)(y1+yp) < (unsigned int)ydim16)
				drawpixel((void *)(p-xp+ypbpl), col);	// 3
			if ((unsigned int)(x1-yp) < (unsigned int)xres && (unsigned int)(y1+xp) < (unsigned int)ydim16)
				drawpixel((void *)(p-yp+xpbpl), col);	// 4
			if ((unsigned int)(x1-yp) < (unsigned int)xres && (unsigned int)(y1-xp) < (unsigned int)ydim16)
				drawpixel((void *)(p-yp-xpbpl), col);	// 5
			if ((unsigned int)(x1-xp) < (unsigned int)xres && (unsigned int)(y1-yp) < (unsigned int)ydim16)
				drawpixel((void *)(p-xp-ypbpl), col);	// 6
			if ((unsigned int)(x1+xp) < (unsigned int)xres && (unsigned int)(y1-yp) < (unsigned int)ydim16)
				drawpixel((void *)(p+xp-ypbpl), col);	// 7
			if ((unsigned int)(x1+yp) < (unsigned int)xres && (unsigned int)(y1-xp) < (unsigned int)ydim16)
				drawpixel((void *)(p+yp-xpbpl), col);	// 8
		}
	}
}


//
// qsetmodeany
//
void qsetmodeany(int daxdim, int daydim)
{
	if (daxdim < 640)
		daxdim = 640;

	if (daydim < 480)
		daydim = 480;

	if (qsetmode != ((daxdim<<16)|(daydim&0xffff))) {
		if (setvideomode(daxdim, daydim, 8, fullscreen) < 0)
			return;

		xdim = xres;
		ydim = yres;
		pixelaspect = 65536;

		setvgapalette();

		setoverheadviewport();
		bakydim16 = ydim16; bakytop16 = ytop16;
		halfxdim16 = xres >> 1;
		midydim16 = scale(200,yres,480);

		clearbuf((void *)(frameplace + (ydim16*bytesperline)), (bytesperline*STATUS2DSIZ) >> 2, 0x08080808L);
		clearbuf((void *)frameplace, (ydim16*bytesperline) >> 2, 0L);
	}

	qsetmode = ((daxdim << 16) | (daydim & 0xffff));
}


//
// clear2dscreen
//
void clear2dscreen()
{
	clearbuf((void *)frameplace, (bytesperline*ydim16) >> 2, 0);
}


//
// draw2dgrid
//
void draw2dgrid(int posxe, int posye, short ange, int zoome, short gride)
{
	int i;
	int xp1;
	int yp1;
	int xp2=0;
	int yp2;
	int tempy;

	std::ignore = ange;

	if (gride > 0)
	{
		yp1 = midydim16-mulscalen<14>(posye+editorgridextent,zoome);
		if (yp1 < 0) yp1 = 0;
		yp2 = midydim16-mulscalen<14>(posye-editorgridextent,zoome);
		if (yp2 >= ydim16) yp2 = ydim16-1;

		if ((yp1 < ydim16) && (yp2 >= 0) && (yp2 >= yp1))
		{
			xp1 = halfxdim16-mulscalen<14>(posxe+editorgridextent,zoome);

			for(i=-editorgridextent;i<=editorgridextent;i+=(2048>>gride))
			{
				xp2 = xp1;
				xp1 = halfxdim16-mulscalen<14>(posxe-i,zoome);

				if (xp1 >= xdim) break;
				if (xp1 >= 0)
				{
					if (xp1 != xp2)
					{
						drawline16(xp1,yp1,xp1,yp2,8);
					}
				}
			}
			if ((i >= editorgridextent) && (xp1 < xdim))
				xp2 = xp1;
			if ((xp2 >= 0) && (xp2 < xdim))
			{
				drawline16(xp2,yp1,xp2,yp2,8);
			}
		}

		xp1 = mulscalen<14>(posxe+editorgridextent,zoome);
		xp2 = mulscalen<14>(posxe-editorgridextent,zoome);
		tempy = 0x80000000L;
		for(i=-editorgridextent;i<=editorgridextent;i+=(2048>>gride))
		{
			yp1 = (((posye-i)*zoome)>>14);
			if (yp1 != tempy)
			{
				if ((yp1 > midydim16-ydim16) && (yp1 <= midydim16))
				{
					drawline16(halfxdim16-xp1,midydim16-yp1,halfxdim16-xp2,midydim16-yp1,8);
					tempy = yp1;
				}
			}
		}
	}
}


//
// draw2dscreen
//
void draw2dscreen(int posxe, int posye, short ange, int zoome, short gride)
{
	walltype *wal;
	int i;
	int j;
	int xp1;
	int yp1;
	int xp2;
	int yp2;
	intptr_t templong;
	unsigned char col;

	if (qsetmode == 200)
		return;

	if (!editstatus)
	{
		faketimerhandler();
		clear2dscreen();

		faketimerhandler();
		draw2dgrid(posxe, posye, ange, zoome, gride);
	}

	faketimerhandler();
	for(i=numwalls-1,wal=&wall[i];i>=0;i--,wal--)
	{
		if (!editstatus)
		{
			if ((show2dwall[i>>3]&pow2char[i&7]) == 0) continue;
			j = wal->nextwall;
			if ((j >= 0) && (i > j))
				if ((show2dwall[j>>3]&pow2char[j&7]) > 0) continue;
		}
		else
		{
			j = wal->nextwall;
			if ((j >= 0) && (i > j)) continue;
		}

		if (j < 0)
		{
			col = 7;
			if (i == linehighlight) if (totalclock & 8) col += (2<<2);
		}
		else
		{
			col = 4;
			if ((wal->cstat&1) != 0) col = 5;
			if ((i == linehighlight) || ((linehighlight >= 0) && (i == wall[linehighlight].nextwall)))
				if (totalclock & 8) col += (2<<2);
		}

		xp1 = mulscalen<14>(wal->pt.x-posxe,zoome);
		yp1 = mulscalen<14>(wal->pt.y-posye,zoome);
		xp2 = mulscalen<14>(wall[wal->point2].pt.x-posxe,zoome);
		yp2 = mulscalen<14>(wall[wal->point2].pt.y-posye,zoome);

		if ((wal->cstat&64) > 0)
		{
			if (std::abs(xp2-xp1) >= std::abs(yp2-yp1))
			{
				drawline16(halfxdim16+xp1,midydim16+yp1+1,halfxdim16+xp2,midydim16+yp2+1,col);
				drawline16(halfxdim16+xp1,midydim16+yp1-1,halfxdim16+xp2,midydim16+yp2-1,col);
			}
			else
			{
				drawline16(halfxdim16+xp1+1,midydim16+yp1,halfxdim16+xp2+1,midydim16+yp2,col);
				drawline16(halfxdim16+xp1-1,midydim16+yp1,halfxdim16+xp2-1,midydim16+yp2,col);
			}
			col += 8;
		}
		drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16+xp2,midydim16+yp2,col);

		if ((zoome >= 256) && editstatus)
			if (((halfxdim16+xp1) >= 2) && ((halfxdim16+xp1) <= xdim-3))
				if (((midydim16+yp1) >= 2) && ((midydim16+yp1) <= ydim16-3))
				{
					col = 2;
					if (i == pointhighlight) {
						if (totalclock & 8) col += (2<<2);
					}
					else if ((highlightcnt > 0) && editstatus)
					{
						if (show2dwall[i>>3]&pow2char[i&7])
							if (totalclock & 8) col += (2<<2);
					}

					templong = ((midydim16+yp1)*bytesperline)+(halfxdim16+xp1)+frameplace;
					drawpixel((void *)(templong-2-(bytesperline<<1)), col);
					drawpixel((void *)(templong-1-(bytesperline<<1)), col);
					drawpixel((void *)(templong+0-(bytesperline<<1)), col);
					drawpixel((void *)(templong+1-(bytesperline<<1)), col);
					drawpixel((void *)(templong+2-(bytesperline<<1)), col);

					drawpixel((void *)(templong-2+(bytesperline<<1)), col);
					drawpixel((void *)(templong-1+(bytesperline<<1)), col);
					drawpixel((void *)(templong+0+(bytesperline<<1)), col);
					drawpixel((void *)(templong+1+(bytesperline<<1)), col);
					drawpixel((void *)(templong+2+(bytesperline<<1)), col);

					drawpixel((void *)(templong-2-bytesperline), col);
					drawpixel((void *)(templong-2+0), col);
					drawpixel((void *)(templong-2+bytesperline), col);

					drawpixel((void *)(templong+2-bytesperline), col);
					drawpixel((void *)(templong+2+0), col);
					drawpixel((void *)(templong+2+bytesperline), col);
				}
	}
	faketimerhandler();

	if ((zoome >= 256) || !editstatus)
		for(i=0;i<numsectors;i++)
			for(j=headspritesect[i];j>=0;j=nextspritesect[j])
				if (editstatus || (show2dsprite[j>>3]&pow2char[j&7]))
				{
					col = 3;
					if ((sprite[j].cstat&1) > 0) col = 5;
					if (editstatus)
					{
						if (j+16384 == pointhighlight) {
							if (totalclock & 8) col += (2<<2);
						}
						else if ((highlightcnt > 0) && editstatus)
						{
							if (show2dsprite[j>>3]&pow2char[j&7])
								if (totalclock & 8) col += (2<<2);
						}
					}

					xp1 = mulscalen<14>(sprite[j].x-posxe,zoome);
					yp1 = mulscalen<14>(sprite[j].y-posye,zoome);
					if (((halfxdim16+xp1) >= 2) && ((halfxdim16+xp1) <= xdim-3))
						if (((midydim16+yp1) >= 2) && ((midydim16+yp1) <= ydim16-3))
						{
							templong = ((midydim16+yp1)*bytesperline)+(halfxdim16+xp1)+frameplace;
							drawpixel((void *)(templong-1-(bytesperline<<1)), col);
							drawpixel((void *)(templong+0-(bytesperline<<1)), col);
							drawpixel((void *)(templong+1-(bytesperline<<1)), col);

							drawpixel((void *)(templong-1+(bytesperline<<1)), col);
							drawpixel((void *)(templong+0+(bytesperline<<1)), col);
							drawpixel((void *)(templong+1+(bytesperline<<1)), col);

							drawpixel((void *)(templong-2-bytesperline), col);
							drawpixel((void *)(templong-2+0), col);
							drawpixel((void *)(templong-2+bytesperline), col);

							drawpixel((void *)(templong+2-bytesperline), col);
							drawpixel((void *)(templong+2+0), col);
							drawpixel((void *)(templong+2+bytesperline), col);

							drawpixel((void *)(templong+1+bytesperline), col);
							drawpixel((void *)(templong-1+bytesperline), col);
							drawpixel((void *)(templong+1-bytesperline), col);
							drawpixel((void *)(templong-1-bytesperline), col);

							if (showspriteextents && (sprite[j].cstat&(32|16))) {
								int np;
								int p;
								int xoff;
								int yoff;
								int dax;
								int day;
								std::array<int, 4> rxi;
								std::array<int, 4> ryi;
								std::array<int, 4> clipx{0};
								std::array<int, 4> clipy{0};

								const int tilenum = sprite[j].picnum;
								const int ang = sprite[j].ang;
								const int xrepeat = sprite[j].xrepeat;
								const int yrepeat = sprite[j].yrepeat;

								xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)sprite[j].xoffset);
								yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)sprite[j].yoffset);
								if ((sprite[j].cstat&4) > 0) xoff = -xoff;
								if ((sprite[j].cstat&8) > 0) yoff = -yoff;

								if (sprite[j].cstat & 32) {
									// Floor sprite
									const int cosang = sintable[(ang+512)&2047];
									const int sinang = sintable[(ang)&2047];

									dax = ((tilesizx[tilenum]>>1)+xoff)*xrepeat;
									day = ((tilesizy[tilenum]>>1)+yoff)*yrepeat;

									rxi[0] = dmulscalen<16>(sinang,dax,cosang,day);
									ryi[0] = dmulscalen<16>(sinang,day,-cosang,dax);
									rxi[1] = rxi[0] - mulscalen<16>(sinang,tilesizx[tilenum]*xrepeat);
									ryi[1] = ryi[0] + mulscalen<16>(cosang,tilesizx[tilenum]*xrepeat);
									rxi[2] = rxi[1] - mulscalen<16>(cosang,tilesizy[tilenum]*yrepeat);
									rxi[3] = rxi[0] - mulscalen<16>(cosang,tilesizy[tilenum]*yrepeat);
									ryi[2] = ryi[1] - mulscalen<16>(sinang,tilesizy[tilenum]*yrepeat);
									ryi[3] = ryi[0] - mulscalen<16>(sinang,tilesizy[tilenum]*yrepeat);
									np = 4;
								} else {
									// Wall sprite
									dax = sintable[ang&2047]*xrepeat;
									day = sintable[(ang+1536)&2047]*xrepeat;
									rxi[2] = rxi[1] = -mulscalen<16>(dax,(tilesizx[tilenum]>>1)+xoff);
									rxi[3] = rxi[0] = rxi[1] + mulscalen<16>(dax,tilesizx[tilenum]);
									ryi[2] = ryi[1] = -mulscalen<16>(day,(tilesizx[tilenum]>>1)+xoff);
									ryi[3] = ryi[0] = ryi[1] + mulscalen<16>(day,tilesizx[tilenum]);
									np = 1;
								}

								if (showspriteextents >= 2) {
									// Apply clipping boundary.
									dax = mulscalen<14>(sintable[(ang-256+512)&2047],kenswalldist);
									day = mulscalen<14>(sintable[(ang-256)&2047],kenswalldist);
									clipx[1] = rxi[1]-day; clipy[1] = ryi[1]+dax;
									clipx[0] = rxi[0]+dax; clipy[0] = ryi[0]+day;
									clipx[3] = rxi[3]+day; clipy[3] = ryi[3]-dax;
									clipx[2] = rxi[2]-dax; clipy[2] = ryi[2]-day;
								}

								// Correct for zoom.
								std::ranges::transform(rxi, rxi.begin(), [zoome](auto rxip) { return mulscalen<14>(rxip, zoome); });
								std::ranges::transform(ryi, ryi.begin(), [zoome](auto ryip) { return mulscalen<14>(ryip, zoome); });
								std::ranges::transform(clipx, clipx.begin(), [zoome](auto clipxp) { return mulscalen<14>(clipxp, zoome); });
								std::ranges::transform(clipy, clipy.begin(), [zoome](auto clipyp) { return mulscalen<14>(clipyp, zoome); });

								drawlinepat = 0x99999999;
								for (p=0;p<np;p++) {
									drawline16(halfxdim16 + xp1 + rxi[p], midydim16 + yp1 + ryi[p],
										halfxdim16 + xp1 + rxi[(p+1)&3], midydim16 + yp1 + ryi[(p+1)&3],
										col);
								}
								if (showspriteextents >= 2) {
									drawlinepat = 0x11111111;
									for (p=0;p<4;p++) {
										drawline16(halfxdim16 + xp1 + clipx[p], midydim16 + yp1 + clipy[p],
											halfxdim16 + xp1 + clipx[(p+1)&3], midydim16 + yp1 + clipy[(p+1)&3],
											col);
									}
								}
								drawlinepat = 0xffffffff;
							}

							xp2 = mulscalen<11>(sintable[(sprite[j].ang+2560)&2047],zoome) / 768;
							yp2 = mulscalen<11>(sintable[(sprite[j].ang+2048)&2047],zoome) / 768;

							if ((sprite[j].cstat&256) > 0)
							{
								if (((sprite[j].ang+256)&512) == 0)
								{
									drawline16(halfxdim16+xp1,midydim16+yp1-1,halfxdim16+xp1+xp2,midydim16+yp1+yp2-1,col);
									drawline16(halfxdim16+xp1,midydim16+yp1+1,halfxdim16+xp1+xp2,midydim16+yp1+yp2+1,col);
								}
								else
								{
									drawline16(halfxdim16+xp1-1,midydim16+yp1,halfxdim16+xp1+xp2-1,midydim16+yp1+yp2,col);
									drawline16(halfxdim16+xp1+1,midydim16+yp1,halfxdim16+xp1+xp2+1,midydim16+yp1+yp2,col);
								}
								col += 8;
							}
							drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16+xp1+xp2,midydim16+yp1+yp2,col);
						}
				}

	faketimerhandler();
	xp1 = mulscalen<11>(sintable[(ange+2560)&2047],zoome) / 768; //Draw white arrow
	yp1 = mulscalen<11>(sintable[(ange+2048)&2047],zoome) / 768;
	drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16-xp1,midydim16-yp1,15);
	drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16+yp1,midydim16-xp1,15);
	drawline16(halfxdim16+xp1,midydim16+yp1,halfxdim16-yp1,midydim16+xp1,15);
}


//
// printext16
//
void printext16(int xpos, int ypos, short col, short backcol, std::string_view name, char fontsize)
{
	const auto* f = &textfonts[std::min(static_cast<int>(fontsize), 2)]; // FIXME: Dumb way to index here.
	int stx{xpos};

	for(auto ch : name)
	{
		const auto* letptr = &f->font[((int)(unsigned char) ch) * f->cellh + f->cellyoff];
		auto* ptr = (unsigned char *)(bytesperline * (ytop16 + ypos + f->charysiz - 1) + stx + frameplace);
		for(int y = f->charysiz - 1; y >= 0; --y)
		{
			for(int x = f->charxsiz - 1; x >= 0; --x)
			{
				if (letptr[y]&pow2char[7-x-f->cellxoff])
					ptr[x] = (unsigned char)col;
				else if (backcol >= 0)
					ptr[x] = (unsigned char)backcol;
			}
			ptr -= bytesperline;
		}
		stx += f->charxsiz;
	}
}

void printcoords16(int posxe, int posye, short ange)
{
	setstatusbarviewport();

	auto currpos = fmt::format("x={} y={} ang={}", posxe, posye, ange);
	printext16(8, 128, 11, 6, currpos, 0);

	int maxsect{0};
	int maxwall{0};
	int maxspri{0};

	switch (mapversion) {
		case 5:
			maxsect = MAXSECTORSV5;
			maxwall = MAXWALLSV5;
			maxspri = MAXSPRITESV5;
			break;
		case 6:
			maxsect = MAXSECTORSV6;
			maxwall = MAXWALLSV6;
			maxspri = MAXSPRITESV6;
			break;
		case 7:
			maxsect = MAXSECTORSV7;
			maxwall = MAXWALLSV7;
			maxspri = MAXSPRITESV7;
			break;
		case 8:
			maxsect = MAXSECTORSV8;
			maxwall = MAXWALLSV8;
			maxspri = MAXSPRITESV8;
			break;
	}

	auto mapvals = fmt::format("v{} {}/{} sect {}/{} wall {}/{} spri",
					mapversion,
					numsectors, maxsect,
					numwalls,   maxwall,
					numsprites, maxspri);

	printext16(264, 128, 14, 6, mapvals, 0);

	restoreviewport();
}

void updatenumsprites()
{
	numsprites = 0;

	for(const auto& aSprite : sprite) {
		if(aSprite.statnum < MAXSTATUS)
			++numsprites;
	}
}

void copysector(short soursector, short destsector, short deststartwall, bool copystat)
{
	short newnumwalls = deststartwall;  //erase existing sector fragments

		//duplicate walls
	const short startwall = g_sector[soursector].wallptr;
	const short endwall = startwall + g_sector[soursector].wallnum;

	for(short j{startwall}; j < endwall; ++j)
	{
		std::memcpy(&wall[newnumwalls], &wall[j], sizeof(walltype));
		wall[newnumwalls].point2 += deststartwall - startwall;
		
		if (wall[newnumwalls].nextwall >= 0)
		{
			wall[newnumwalls].nextwall += deststartwall-startwall;
			wall[newnumwalls].nextsector += destsector-soursector;
		}

		newnumwalls++;
	}

	//for(j=deststartwall;j<newnumwalls;j++)
	//{
	//   if (wall[j].nextwall >= 0)
	//      checksectorpointer(wall[j].nextwall,wall[j].nextsector);
	//   checksectorpointer((short)j,destsector);
	//}

	if (newnumwalls > deststartwall)
	{
			//duplicate sectors
		std::memcpy(&g_sector[destsector],&g_sector[soursector],sizeof(sectortype));
		g_sector[destsector].wallptr = deststartwall;
		g_sector[destsector].wallnum = newnumwalls-deststartwall;

		if (copystat)
		{
				//duplicate sprites
			short j = headspritesect[soursector];
			while (j >= 0)
			{
				const short k = nextspritesect[j];

				const short m = insertsprite(destsector,sprite[j].statnum);
				std::memcpy(&sprite[m],&sprite[j],sizeof(spritetype));
				sprite[m].sectnum = destsector;   //Don't let memcpy overwrite sector!

				j = k;
			}
		}

	}
}

void showsectordata(short sectnum)
{
	std::array<char, 80> snotbuf;

	setstatusbarviewport();

	fmt::format_to(&snotbuf[0],"Sector {}",sectnum);
	printext16(8,32,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Firstwall: {}",g_sector[sectnum].wallptr);
	printext16(8,48,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Numberofwalls: {}",g_sector[sectnum].wallnum);
	printext16(8,56,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Firstsprite: {}",headspritesect[sectnum]);
	printext16(8,64,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Tags: {}, {}",g_sector[sectnum].hitag,g_sector[sectnum].lotag);
	printext16(8,72,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"     (0x{}), (0x{})",g_sector[sectnum].hitag,g_sector[sectnum].lotag);
	printext16(8,80,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Extra: {}",g_sector[sectnum].extra);
	printext16(8,88,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Visibility: {}",g_sector[sectnum].visibility);
	printext16(8,96,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Pixel height: {}",(g_sector[sectnum].floorz-g_sector[sectnum].ceilingz)>>8);
	printext16(8,104,11,-1,&snotbuf[0],0);

	printext16(200,32,11,-1,"CEILINGS:",0);
	fmt::format_to(&snotbuf[0],"Flags (hex): {}",g_sector[sectnum].ceilingstat);
	printext16(200,48,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"(X,Y)pan: {}, {}",g_sector[sectnum].ceilingxpanning,g_sector[sectnum].ceilingypanning);
	printext16(200,56,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Shade byte: {}",g_sector[sectnum].ceilingshade);
	printext16(200,64,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Z-coordinate: {}",g_sector[sectnum].ceilingz);
	printext16(200,72,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Tile number: {}",g_sector[sectnum].ceilingpicnum);
	printext16(200,80,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Ceiling heinum: {}",g_sector[sectnum].ceilingheinum);
	printext16(200,88,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Palookup number: {}",g_sector[sectnum].ceilingpal);
	printext16(200,96,11,-1,&snotbuf[0],0);

	printext16(400,32,11,-1,"FLOORS:",0);
	fmt::format_to(&snotbuf[0],"Flags (hex): {}",g_sector[sectnum].floorstat);
	printext16(400,48,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"(X,Y)pan: {}, {}",g_sector[sectnum].floorxpanning,g_sector[sectnum].floorypanning);
	printext16(400,56,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Shade byte: {}",g_sector[sectnum].floorshade);
	printext16(400,64,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Z-coordinate: {}",g_sector[sectnum].floorz);
	printext16(400,72,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Tile number: {}",g_sector[sectnum].floorpicnum);
	printext16(400,80,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Floor heinum: {}",g_sector[sectnum].floorheinum);
	printext16(400,88,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Palookup number: {}",g_sector[sectnum].floorpal);
	printext16(400,96,11,-1,&snotbuf[0],0);

	restoreviewport();
}

void showwalldata(short wallnum)
{
	setstatusbarviewport();

	std::array<char, 80> snotbuf{};

	fmt::format_to(&snotbuf[0],"Wall {}",wallnum);
	printext16(8,32,11,-1, &snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"X-coordinate: {}",wall[wallnum].pt.x);
	printext16(8,48,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Y-coordinate: {}",wall[wallnum].pt.y);
	printext16(8,56,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Point2: {}",wall[wallnum].point2);
	printext16(8,64,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Sector: {}",sectorofwall(wallnum));
	printext16(8,72,11,-1,&snotbuf[0],0);

	fmt::format_to(&snotbuf[0],"Tags: {}, {}",wall[wallnum].hitag,wall[wallnum].lotag);
	printext16(8,88,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"     (0x{}), (0x{})",wall[wallnum].hitag,wall[wallnum].lotag);
	printext16(8,96,11,-1,&snotbuf[0],0);

	printext16(200,32,11,-1,names[wall[wallnum].picnum],0);
	fmt::format_to(&snotbuf[0],"Flags (hex): {}",wall[wallnum].cstat);
	printext16(200,48,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Shade: {}",wall[wallnum].shade);
	printext16(200,56,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Pal: {}",wall[wallnum].pal);
	printext16(200,64,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"(X,Y)repeat: {}, {}",wall[wallnum].xrepeat,wall[wallnum].yrepeat);
	printext16(200,72,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"(X,Y)pan: {}, {}",wall[wallnum].xpanning,wall[wallnum].ypanning);
	printext16(200,80,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"Tile number: {}",wall[wallnum].picnum);
	printext16(200,88,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"OverTile number: {}",wall[wallnum].overpicnum);
	printext16(200,96,11,-1,&snotbuf[0],0);

	fmt::format_to(&snotbuf[0],"nextsector: {}",wall[wallnum].nextsector);
	printext16(400,48,11,-1,&snotbuf[0],0);
	fmt::format_to(&snotbuf[0],"nextwall: {}",wall[wallnum].nextwall);
	printext16(400,56,11,-1,&snotbuf[0],0);

	fmt::format_to(&snotbuf[0],"Extra: {}",wall[wallnum].extra);
	printext16(400,72,11,-1,&snotbuf[0],0);

	int dax = wall[wallnum].pt.x-wall[wall[wallnum].point2].pt.x;
	const int day = wall[wallnum].pt.y-wall[wall[wallnum].point2].pt.y;
	const int dist = static_cast<int>(std::hypot(dax, day));
	fmt::format_to(&snotbuf[0],"Wall length: {}",dist>>4);
	printext16(400,96,11,-1,&snotbuf[0],0);

	dax = (int)sectorofwall(wallnum);
	fmt::format_to(&snotbuf[0],"Pixel height: {}",(g_sector[dax].floorz-g_sector[dax].ceilingz)>>8);
	printext16(400,104,11,-1,&snotbuf[0],0);

	restoreviewport();
}

void showspritedata(short spritenum)
{
	setstatusbarviewport();

	std::array<char, 80> snotbuf{};
	fmt::format_to(&snotbuf[0],"Sprite {}",spritenum);
	printext16(8,32,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"X-coordinate: {}",sprite[spritenum].x);
	printext16(8,48,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Y-coordinate: {}",sprite[spritenum].y);
	printext16(8,56,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Z-coordinate: {}",sprite[spritenum].z);
	printext16(8,64,11,-1, &snotbuf[0], 0);

	fmt::format_to(&snotbuf[0],"Sectnum: {}",sprite[spritenum].sectnum);
	printext16(8,72,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Statnum: {}",sprite[spritenum].statnum);
	printext16(8,80,11,-1, &snotbuf[0], 0);

	fmt::format_to(&snotbuf[0],"Tags: {}, {}",sprite[spritenum].hitag,sprite[spritenum].lotag);
	printext16(8,96,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"     (0x{}), (0x{})",sprite[spritenum].hitag,sprite[spritenum].lotag);
	printext16(8,104,11,-1, &snotbuf[0], 0);

	printext16(200,32,11,-1,names[sprite[spritenum].picnum],0);
	fmt::format_to(&snotbuf[0],"Flags (hex): {}",sprite[spritenum].cstat);
	printext16(200,48,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Shade: {}",sprite[spritenum].shade);
	printext16(200,56,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Pal: {}",sprite[spritenum].pal);
	printext16(200,64,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"(X,Y)repeat: {}, {}",sprite[spritenum].xrepeat,sprite[spritenum].yrepeat);
	printext16(200,72,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"(X,Y)offset: {}, {}",sprite[spritenum].xoffset,sprite[spritenum].yoffset);
	printext16(200,80,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Tile number: {}",sprite[spritenum].picnum);
	printext16(200,88,11,-1, &snotbuf[0], 0);

	fmt::format_to(&snotbuf[0],"Angle (2048 degrees): {}",sprite[spritenum].ang);
	printext16(400,48,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"X-Velocity: {}",sprite[spritenum].xvel);
	printext16(400,56,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Y-Velocity: {}",sprite[spritenum].yvel);
	printext16(400,64,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Z-Velocity: {}",sprite[spritenum].zvel);
	printext16(400,72,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Owner: {}",sprite[spritenum].owner);
	printext16(400,80,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Clipdist: {}",sprite[spritenum].clipdist);
	printext16(400,88,11,-1, &snotbuf[0], 0);
	fmt::format_to(&snotbuf[0],"Extra: {}",sprite[spritenum].extra);
	printext16(400,96,11,-1, &snotbuf[0], 0);

	restoreviewport();
}

void keytimerstuff()
{
	static int ltotalclock{0};

	if (totalclock == ltotalclock)
		return;

	ltotalclock=totalclock;

	if (keystatus[buildkeys[5]] == 0)
	{
		if (keystatus[buildkeys[2]] > 0)
			angvel = std::max(angvel - 16, -128);

		if (keystatus[buildkeys[3]] > 0)
			angvel = std::min(angvel + 16,  127);
	}
	else
	{
		if (keystatus[buildkeys[2]] > 0)
			svel = std::min(svel + 8,  127);

		if (keystatus[buildkeys[3]] > 0)
			svel = std::max(svel - 8, -128);
	}

	if (keystatus[buildkeys[0]] > 0)
		vel = std::min(vel + 8,  127);

	if (keystatus[buildkeys[1]] > 0)
		vel = std::max(vel - 8, -128);

	if (keystatus[buildkeys[12]] > 0)
		svel = std::min(svel + 8,  127);

	if (keystatus[buildkeys[13]] > 0)
		svel = std::max(svel - 8, -128);

	if (angvel < 0)
		angvel = std::min(angvel + 12, 0);

	if (angvel > 0)
		angvel = std::max(angvel - 12, 0);

	if (svel < 0)
		svel = std::min(svel + 2, 0);

	if (svel > 0)
		svel = std::max(svel - 2, 0);

	if (vel < 0)
		vel = std::min(vel + 2, 0);

	if (vel > 0)
		vel = std::max(vel - 2, 0);
}

void printmessage16(const char* name)
{
	std::array<char, 60> snotbuf;
	int i{0};

	while ((name[i] != 0) && (i < 54))
	{
		snotbuf[i] = name[i];
		i++;
	}

	while (i < 54)
	{
		snotbuf[i] = 32;
		i++;
	}
	
	snotbuf[54] = 0;

	setstatusbarviewport();
	printext16(200L, 8L, 0, 6, &snotbuf[0], 0);
	restoreviewport();
}

void printmessage256(const char *name)
{
	std::array<char, 40> snotbuf;
    int i{0};

	while ((name[i] != 0) && (i < 38))
	{
		snotbuf[i] = name[i];
		i++;
	}

	while (i < 38)
	{
		snotbuf[i] = 32;
		i++;
	}
	
	snotbuf[38] = 0;

	printext256(0L, 0L, whitecol, blackcol, &snotbuf[0], 0);
}

	//Find closest point (*dax, *day) on wall (dawall) to (x, y)
void getclosestpointonwall(int x, int y, int dawall, int *nx, int *ny)
{
    if (dawall < 0) {
		*nx = 0;
		*ny = 0;
		return;
	}

	walltype* wal = &wall[dawall];
	const int dx = wall[wal->point2].pt.x-wal->pt.x;
	const int dy = wall[wal->point2].pt.y-wal->pt.y;
	int i = dx*(x-wal->pt.x) + dy*(y-wal->pt.y);

	if (i <= 0) {
		*nx = wal->pt.x;
		*ny = wal->pt.y;
		return;
	}

	const int j = dx * dx + dy * dy;

	if (i >= j) {
		*nx = wal->pt.x + dx;
		*ny = wal->pt.y + dy;
		return;
	}

	i = divscalen<30>(i, j);
	*nx = wal->pt.x + mulscalen<30>(dx, i);
	*ny = wal->pt.y + mulscalen<30>(dy, i);
}

void initcrc()
{
	for (int j{0}; j < 256; ++j) // Calculate CRC table
	{
		int k = j << 8;
		int a{0};

		for(int i{7}; i >= 0; --i)
		{
			if (((k ^ a) & 0x8000) > 0)
				a = ((a << 1) & 65535) ^ 0x1021;   //0x1021 = genpoly
			else
				a = ((a << 1) & 65535);
			k = ((k << 1) & 65535);
		}

		crctable[j] = (a & 65535);
	}
}

int GetWallZPeg(int nWall)
{
	const int nSector = sectorofwall((short)nWall);
	const int nNextSector = wall[nWall].nextsector;
	
	if (nNextSector == -1)
	{
		//1-sided wall
		if (wall[nWall].cstat & 4)
			return g_sector[nSector].floorz;
		else
			return g_sector[nSector].ceilingz;
	}
	else
	{
			//2-sided wall
		if (wall[nWall].cstat & 4)
			return g_sector[nSector].ceilingz;
		else
		{
			if (g_sector[nNextSector].ceilingz > g_sector[nSector].ceilingz)
				return g_sector[nNextSector].ceilingz;   //top step

			if (g_sector[nNextSector].floorz < g_sector[nSector].floorz)
				return g_sector[nNextSector].floorz;   //bottom step
		}
	}

	return 0;
}

void AlignWalls(int nWall0, int z0, int nWall1, int z1, int nTile)
{
		//do the x alignment
	wall[nWall1].cstat &= ~0x0108;    //Set to non-flip
	wall[nWall1].xpanning = (unsigned char)((wall[nWall0].xpanning + (wall[nWall0].xrepeat << 3)) % tilesizx[nTile]);

	z1 = GetWallZPeg(nWall1);

	int n{picsiz[nTile] >> 4};
	
	for(; (1 << n) < tilesizy[nTile]; ++n);

	wall[nWall1].yrepeat = wall[nWall0].yrepeat;
	wall[nWall1].ypanning = (unsigned char)(wall[nWall0].ypanning + (((z1 - z0) * wall[nWall0].yrepeat)>>(n + 3)));
}

void AutoAlignWalls(int nWall0, int ply)
{
	static std::array<bool, 8192> visited{};

	const int nTile = wall[nWall0].picnum;
	int branch{0};

	if (ply == 0)
	{
			//clear visited bits
	    std::ranges::fill(visited, false);
		visited[nWall0] = true;
	}

	int z0 = GetWallZPeg(nWall0);

	int nWall1 = wall[nWall0].point2;

		//loop through walls at this vertex in CCW order
	while (1)
	{
			//break if this wall would connect us in a loop
		if (visited[nWall1])
			break;

		visited[nWall1] = true;

			//break if reached back of left wall
		if (wall[nWall1].nextwall == nWall0)
			break;

		if (wall[nWall1].picnum == nTile)
		{
			const int z1 = GetWallZPeg(nWall1);
			
			bool visible{false};

			const int nNextSector = wall[nWall1].nextsector;

			if (nNextSector < 0)
				visible = true;
			else
			{
					//ignore two sided walls that have no visible face
				const int nSector = wall[wall[nWall1].nextwall].nextsector;

				if (getceilzofslope((short)nSector, wall[nWall1].pt.x, wall[nWall1].pt.y) <
					getceilzofslope((short)nNextSector, wall[nWall1].pt.x, wall[nWall1].pt.y))
					visible = true;

				if (getflorzofslope((short)nSector, wall[nWall1].pt.x, wall[nWall1].pt.y) >
					getflorzofslope((short)nNextSector, wall[nWall1].pt.x, wall[nWall1].pt.y))
					visible = true;
			}

			if (visible)
			{
				branch++;
				AlignWalls(nWall0, z0, nWall1, z1, nTile);

					//if wall was 1-sided, no need to recurse
				if (wall[nWall1].nextwall < 0)
				{
					nWall0 = nWall1;
					z0 = GetWallZPeg(nWall0);
					nWall1 = wall[nWall0].point2;
					branch = 0;
					continue;
				}
				else
					AutoAlignWalls(nWall1, ply + 1);
			}
		}

		if (wall[nWall1].nextwall < 0)
			break;
		
		nWall1 = wall[wall[nWall1].nextwall].point2;
	}
}


/*
 * vim:ts=4:
 */

