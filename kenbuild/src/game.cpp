// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "build.hpp"
#include "names.hpp"
#include "pragmas.hpp"
#include "cache1d.hpp"
#include "game.hpp"
#include "osd.hpp"
#include "mmulti.hpp"
#include "kdmsound.hpp"
#include "string_utils.hpp"
#include "point.hpp"

#include "baselayer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>

constexpr auto TIMERINTSPERSECOND{140}; //280
constexpr auto MOVESPERSECOND{40};
constexpr auto TICSPERFRAME{3};
constexpr auto MOVEFIFOSIZ{256};
constexpr auto EYEHEIGHT = 32 << 8;   //Normally (32<<8), (51<<8) to make mirrors happy

// declared in config.c
int loadsetup(const std::string&);
int writesetup(const std::string&);


/***************************************************************************
	KEN'S TAG DEFINITIONS:      (Please define your own tags for your games)

 g_sector[?].lotag = 0   Normal sector
 g_sector[?].lotag = 1   If you are on a sector with this tag, then all sectors
			with same hi tag as this are operated.  Once.
 g_sector[?].lotag = 2   Same as sector[?].tag = 1 but this is retriggable.
 g_sector[?].lotag = 3   A really stupid sector that really does nothing now.
 g_sector[?].lotag = 4   A sector where you are put closer to the floor
			(such as the slime in DOOM1.DAT)
 g_sector[?].lotag = 5   A really stupid sector that really does nothing now.
 g_sector[?].lotag = 6   A normal door - instead of pressing D, you tag the
			sector with a 6.  The reason I make you edit doors
			this way is so that can program the doors
			yourself.
 g_sector[?].lotag = 7   A door the goes down to open.
 g_sector[?].lotag = 8   A door that opens horizontally in the middle.
 g_sector[?].lotag = 9   A sliding door that opens vertically in the middle.
			-Example of the advantages of not using BSP tree.
 g_sector[?].lotag = 10  A warping sector with floor and walls that shade.
 g_sector[?].lotag = 11  A sector with all walls that do X-panning.
 g_sector[?].lotag = 12  A sector with walls using the dragging function.
 g_sector[?].lotag = 13  A sector with some swinging doors in it.
 g_sector[?].lotag = 14  A revolving door sector.
 g_sector[?].lotag = 15  A subway track.
 g_sector[?].lotag = 16  A true double-sliding door.

	wall[?].lotag = 0   Normal wall
	wall[?].lotag = 1   Y-panning wall
	wall[?].lotag = 2   Switch - If you flip it, then all sectors with same hi
			tag as this are operated.
	wall[?].lotag = 3   Marked wall to detemine starting dir. (sector tag 12)
	wall[?].lotag = 4   Mark on the shorter wall closest to the pivot point
			of a swinging door. (sector tag 13)
	wall[?].lotag = 5   Mark where a subway should stop. (sector tag 15)
	wall[?].lotag = 6   Mark for true double-sliding doors (sector tag 16)
	wall[?].lotag = 7   Water fountain
	wall[?].lotag = 8   Bouncy wall!

 sprite[?].lotag = 0   Normal sprite
 sprite[?].lotag = 1   If you press space bar on an AL, and the AL is tagged
								  with a 1, he will turn evil.
 sprite[?].lotag = 2   When this sprite is operated, a bomb is shot at its
								  position.
 sprite[?].lotag = 3   Rotating sprite.
 sprite[?].lotag = 4   Sprite switch.
 sprite[?].lotag = 5   Basketball hoop score.

KEN'S STATUS DEFINITIONS:  (Please define your own statuses for your games)
 status = 0            Inactive sprite
 status = 1            Active monster sprite
 status = 2            Monster that becomes active only when it sees you
 status = 3            Smoke on the wall for chainguns
 status = 4            Splashing sprites (When you shoot slime)
 status = 5            Explosion!
 status = 6            Travelling bullet
 status = 7            Bomb sprial-out explosion
 status = 8            Player!
 status = 9            EVILALGRAVE shrinking list
 status = 10           EVILAL list
 status = 11           Sprite respawning list
 status = 12           Sprite which does not respawn (Andy's addition)
 status = MAXSTATUS    Non-existent sprite (this will be true for your
			code also)
**************************************************************************/

struct input
{
	signed char fvel, svel, avel;
	short bits;
};

static int screentilt = 0, oscreentilt = 0;


static int fvel, svel, avel;
static int fvel2, svel2, avel2;

int ydimgame = 480, bppgame = 8;
int forcesetup = 1;

static int digihz[8] = {6000,8000,11025,16000,22050,32000,44100,48000};

static unsigned char frame2draw[MAXPLAYERS];
static int frameskipcnt[MAXPLAYERS];

constexpr auto LAVASIZ{128};
constexpr auto LAVALOGSIZ{7};
constexpr auto LAVAMAXDROPS{32};
static unsigned char lavabakpic[(LAVASIZ+4)*(LAVASIZ+4)], lavainc[LAVASIZ];
static int lavanumdrops, lavanumframes;
static int lavadropx[LAVAMAXDROPS], lavadropy[LAVAMAXDROPS];
static int lavadropsiz[LAVAMAXDROPS], lavadropsizlookup[LAVAMAXDROPS];
static int lavaradx[24][96], lavarady[24][96], lavaradcnt[32];

	//Shared player variables
static int posx[MAXPLAYERS], posy[MAXPLAYERS], posz[MAXPLAYERS];
static int horiz[MAXPLAYERS], zoom[MAXPLAYERS], hvel[MAXPLAYERS];
static short ang[MAXPLAYERS], cursectnum[MAXPLAYERS], ocursectnum[MAXPLAYERS];
static short playersprite[MAXPLAYERS], deaths[MAXPLAYERS];
static int lastchaingun[MAXPLAYERS];
static int health[MAXPLAYERS], flytime[MAXPLAYERS];
static short oflags[MAXPLAYERS];
static short numbombs[MAXPLAYERS];
static short numgrabbers[MAXPLAYERS];   // Andy did this
static short nummissiles[MAXPLAYERS];   // Andy did this
static unsigned char dimensionmode[MAXPLAYERS];
static unsigned char revolvedoorstat[MAXPLAYERS];
static short revolvedoorang[MAXPLAYERS], revolvedoorrotang[MAXPLAYERS];
static int revolvedoorx[MAXPLAYERS], revolvedoory[MAXPLAYERS];

static int nummoves;
// Bug: NUMSTATS used to be equal to the greatest tag number,
// so that the last statrate[] entry was random memory junk
// because stats 0-NUMSTATS required NUMSTATS+1 bytes.   -Andy
constexpr auto NUMSTATS{13};
static signed char statrate[NUMSTATS] = {-1,0,-1,0,0,0,1,3,0,3,15,-1,-1};

	//Input structures
static int locselectedgun, locselectedgun2;
static input loc, oloc, loc2;
static std::array<input, MAXPLAYERS> ffsync;
static std::array<input, MAXPLAYERS> osync;
static std::array<input, MAXPLAYERS> ssync;
	//Input faketimerhandler -> movethings fifo
static int movefifoplc, movefifoend[MAXPLAYERS];
static input baksync[MOVEFIFOSIZ][MAXPLAYERS];
	//Game recording variables
static int reccnt, recstat = 1;
static input recsync[16384][2];

//static int myminlag[MAXPLAYERS], mymaxlag, otherminlag, bufferjitter = 1;
static signed char otherlag[MAXPLAYERS] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static int averagelag[MAXPLAYERS] = {512,512,512,512,512,512,512,512,512,512,512,512,512,512,512,512};

static int fakemovefifoplc;
static int myx, myy, myz, omyx, omyy, omyz, myzvel;
static int myhoriz, omyhoriz;
static short myang, omyang, mycursectnum;
static int myxbak[MOVEFIFOSIZ], myybak[MOVEFIFOSIZ], myzbak[MOVEFIFOSIZ];
static int myhorizbak[MOVEFIFOSIZ];
static short myangbak[MOVEFIFOSIZ];

	//GAME.C sync state variables
static unsigned char syncstat, syncval[MOVEFIFOSIZ], othersyncval[MOVEFIFOSIZ];
static int syncvaltottail, syncvalhead, othersyncvalhead, syncvaltail;

static unsigned char detailmode = 0, ready2send = 0;
static int ototalclock = 0, gotlastpacketclock = 0, smoothratio;
static int oposx[MAXPLAYERS], oposy[MAXPLAYERS], oposz[MAXPLAYERS];
static int ohoriz[MAXPLAYERS], ozoom[MAXPLAYERS];
static short oang[MAXPLAYERS];

static point3di osprite[MAXSPRITES];

constexpr auto MAXINTERPOLATIONS{1024};
static int numinterpolations = 0, startofdynamicinterpolations = 0;
static int oldipos[MAXINTERPOLATIONS];
static int bakipos[MAXINTERPOLATIONS];
static int *curipos[MAXINTERPOLATIONS];

static unsigned char playerreadyflag[MAXPLAYERS];

	//Miscellaneous variables
static unsigned char packbuf[MAXXDIM];
static char tempbuf[MAXXDIM];
static char boardfilename[BMAX_PATH];
static short tempshort[MAXSECTORS];
static short screenpeek = 0, oldmousebstatus = 0;
static short screensize, screensizeflag = 0;
static short neartagsector, neartagwall, neartagsprite;
static int lockclock, neartagdist, neartaghitdist;
static int globhiz, globloz, globhihit, globlohit;

	//Over the shoulder mode variables
static int cameradist = -1, cameraang = 0, cameraclock = 0;

	//Board animation variables
constexpr auto MAXMIRRORS{64};
static short mirrorwall[MAXMIRRORS], mirrorsector[MAXMIRRORS], mirrorcnt;
static short floormirrorsector[64], floormirrorcnt;
static short turnspritelist[16], turnspritecnt;
static short warpsectorlist[64], warpsectorcnt;
static short xpanningsectorlist[16], xpanningsectorcnt;
static short ypanningwalllist[64], ypanningwallcnt;
static short floorpanninglist[64], floorpanningcnt;
static short dragsectorlist[16], dragxdir[16], dragydir[16], dragsectorcnt;
static int dragx1[16], dragy1[16], dragx2[16], dragy2[16], dragfloorz[16];
static short swingcnt, swingwall[32][5], swingsector[32];
static short swingangopen[32], swingangclosed[32], swingangopendir[32];
static short swingang[32], swinganginc[32];
static int swingx[32][8], swingy[32][8];
static short revolvesector[4], revolveang[4], revolvecnt;
static int revolvex[4][16], revolvey[4][16];
static int revolvepivotx[4], revolvepivoty[4];
static short subwaytracksector[4][128], subwaynumsectors[4], subwaytrackcnt;
static int subwaystop[4][8], subwaystopcnt[4];
static int subwaytrackx1[4], subwaytracky1[4];
static int subwaytrackx2[4], subwaytracky2[4];
static int subwayx[4], subwaygoalstop[4], subwayvel[4], subwaypausetime[4];
static short waterfountainwall[MAXPLAYERS], waterfountaincnt[MAXPLAYERS];
static short slimesoundcnt[MAXPLAYERS];

	//Variables that let you type messages to other player
static char getmessage[162];
static int getmessageleng, getmessagetimeoff;
static unsigned char typemessage[162];
static int typemessageleng = 0, typemode = 0;
/*
static unsigned char scantoasc[128] =
{
	0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
	'q','w','e','r','t','y','u','i','o','p','[',']',0,0,'a','s',
	'd','f','g','h','j','k','l',';',39,'`',0,92,'z','x','c','v',
	'b','n','m',',','.','/',0,'*',0,32,0,0,0,0,0,0,
	0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1',
	'2','3','0','.',0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static unsigned char scantoascwithshift[128] =
{
	0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
	'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,'A','S',
	'D','F','G','H','J','K','L',':',34,'~',0,'|','Z','X','C','V',
	'B','N','M','<','>','?',0,'*',0,32,0,0,0,0,0,0,
	0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1',
	'2','3','0','.',0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
*/

	//These variables are for animating x, y, or z-coordinates of sectors,
	//walls, or sprites (They are NOT to be used for changing the [].picnum's)
	//See the setanimation(), and getanimategoal() functions for more details.
constexpr auto MAXANIMATES{512};
static int *animateptr[MAXANIMATES], animategoal[MAXANIMATES];
static int animatevel[MAXANIMATES], animateacc[MAXANIMATES], animatecnt = 0;

#if USE_POLYMOST && USE_OPENGL
	//These parameters are in exact order of sprite structure in BUILD.H
#define spawnsprite(newspriteindex2,x2,y2,z2,cstat2,shade2,pal2,       \
		clipdist2,xrepeat2,yrepeat2,xoffset2,yoffset2,picnum2,ang2,      \
		xvel2,yvel2,zvel2,owner2,sectnum2,statnum2,lotag2,hitag2,extra2) \
{                                                                      \
	spritetype *spr2;                                                   \
	newspriteindex2 = insertsprite(sectnum2,statnum2);                  \
	spr2 = &sprite[newspriteindex2];                                    \
	spr2->x = x2; spr2->y = y2; spr2->z = z2;                           \
	spr2->cstat = cstat2; spr2->shade = shade2;                         \
	spr2->pal = pal2; spr2->clipdist = clipdist2;                       \
	spr2->xrepeat = xrepeat2; spr2->yrepeat = yrepeat2;                 \
	spr2->xoffset = xoffset2; spr2->yoffset = yoffset2;                 \
	spr2->picnum = picnum2; spr2->ang = ang2;                           \
	spr2->xvel = xvel2; spr2->yvel = yvel2; spr2->zvel = zvel2;         \
	spr2->owner = owner2;                                               \
	spr2->lotag = lotag2; spr2->hitag = hitag2; spr2->extra = extra2;   \
	copybuf(&spr2->x,&osprite[newspriteindex2].x,3);                    \
	show2dsprite[newspriteindex2>>3] &= ~(1<<(newspriteindex2&7));      \
	if (show2dsector[sectnum2>>3]&(1<<(sectnum2&7)))                    \
		show2dsprite[newspriteindex2>>3] |= (1<<(newspriteindex2&7));    \
	clearbufbyte(&spriteext[newspriteindex2], sizeof(spriteexttype), 0);   \
}
#else
#define spawnsprite(newspriteindex2,x2,y2,z2,cstat2,shade2,pal2,       \
		clipdist2,xrepeat2,yrepeat2,xoffset2,yoffset2,picnum2,ang2,      \
		xvel2,yvel2,zvel2,owner2,sectnum2,statnum2,lotag2,hitag2,extra2) \
{                                                                      \
	spritetype *spr2;                                                   \
	newspriteindex2 = insertsprite(sectnum2,statnum2);                  \
	spr2 = &sprite[newspriteindex2];                                    \
	spr2->x = x2; spr2->y = y2; spr2->z = z2;                           \
	spr2->cstat = cstat2; spr2->shade = shade2;                         \
	spr2->pal = pal2; spr2->clipdist = clipdist2;                       \
	spr2->xrepeat = xrepeat2; spr2->yrepeat = yrepeat2;                 \
	spr2->xoffset = xoffset2; spr2->yoffset = yoffset2;                 \
	spr2->picnum = picnum2; spr2->ang = ang2;                           \
	spr2->xvel = xvel2; spr2->yvel = yvel2; spr2->zvel = zvel2;         \
	spr2->owner = owner2;                                               \
	spr2->lotag = lotag2; spr2->hitag = hitag2; spr2->extra = extra2;   \
	copybuf(&spr2->x,&osprite[newspriteindex2].x,3);                    \
	show2dsprite[newspriteindex2>>3] &= ~(1<<(newspriteindex2&7));      \
	if (show2dsector[sectnum2>>3]&(1<<(sectnum2&7)))                    \
		show2dsprite[newspriteindex2>>3] |= (1<<(newspriteindex2&7));    \
}
#endif

int nextvoxid = 0;

static int osdcmd_restartvid(const osdfuncparm_t *parm)
{
	(void)parm;

	resetvideomode();
	if (setgamemode(fullscreen,xdim,ydim,bpp))
		buildputs("restartvid: Reset failed...\n");

	return OSDCMD_OK;
}

static int osdcmd_vidmode(const osdfuncparm_t *parm)
{
	int newx = xdim;
	int newy = ydim;
	int newbpp = bpp;
	int tmpscr{0};
	int newfullscreen = fullscreen;

	if (parm->parms.size() < 1 || parm->parms.size() > 4) return OSDCMD_SHOWHELP;

	switch (parm->parms.size()) {
		case 1:   // bpp switch
			std::from_chars(parm->parms[0].data(), parm->parms[0].data() + parm->parms[0].size(), newbpp);
			break;
		case 2: // res switch
			std::from_chars(parm->parms[0].data(), parm->parms[0].data() + parm->parms[0].size(), newx);
			std::from_chars(parm->parms[1].data(), parm->parms[1].data() + parm->parms[1].size(), newy);
			break;
		case 3:   // res & bpp switch
		case 4:
		    std::from_chars(parm->parms[0].data(), parm->parms[0].data() + parm->parms[0].size(), newx);
		    std::from_chars(parm->parms[1].data(), parm->parms[1].data() + parm->parms[1].size(), newy);
		    std::from_chars(parm->parms[2].data(), parm->parms[2].data() + parm->parms[2].size(), newbpp);

			if (parm->parms.size() == 4)
				std::from_chars(parm->parms[3].data(), parm->parms[3].data() + parm->parms[3].size(), tmpscr);
				newfullscreen = (tmpscr != 0);
			break;
	}

	if (setgamemode(newfullscreen,newx,newy,newbpp))
		buildputs("vidmode: Mode change failed!\n");
	screensize = xdim+1;
	return OSDCMD_OK;
}

static int osdcmd_map(const osdfuncparm_t *parm) {
    int i;
    char *dot;
    char namebuf[BMAX_PATH+1];

    if (parm->parms.size() != 1) return OSDCMD_SHOWHELP;

    strncpy(namebuf, parm->parms[0].c_str(), BMAX_PATH);
    namebuf[BMAX_PATH] = 0;
    dot = strrchr(namebuf, '.');
    if ((!dot || !IsSameAsNoCase(dot, ".map")) && std::strlen(namebuf) <= BMAX_PATH-4) {
        std::strcat(namebuf, ".map");
    }

    prepareboard(namebuf);

    screenpeek = myconnectindex;
	reccnt = 0;
	for(i=connecthead;i>=0;i=connectpoint2[i]) initplayersprite((short)i);

	waitforeverybody();
	totalclock = ototalclock = 0; gotlastpacketclock = 0; nummoves = 0;

	ready2send = 1;
	drawscreen(screenpeek,65536L);

    return OSDCMD_OK;
}

#if defined RENDERTYPEWIN || (defined RENDERTYPESDL && (defined __APPLE__ || defined HAVE_GTK))
# define HAVE_STARTWIN
#endif

int app_main(int argc, char const * const argv[])
{
	int i;
	int j;
	int k;
	int waitplayers;
	int x1;
	int y1;
	int x2;
	int y2;
	int other, netparm = 0, endnetparm = 0, netsuccess = 0;

#ifdef HAVE_STARTWIN
	int cmdsetup = 0;
    int startretval = STARTWIN_RUN;
    struct startwin_settings settings;
#endif

#if defined(DATADIR)
    {
        const char *datadir = DATADIR;
        if (datadir && datadir[0]) {
            addsearchpath(datadir);
        }
    }
#endif

    {
        std::string supportdir = Bgetsupportdir(1);
        char *appdir = Bgetappdir();
        char dirpath[BMAX_PATH+1];

        // the OSX app bundle, or on Windows the directory where the EXE was launched
        if (appdir) {
            addsearchpath(appdir);
            std::free(appdir);
        }

        // the global support files directory
        if (!supportdir.empty()) {
			// FIXME: Defaults to '/'; should be '\' on windows
			fmt::format_to(&dirpath[0], "{}/KenBuild", supportdir);
            addsearchpath(dirpath);
        }
    }

    // creating a 'user_profiles_disabled' file in the current working
    // directory where the game was launched makes the installation
    // "portable" by writing into the working directory
    if (access("user_profiles_disabled", F_OK) == 0) {
        char cwd[BMAX_PATH+1];
        if (getcwd(cwd, sizeof(cwd))) {
            addsearchpath(cwd);
        }
    } else {
        char dirpath[BMAX_PATH];
        int asperr;
        std::string supportdir = Bgetsupportdir(0); 

        if (!supportdir.empty()) {
#if defined(_WIN32) || defined(__APPLE__)
            static constexpr std::string_view dirname = "KenBuild";
#else
            const char *dirname = ".kenbuild";
#endif
			fmt::format_to(&dirpath[0], "{}/{}", supportdir, dirname);
            asperr = addsearchpath(dirpath);
            if (asperr == -2) {
                if (Bmkdir(dirpath, S_IRWXU) == 0) {
                    asperr = addsearchpath(dirpath);
                } else {
                    buildprintf("warning: could not create directory {}\n", dirpath);
                    asperr = -1;
                }
            }
            if (asperr == 0 && chdir(dirpath) < 0) {
                buildprintf("warning: could not change directory to {}\n", dirpath);
            }
        }
    }

	buildsetlogfile("console.txt");

	OSD_RegisterFunction("restartvid","restartvid: reinitialise the video mode",osdcmd_restartvid);
	OSD_RegisterFunction("vidmode","vidmode [xdim ydim] [bpp] [fullscreen]: immediately change the video mode",osdcmd_vidmode);
	OSD_RegisterFunction("map", "map [filename]: load a map", osdcmd_map);

	wm_setapptitle("KenBuild by Ken Silverman");

	std::strcpy(boardfilename, "nukeland.map");
	for (i=1;i<argc;i++) {
#ifdef _WIN32
		if (argv[i][0] == '-' || argv[i][0] == '/') {
#else
		if (argv[i][0] == '-') {
#endif
			if (IsSameAsNoCase("net", &argv[i][1])) {
				netparm = ++i;
				for (; i<argc; i++)
					if (IsSameAsNoCase(argv[i], "--")) break;
				endnetparm = i;
			}
#ifdef HAVE_STARTWIN
			else if (IsSameAsNoCase(&argv[i][1], "setup")) cmdsetup = 1;
			else if (IsSameAsNoCase(&argv[i][1], "nosetup")) cmdsetup = -1;
#endif
		}
		else {
			std::strcpy(boardfilename, argv[i]);
			if (!std::strrchr(boardfilename,'.')) std::strcat(boardfilename,".map");
		}
	}

	initgroupfile("stuff.dat");
	if (!initengine()) {
		wm_msgbox(nullptr, "There was a problem initialising the engine: %s.\n", engineerrstr.c_str());
		return -1;
	}

	if ((i = loadsetup("game.cfg")) < 0)
		buildputs("Configuration file not found, using defaults.\n");

#ifdef HAVE_STARTWIN
    std::memset(&settings, 0, sizeof(settings));
    settings.fullscreen = fullscreen;
    settings.xdim3d = xdimgame;
    settings.ydim3d = ydimgame;
    settings.bpp3d = bppgame;
    settings.forcesetup = forcesetup;
    settings.netoverride = netparm > 0;

	if (i || (forcesetup && cmdsetup == 0) || (cmdsetup > 0)) {
        if (quitevent) return 0;

        startretval = startwin_run(&settings);
        if (startretval == STARTWIN_CANCEL)
            return 0;
	}

    fullscreen = settings.fullscreen;
    xdimgame = settings.xdim3d;
    ydimgame = settings.ydim3d;
    bppgame = settings.bpp3d;
    forcesetup = settings.forcesetup;
#endif

    writesetup("game.cfg");

	initinput();
	if (option[3] != 0) initmouse();
	inittimer(TIMERINTSPERSECOND, nullptr);

	if (netparm) {
		netsuccess = initmultiplayersparms(endnetparm - netparm, &argv[netparm]);
	}
#ifdef HAVE_STARTWIN
	else if (settings.numplayers > 1) {
		char modeparm[8];
		const char *parmarr[3] = { modeparm, nullptr, nullptr };
		int parmc = 0;

		if (settings.joinhost) {
			std::strcpy(modeparm, "-nm");
			parmarr[1] = settings.joinhost;
			parmc = 2;
		} else if (settings.numplayers > 1 && settings.numplayers <= MAXPLAYERS) {
			std::sprintf(modeparm, "-nm:%d", settings.numplayers);
			parmc = 1;
		}

		if (parmc > 0) {
			netsuccess = initmultiplayersparms(parmc, parmarr);
		}

		if (settings.joinhost) {
			std::free(settings.joinhost);
		}
	}
#endif

    if (netsuccess) {
        buildputs("Waiting for players...\n");
        while (initmultiplayerscycle()) {
            handleevents();
            if (quitevent) {
                musicoff();
                uninitmultiplayers();
                uninitengine();
                uninitsb();
                uninitgroupfile();
                return 0;
            }
        }
    } else {
        initsingleplayers();
    }

	option[4] = (numplayers >= 2);

	pskyoff[0] = 0; pskyoff[1] = 0; pskybits = 1;

	loadpics("tiles000.art", 1048576);                      //Load artwork
	if (!qloadkvx(nextvoxid, "voxel000.kvx"))
		tiletovox[PLAYER] = nextvoxid++;
	if (!qloadkvx(nextvoxid, "voxel001.kvx"))
		tiletovox[BROWNMONSTER] = nextvoxid++;
	if (!loaddefinitionsfile("kenbuild.def")) buildputs("Definitions file loaded.\n");

		//Here's an example of TRUE ornamented walls
		//The allocatepermanenttile should be called right after loadpics
		//Since it resets the tile cache for each call.
	if (allocatepermanenttile(SLIME,128,128) == 0)    //If enough memory
	{
		buildputs("Not enough memory for slime!\n");
		std::exit(0);
	}
	if (allocatepermanenttile(MAXTILES-1,64,64) != 0)    //If enough memory
	{
			//My face with an explosion written over it
		copytilepiece(KENPICTURE,0,0,64,64,MAXTILES-1,0,0);
		copytilepiece(EXPLOSION,0,0,64,64,MAXTILES-1,0,0);
	}

	initlava();

	unsigned char remapbuf[256];
	for(j=0;j<256;j++)
		remapbuf[j] = ((j+32)&255);  //remap colors for screwy palette sectors
	makepalookup(16,remapbuf,0,0,0,1);

	for(j=0;j<256;j++) remapbuf[j] = j;
	makepalookup(17,remapbuf,24,24,24,1);

	for(j=0;j<256;j++) remapbuf[j] = j; //(j&31)+32;
	makepalookup(18,remapbuf,8,8,48,1);

	prepareboard(boardfilename);                   //Load board

	initsb(option[1],option[2],digihz[option[7]>>4],((option[7]&4)>0)+1,((option[7]&2)>0)+1,60,option[7]&1);
	if (IsSameAsNoCase(boardfilename, "klab.map"))
	    loadsong("klabsong.kdm");
	else
		loadsong("neatsong.kdm");
	musicon();

	if (option[4] > 0)
	{
		x1 = ((xdim-screensize)>>1);
		x2 = x1+screensize-1;
		y1 = (((ydim-32)-scale(screensize,ydim-32,xdim))>>1);
		y2 = y1 + scale(screensize,ydim-32,xdim)-1;

		drawtilebackground(0L,0L,BACKGROUND,8,x1,y1,x2,y2,0);

		if (option[4] < 5) waitplayers = 2; else waitplayers = option[4]-3;
		while (numplayers < waitplayers)
		{
			std::sprintf(tempbuf,"%d of %d players in...",numplayers,waitplayers);
			printext256(68L,84L,31,0,tempbuf,0);
			nextpage();

			if (getpacket(&other,packbuf) > 0)
				if (packbuf[0] == 255)
					keystatus[1] = 1;

			if (handleevents()) {
				if (quitevent) {
					keystatus[1] = 1;
					quitevent = false;
				}
			}

			if (keystatus[1])
			{
				musicoff();
				uninitmultiplayers();
				uninitengine();
				uninitsb();
				uninitgroupfile();
				std::exit(0);
			}
		}
		screenpeek = myconnectindex;

		j = 1;
		for(i=connecthead;i>=0;i=connectpoint2[i])
		{
			if (myconnectindex == i) break;
			j++;
		}
		std::sprintf(getmessage,"Player %d",j);
		if (networkmode == 0)
		{
			if (j == 1) std::strcat(getmessage," (Master)");
			else std::strcat(getmessage," (Slave)");
		} else
			std::strcat(getmessage," (Even)");
		getmessageleng = (int)std::strlen(getmessage);
		getmessagetimeoff = totalclock+120;
	}

	screenpeek = myconnectindex;
	reccnt = 0;
	for(i=connecthead;i>=0;i=connectpoint2[i]) initplayersprite((short)i);

	waitforeverybody();
	totalclock = ototalclock = 0; gotlastpacketclock = 0; nummoves = 0;

	ready2send = 1;
	drawscreen(screenpeek,65536L);

	while (!keystatus[1])       //Main loop starts here
	{
		if (handleevents()) {
			if (quitevent) {
				keystatus[1] = 1;
				quitevent = false;
			}
		}

		refreshaudio();
		OSD_DispatchQueued();

			// backslash (useful only with KDM)
//      if (keystatus[0x2b]) { keystatus[0x2b] = 0; preparesndbuf(); }

		if ((networkmode == 1) || (myconnectindex != connecthead))
			while (fakemovefifoplc != movefifoend[myconnectindex]) fakedomovethings();

		getpackets();

		if (typemode == 0)           //if normal game keys active
		{
			if ((keystatus[0x2a]&keystatus[0x36]&keystatus[0x13]) > 0)   //Sh.Sh.R (replay)
			{
				keystatus[0x13] = 0;
				playback();
			}

			if (keystatus[0x26]&(keystatus[0x1d]|keystatus[0x9d])) //Load game
			{
				keystatus[0x26] = 0;
				loadgame();
				drawstatusbar(screenpeek);   // Andy did this
			}

			if (keystatus[0x1f]&(keystatus[0x1d]|keystatus[0x9d])) //Save game
			{
				keystatus[0x1f] = 0;
				savegame();
			}
		}

		if ((networkmode == 0) || (option[4] == 0))
		{
			while (movefifoplc != movefifoend[0]) domovethings();
		}
		else
		{
			j = connecthead;
			if (j == myconnectindex) j = connectpoint2[j];
			averagelag[j] = ((averagelag[j]*7+(((movefifoend[myconnectindex]-movefifoend[j]+otherlag[j]+2)&255)<<8))>>3);
			j = std::max(averagelag[j] >> 9, 1);
			while (((movefifoend[myconnectindex]-movefifoplc)&(MOVEFIFOSIZ-1)) > j)
			{
				for(i=connecthead;i>=0;i=connectpoint2[i])
					if (movefifoplc == movefifoend[i]) break;
				if (i >= 0) break;
				if (myconnectindex != connecthead)
				{
					k = ((movefifoend[myconnectindex]-movefifoend[connecthead]-otherlag[connecthead]+128)&255);
					if (k > 128+1) ototalclock++;
					if (k < 128-1) ototalclock--;
				}
				domovethings();
			}
		}
		i = (totalclock-gotlastpacketclock)*(65536/(TIMERINTSPERSECOND/MOVESPERSECOND));

		drawscreen(screenpeek,i);
	}

	musicoff();
	uninitmultiplayers();
	uninitengine();
	uninitsb();
	uninitgroupfile();

	return(0);
}

void operatesector(short dasector)
{     //Door code
	int i;
	int j;
	int datag;
	int daz;
	int dax2;
	int day2;
	int centx;
	int centy;
	short startwall;
	short endwall;
	short wallfind[2];

	datag = g_sector[dasector].lotag;

	startwall = g_sector[dasector].wallptr;
	endwall = startwall + g_sector[dasector].wallnum;
	centx = 0L, centy = 0L;
	for(i=startwall;i<endwall;i++)
	{
		centx += wall[i].pt.x;
		centy += wall[i].pt.y;
	}
	centx /= (endwall-startwall);
	centy /= (endwall-startwall);

		//Simple door that moves up  (tag 8 is a combination of tags 6 & 7)
	if ((datag == 6) || (datag == 8))    //If the sector in front is a door
	{
		i = getanimationgoal(&g_sector[dasector].ceilingz);
		if (i >= 0)      //If door already moving, reverse its direction
		{
			if (datag == 8)
				daz = ((g_sector[dasector].ceilingz+g_sector[dasector].floorz)>>1);
			else
				daz = g_sector[dasector].floorz;

			if (animategoal[i] == daz)
				animategoal[i] = g_sector[nextsectorneighborz(dasector,g_sector[dasector].floorz,-1,-1)].ceilingz;
			else
				animategoal[i] = daz;
			animatevel[i] = 0;
		}
		else      //else insert the door's ceiling on the animation list
		{
			if (g_sector[dasector].ceilingz == g_sector[dasector].floorz)
				daz = g_sector[nextsectorneighborz(dasector,g_sector[dasector].floorz,-1,-1)].ceilingz;
			else
			{
				if (datag == 8)
					daz = ((g_sector[dasector].ceilingz+g_sector[dasector].floorz)>>1);
				else
					daz = g_sector[dasector].floorz;
			}
			if ((j = setanimation(&g_sector[dasector].ceilingz,daz,6L,6L)) >= 0)
				wsayfollow("updowndr.wav",4096L+(krand()&255)-128,256L,&centx,&centy,0);
		}
	}
		//Simple door that moves down
	if ((datag == 7) || (datag == 8)) //If the sector in front's elevator
	{
		i = getanimationgoal(&g_sector[dasector].floorz);
		if (i >= 0)      //If elevator already moving, reverse its direction
		{
			if (datag == 8)
				daz = ((g_sector[dasector].ceilingz+g_sector[dasector].floorz)>>1);
			else
				daz = g_sector[dasector].ceilingz;

			if (animategoal[i] == daz)
				animategoal[i] = g_sector[nextsectorneighborz(dasector,g_sector[dasector].ceilingz,1,1)].floorz;
			else
				animategoal[i] = daz;
			animatevel[i] = 0;
		}
		else      //else insert the elevator's ceiling on the animation list
		{
			if (g_sector[dasector].floorz == g_sector[dasector].ceilingz)
				daz = g_sector[nextsectorneighborz(dasector,g_sector[dasector].ceilingz,1,1)].floorz;
			else
			{
				if (datag == 8)
					daz = ((g_sector[dasector].ceilingz+g_sector[dasector].floorz)>>1);
				else
					daz = g_sector[dasector].ceilingz;
			}
			if ((j = setanimation(&g_sector[dasector].floorz,daz,6L,6L)) >= 0)
				wsayfollow("updowndr.wav",4096L+(krand()&255)-128,256L,&centx,&centy,0);
		}
	}

	if (datag == 9)   //Smooshy-wall sideways double-door
	{
		//find any points with either same x or same y coordinate
		//  as center (centx, centy) - should be 2 points found.
		wallfind[0] = -1;
		wallfind[1] = -1;
		for(i=startwall;i<endwall;i++)
			if ((wall[i].pt.x == centx) || (wall[i].pt.y == centy))
			{
				if (wallfind[0] == -1)
					wallfind[0] = i;
				else
					wallfind[1] = i;
			}

		for(j=0;j<2;j++)
		{
			if ((wall[wallfind[j]].pt.x == centx) && (wall[wallfind[j]].pt.y == centy))
			{
				//find what direction door should open by averaging the
				//  2 neighboring points of wallfind[0] & wallfind[1].
				i = wallfind[j]-1; if (i < startwall) i = endwall-1;
				dax2 = ((wall[i].pt.x+wall[wall[wallfind[j]].point2].pt.x)>>1)-wall[wallfind[j]].pt.x;
				day2 = ((wall[i].pt.y+wall[wall[wallfind[j]].point2].pt.y)>>1)-wall[wallfind[j]].pt.y;
				if (dax2 != 0)
				{
					dax2 = wall[wall[wall[wallfind[j]].point2].point2].pt.x;
					dax2 -= wall[wall[wallfind[j]].point2].pt.x;
					setanimation(&wall[wallfind[j]].pt.x, wall[wallfind[j]].pt.x + dax2,4L,0L);
					setanimation(&wall[i].pt.x,wall[i].pt.x+dax2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.x, wall[wall[wallfind[j]].point2].pt.x + dax2,4L,0L);
				}
				else if (day2 != 0)
				{
					day2 = wall[wall[wall[wallfind[j]].point2].point2].pt.y;
					day2 -= wall[wall[wallfind[j]].point2].pt.y;
					setanimation(&wall[wallfind[j]].pt.y,wall[wallfind[j]].pt.y+day2,4L,0L);
					setanimation(&wall[i].pt.y,wall[i].pt.y+day2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.y,wall[wall[wallfind[j]].point2].pt.y+day2,4L,0L);
				}
			}
			else
			{
				i = wallfind[j]-1; if (i < startwall) i = endwall-1;
				dax2 = ((wall[i].pt.x+wall[wall[wallfind[j]].point2].pt.x)>>1)-wall[wallfind[j]].pt.x;
				day2 = ((wall[i].pt.y+wall[wall[wallfind[j]].point2].pt.y)>>1)-wall[wallfind[j]].pt.y;
				if (dax2 != 0)
				{
					setanimation(&wall[wallfind[j]].pt.x,centx,4L,0L);
					setanimation(&wall[i].pt.x,centx+dax2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.x,centx+dax2,4L,0L);
				}
				else if (day2 != 0)
				{
					setanimation(&wall[wallfind[j]].pt.y,centy,4L,0L);
					setanimation(&wall[i].pt.y,centy+day2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.y,centy+day2,4L,0L);
				}
			}
		}
		wsayfollow("updowndr.wav",4096L-256L,256L,&centx,&centy,0);
		wsayfollow("updowndr.wav",4096L+256L,256L,&centx,&centy,0);
	}

	if (datag == 13)  //Swinging door
	{
		for(i=0;i<swingcnt;i++)
		{
			if (swingsector[i] == dasector)
			{
				if (swinganginc[i] == 0)
				{
					if (swingang[i] == swingangclosed[i])
					{
						swinganginc[i] = swingangopendir[i];
						wsayfollow("opendoor.wav",4096L+(krand()&511)-256,256L,&centx,&centy,0);
					}
					else
						swinganginc[i] = -swingangopendir[i];
				}
				else
					swinganginc[i] = -swinganginc[i];

				for(j=1;j<=3;j++)
				{
					setinterpolation(&wall[swingwall[i][j]].pt.x);
					setinterpolation(&wall[swingwall[i][j]].pt.y);
				}
			}
		}
	}

	if (datag == 16)  //True sideways double-sliding door
	{
		 //get 2 closest line segments to center (dax, day)
		wallfind[0] = -1;
		wallfind[1] = -1;
		for(i=startwall;i<endwall;i++)
			if (wall[i].lotag == 6)
			{
				if (wallfind[0] == -1)
					wallfind[0] = i;
				else
					wallfind[1] = i;
			}

		for(j=0;j<2;j++)
		{
			if ((((wall[wallfind[j]].pt.x + wall[wall[wallfind[j]].point2].pt.x)>>1) == centx) && (((wall[wallfind[j]].pt.y + wall[wall[wallfind[j]].point2].pt.y)>>1) == centy))
			{     //door was closed
					//find what direction door should open
				i = wallfind[j]-1; if (i < startwall) i = endwall-1;
				dax2 = wall[i].pt.x-wall[wallfind[j]].pt.x;
				day2 = wall[i].pt.y-wall[wallfind[j]].pt.y;
				if (dax2 != 0)
				{
					dax2 = wall[wall[wall[wall[wallfind[j]].point2].point2].point2].pt.x;
					dax2 -= wall[wall[wall[wallfind[j]].point2].point2].pt.x;
					setanimation(&wall[wallfind[j]].pt.x,wall[wallfind[j]].pt.x+dax2,4L,0L);
					setanimation(&wall[i].pt.x,wall[i].pt.x+dax2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.x,wall[wall[wallfind[j]].point2].pt.x+dax2,4L,0L);
					setanimation(&wall[wall[wall[wallfind[j]].point2].point2].pt.x,wall[wall[wall[wallfind[j]].point2].point2].pt.x+dax2,4L,0L);
				}
				else if (day2 != 0)
				{
					day2 = wall[wall[wall[wall[wallfind[j]].point2].point2].point2].pt.y;
					day2 -= wall[wall[wall[wallfind[j]].point2].point2].pt.y;
					setanimation(&wall[wallfind[j]].pt.y,wall[wallfind[j]].pt.y + day2,4L,0L);
					setanimation(&wall[i].pt.y,wall[i].pt.y+day2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.y,wall[wall[wallfind[j]].point2].pt.y+day2,4L,0L);
					setanimation(&wall[wall[wall[wallfind[j]].point2].point2].pt.y,wall[wall[wall[wallfind[j]].point2].point2].pt.y+day2,4L,0L);
				}
			}
			else
			{    //door was not closed
				i = wallfind[j]-1; if (i < startwall) i = endwall-1;
				dax2 = wall[i].pt.x-wall[wallfind[j]].pt.x;
				day2 = wall[i].pt.y-wall[wallfind[j]].pt.y;
				if (dax2 != 0)
				{
					setanimation(&wall[wallfind[j]].pt.x,centx,4L,0L);
					setanimation(&wall[i].pt.x,centx+dax2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.x,centx,4L,0L);
					setanimation(&wall[wall[wall[wallfind[j]].point2].point2].pt.x,centx+dax2,4L,0L);
				}
				else if (day2 != 0)
				{
					setanimation(&wall[wallfind[j]].pt.y,centy,4L,0L);
					setanimation(&wall[i].pt.y,centy+day2,4L,0L);
					setanimation(&wall[wall[wallfind[j]].point2].pt.y,centy,4L,0L);
					setanimation(&wall[wall[wall[wallfind[j]].point2].point2].pt.y,centy+day2,4L,0L);
				}
			}
		}
		wsayfollow("updowndr.wav",4096L-64L,256L,&centx,&centy,0);
		wsayfollow("updowndr.wav",4096L+64L,256L,&centx,&centy,0);
	}
}

void operatesprite(short dasprite)
{
	int datag;

	datag = sprite[dasprite].lotag;

	if (datag == 2)    //A sprite that shoots a bomb
	{
		shootgun(dasprite,
			sprite[dasprite].x,sprite[dasprite].y,sprite[dasprite].z,
			sprite[dasprite].ang,100L,sprite[dasprite].sectnum,2);
	}
}

int changehealth(short snum, short deltahealth)
{
	if (health[snum] > 0)
	{
		health[snum] += deltahealth;
		if (health[snum] > 999) health[snum] = 999;

		if (health[snum] <= 0)
		{
			health[snum] = -1;
			wsayfollow("death.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
			sprite[playersprite[snum]].picnum = SKELETON;
		}

		if ((snum == screenpeek) && (screensize <= xdim))
		{
			if (health[snum] > 0)
				std::sprintf(tempbuf,"Health:%3d",health[snum]);
			else
				std::sprintf(tempbuf,"YOU STINK!");

			printext((xdim>>1)-(int)(std::strlen(tempbuf)<<2),ydim-24,tempbuf,ALPHABET,80);
		}
	}
	return(health[snum] <= 0);      //You were just injured
}

void changenumbombs(short snum, short deltanumbombs) {   // Andy did this
	numbombs[snum] += deltanumbombs;
	if (numbombs[snum] > 999) numbombs[snum] = 999;
	if (numbombs[snum] <= 0) {
		wsayfollow("doh.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
		numbombs[snum] = 0;
	}

	if ((snum == screenpeek) && (screensize <= xdim)) {
		std::sprintf(tempbuf,"B:%3d",numbombs[snum]);
		printext(8L,(ydim - 28L),tempbuf,ALPHABET,80);
	}
}

void changenummissiles(short snum, short deltanummissiles) {   // Andy did this
	nummissiles[snum] += deltanummissiles;
	if (nummissiles[snum] > 999) nummissiles[snum] = 999;
	if (nummissiles[snum] <= 0) {
		wsayfollow("doh.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
		nummissiles[snum] = 0;
	}

	if ((snum == screenpeek) && (screensize <= xdim)) {
		std::sprintf(tempbuf,"M:%3d",nummissiles[snum]);
		printext(8L,(ydim - 20L),tempbuf,ALPHABET,80);
	}
}

void changenumgrabbers(short snum, short deltanumgrabbers) {   // Andy did this
	numgrabbers[snum] += deltanumgrabbers;
	if (numgrabbers[snum] > 999) numgrabbers[snum] = 999;
	if (numgrabbers[snum] <= 0) {
		wsayfollow("doh.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
		numgrabbers[snum] = 0;
	}

	if ((snum == screenpeek) && (screensize <= xdim)) {
		std::sprintf(tempbuf,"G:%3d",numgrabbers[snum]);
		printext(8L,(ydim - 12L),tempbuf,ALPHABET,80);
	}
}

static int ostatusflytime = 0x80000000;
void drawstatusflytime(short snum) {   // Andy did this
	int nstatusflytime;

	if ((snum == screenpeek) && (screensize <= xdim)) {
		nstatusflytime = (((flytime[snum] + 119) - lockclock) / 120);
		if (nstatusflytime > 1000) nstatusflytime = 1000;
		else if (nstatusflytime < 0) nstatusflytime = 0;
		if (nstatusflytime != ostatusflytime) {
			if (nstatusflytime > 999) std::sprintf(tempbuf,"FT:BIG");
			else std::sprintf(tempbuf,"FT:%3d",nstatusflytime);
			printext((xdim - 56L),(ydim - 20L),tempbuf,ALPHABET,80);
			ostatusflytime = nstatusflytime;
		}
	}
}

void drawstatusbar(short snum) {   // Andy did this
	int nstatusflytime;

	if ((snum == screenpeek) && (screensize <= xdim)) {
		std::sprintf(tempbuf,"Deaths:%d",deaths[snum]);
		printext((xdim>>1)-(int)(std::strlen(tempbuf)<<2),ydim-16,tempbuf,ALPHABET,80);
		std::sprintf(tempbuf,"Health:%3d",health[snum]);
		printext((xdim>>1)-(int)(std::strlen(tempbuf)<<2),ydim-24,tempbuf,ALPHABET,80);

		std::sprintf(tempbuf,"B:%3d",numbombs[snum]);
		printext(8L,(ydim - 28L),tempbuf,ALPHABET,80);
		std::sprintf(tempbuf,"M:%3d",nummissiles[snum]);
		printext(8L,(ydim - 20L),tempbuf,ALPHABET,80);
		std::sprintf(tempbuf,"G:%3d",numgrabbers[snum]);
		printext(8L,(ydim - 12L),tempbuf,ALPHABET,80);

		nstatusflytime = (((flytime[snum] + 119) - lockclock) / 120);
		if (nstatusflytime < 0) {
			std::sprintf(tempbuf,"FT:  0");
			ostatusflytime = 0;
		}
		else if (nstatusflytime > 999) {
			std::sprintf(tempbuf,"FT:BIG");
			ostatusflytime = 999;
		}
		else {
			std::sprintf(tempbuf,"FT:%3d",nstatusflytime);
			ostatusflytime = nstatusflytime;
		}
		printext((xdim - 56L),(ydim - 20L),tempbuf,ALPHABET,80);
	}
}

void prepareboard(char *daboardfilename)
{
	short startwall;
	short endwall;
	short dasector;
	int i;
	int j;
	int k=0;
	int s;
	int dax;
	int day;
	int dax2;
	int day2;

	getmessageleng = 0;
	typemessageleng = 0;

	randomseed = 17L;

		//Clear (do)animation's list
	animatecnt = 0;
	typemode = 0;
	locselectedgun = 0;
	locselectedgun2 = 0;

	if (loadboard(daboardfilename,0,&posx[0],&posy[0],&posz[0],&ang[0],&cursectnum[0]) == -1)
	{
		musicoff();
		uninitmultiplayers();
		uninitengine();
		uninitsb();
		uninitgroupfile();
		std::printf("Board not found\n");
		std::exit(0);
	} else {
		char tempfn[BMAX_PATH + 1];
		char *fp;

		wm_setwindowtitle(daboardfilename);

		strncpy(tempfn, daboardfilename, BMAX_PATH);
		tempfn[BMAX_PATH] = 0;

		fp = strrchr(tempfn,'.');
		if (fp) *fp = 0;

		if (std::strlen(tempfn) <= BMAX_PATH-4) {
			std::strcat(tempfn,".mhk");
			loadmaphack(tempfn);
		}
	}

	setup3dscreen();

	for(i=0;i<MAXPLAYERS;i++)
	{
		posx[i] = posx[0];
		posy[i] = posy[0];
		posz[i] = posz[0];
		ang[i] = ang[0];
		cursectnum[i] = cursectnum[0];
		ocursectnum[i] = cursectnum[0];
		horiz[i] = 100;
		lastchaingun[i] = 0;
		health[i] = 100;
		dimensionmode[i] = 3;
		numbombs[i] = 0;
		numgrabbers[i] = 0;
		nummissiles[i] = 0;
		flytime[i] = 0L;
		zoom[i] = 768L;
		deaths[i] = 0L;
		playersprite[i] = -1;
		screensize = xdim;

		oposx[i] = posx[0];
		oposy[i] = posy[0];
		oposz[i] = posz[0];
		ohoriz[i] = horiz[0];
		ozoom[i] = zoom[0];
		oang[i] = ang[0];
	}

	myx = omyx = posx[myconnectindex];
	myy = omyy = posy[myconnectindex];
	myz = omyz = posz[myconnectindex];
	myhoriz = omyhoriz = horiz[myconnectindex];
	myang = omyang = ang[myconnectindex];
	mycursectnum = cursectnum[myconnectindex];
	myzvel = 0;

	movefifoplc = fakemovefifoplc = 0;
	syncvalhead = 0L; othersyncvalhead = 0L;
	syncvaltottail = 0L; syncvaltail = 0L;
	numinterpolations = 0;

	clearbufbyte(&oloc,sizeof(input),0L);

	std::ranges::fill(movefifoend, 0);
	std::ranges::fill(ffsync, input{});
	std::ranges::fill(ssync, input{});
	std::ranges::fill(osync, input{});

	//Scan sector tags
	std::ranges::fill(waterfountainwall, -1);
	std::ranges::fill(waterfountaincnt, 0);
	std::ranges::fill(slimesoundcnt, 0);

	warpsectorcnt = 0;      //Make a list of warping sectors
	xpanningsectorcnt = 0;  //Make a list of wall x-panning sectors
	floorpanningcnt = 0;    //Make a list of slime sectors
	dragsectorcnt = 0;      //Make a list of moving platforms
	swingcnt = 0;           //Make a list of swinging doors
	revolvecnt = 0;         //Make a list of revolving doors
	subwaytrackcnt = 0;     //Make a list of subways

	floormirrorcnt = 0;
	tilesizx[FLOORMIRROR] = 0;
	tilesizy[FLOORMIRROR] = 0;

	for(i=0;i<numsectors;i++)
	{
		switch(g_sector[i].lotag)
		{
			case 4:
				floorpanninglist[floorpanningcnt++] = i;
				break;
			case 10:
				warpsectorlist[warpsectorcnt++] = i;
				break;
			case 11:
				xpanningsectorlist[xpanningsectorcnt++] = i;
				break;
			case 12:
				dasector = i;
				dax = 0x7fffffff;
				day = 0x7fffffff;
				dax2 = 0x80000000;
				day2 = 0x80000000;
				startwall = g_sector[i].wallptr;
				endwall = startwall+g_sector[i].wallnum;
				for(j=startwall;j<endwall;j++)
				{
					if (wall[j].pt.x < dax) dax = wall[j].pt.x;
					if (wall[j].pt.y < day) day = wall[j].pt.y;
					if (wall[j].pt.x > dax2) dax2 = wall[j].pt.x;
					if (wall[j].pt.y > day2) day2 = wall[j].pt.y;
					if (wall[j].lotag == 3) k = j;
				}
				if (wall[k].pt.x == dax) dragxdir[dragsectorcnt] = -16;
				if (wall[k].pt.y == day) dragydir[dragsectorcnt] = -16;
				if (wall[k].pt.x == dax2) dragxdir[dragsectorcnt] = 16;
				if (wall[k].pt.y == day2) dragydir[dragsectorcnt] = 16;

				dasector = wall[startwall].nextsector;
				dragx1[dragsectorcnt] = 0x7fffffff;
				dragy1[dragsectorcnt] = 0x7fffffff;
				dragx2[dragsectorcnt] = 0x80000000;
				dragy2[dragsectorcnt] = 0x80000000;
				startwall = g_sector[dasector].wallptr;
				endwall = startwall+g_sector[dasector].wallnum;
				for(j=startwall;j<endwall;j++)
				{
					if (wall[j].pt.x < dragx1[dragsectorcnt]) dragx1[dragsectorcnt] = wall[j].pt.x;
					if (wall[j].pt.y < dragy1[dragsectorcnt]) dragy1[dragsectorcnt] = wall[j].pt.y;
					if (wall[j].pt.x > dragx2[dragsectorcnt]) dragx2[dragsectorcnt] = wall[j].pt.x;
					if (wall[j].pt.y > dragy2[dragsectorcnt]) dragy2[dragsectorcnt] = wall[j].pt.y;

					setinterpolation(&g_sector[dasector].floorz);
					setinterpolation(&wall[j].pt.x);
					setinterpolation(&wall[j].pt.y);
					setinterpolation(&wall[wall[j].nextwall].pt.x);
					setinterpolation(&wall[wall[j].nextwall].pt.y);
				}

				dragx1[dragsectorcnt] += (wall[g_sector[i].wallptr].pt.x-dax);
				dragy1[dragsectorcnt] += (wall[g_sector[i].wallptr].pt.y-day);
				dragx2[dragsectorcnt] -= (dax2-wall[g_sector[i].wallptr].pt.x);
				dragy2[dragsectorcnt] -= (day2-wall[g_sector[i].wallptr].pt.y);

				dragfloorz[dragsectorcnt] = g_sector[i].floorz;

				dragsectorlist[dragsectorcnt++] = i;
				break;
			case 13:
				startwall = g_sector[i].wallptr;
				endwall = startwall+g_sector[i].wallnum;
				for(j=startwall;j<endwall;j++)
				{
					if (wall[j].lotag == 4)
					{
						k = wall[wall[wall[wall[j].point2].point2].point2].point2;
						if ((wall[j].pt.x == wall[k].pt.x) && (wall[j].pt.y == wall[k].pt.y))
						{     //Door opens counterclockwise
							swingwall[swingcnt][0] = j;
							swingwall[swingcnt][1] = wall[j].point2;
							swingwall[swingcnt][2] = wall[wall[j].point2].point2;
							swingwall[swingcnt][3] = wall[wall[wall[j].point2].point2].point2;
							swingangopen[swingcnt] = 1536;
							swingangclosed[swingcnt] = 0;
							swingangopendir[swingcnt] = -1;
						}
						else
						{     //Door opens clockwise
							swingwall[swingcnt][0] = wall[j].point2;
							swingwall[swingcnt][1] = j;
							swingwall[swingcnt][2] = lastwall(j);
							swingwall[swingcnt][3] = lastwall(swingwall[swingcnt][2]);
							swingwall[swingcnt][4] = lastwall(swingwall[swingcnt][3]);
							swingangopen[swingcnt] = 512;
							swingangclosed[swingcnt] = 0;
							swingangopendir[swingcnt] = 1;
						}
						for(k=0;k<4;k++)
						{
							swingx[swingcnt][k] = wall[swingwall[swingcnt][k]].pt.x;
							swingy[swingcnt][k] = wall[swingwall[swingcnt][k]].pt.y;
						}

						swingsector[swingcnt] = i;
						swingang[swingcnt] = swingangclosed[swingcnt];
						swinganginc[swingcnt] = 0;
						swingcnt++;
					}
				}
				break;
			case 14:
				startwall = g_sector[i].wallptr;
				endwall = startwall+g_sector[i].wallnum;
				dax = 0L;
				day = 0L;
				for(j=startwall;j<endwall;j++)
				{
					dax += wall[j].pt.x;
					day += wall[j].pt.y;
				}
				revolvepivotx[revolvecnt] = dax / (endwall-startwall);
				revolvepivoty[revolvecnt] = day / (endwall-startwall);

				k = 0;
				for(j=startwall;j<endwall;j++)
				{
					revolvex[revolvecnt][k] = wall[j].pt.x;
					revolvey[revolvecnt][k] = wall[j].pt.y;

					setinterpolation(&wall[j].pt.x);
					setinterpolation(&wall[j].pt.y);
					setinterpolation(&wall[wall[j].nextwall].pt.x);
					setinterpolation(&wall[wall[j].nextwall].pt.y);

					k++;
				}
				revolvesector[revolvecnt] = i;
				revolveang[revolvecnt] = 0;

				revolvecnt++;
				break;
			case 15:
				subwaytracksector[subwaytrackcnt][0] = i;

				subwaystopcnt[subwaytrackcnt] = 0;
				dax = 0x7fffffff;
				day = 0x7fffffff;
				dax2 = 0x80000000;
				day2 = 0x80000000;
				startwall = g_sector[i].wallptr;
				endwall = startwall+g_sector[i].wallnum;
				for(j=startwall;j<endwall;j++)
				{
					if (wall[j].pt.x < dax) dax = wall[j].pt.x;
					if (wall[j].pt.y < day) day = wall[j].pt.y;
					if (wall[j].pt.x > dax2) dax2 = wall[j].pt.x;
					if (wall[j].pt.y > day2) day2 = wall[j].pt.y;
				}
				for(j=startwall;j<endwall;j++)
				{
					if (wall[j].lotag == 5)
					{
						if ((wall[j].pt.x > dax) && (wall[j].pt.y > day) && (wall[j].pt.x < dax2) && (wall[j].pt.y < day2))
						{
							subwayx[subwaytrackcnt] = wall[j].pt.x;
						}
						else
						{
							subwaystop[subwaytrackcnt][subwaystopcnt[subwaytrackcnt]] = wall[j].pt.x;
							subwaystopcnt[subwaytrackcnt]++;
						}
					}
				}

				for(j=1;j<subwaystopcnt[subwaytrackcnt];j++)
					for(k=0;k<j;k++)
						if (subwaystop[subwaytrackcnt][j] < subwaystop[subwaytrackcnt][k])
						{
							s = subwaystop[subwaytrackcnt][j];
							subwaystop[subwaytrackcnt][j] = subwaystop[subwaytrackcnt][k];
							subwaystop[subwaytrackcnt][k] = s;
						}

				subwaygoalstop[subwaytrackcnt] = 0;
				for(j=0;j<subwaystopcnt[subwaytrackcnt];j++)
					if (std::abs(subwaystop[subwaytrackcnt][j]-subwayx[subwaytrackcnt]) < std::abs(subwaystop[subwaytrackcnt][subwaygoalstop[subwaytrackcnt]]-subwayx[subwaytrackcnt]))
						subwaygoalstop[subwaytrackcnt] = j;

				subwaytrackx1[subwaytrackcnt] = dax;
				subwaytracky1[subwaytrackcnt] = day;
				subwaytrackx2[subwaytrackcnt] = dax2;
				subwaytracky2[subwaytrackcnt] = day2;

				subwaynumsectors[subwaytrackcnt] = 1;
				for(j=0;j<numsectors;j++)
					if (j != i)
					{
						startwall = g_sector[j].wallptr;
						if (wall[startwall].pt.x > subwaytrackx1[subwaytrackcnt])
							if (wall[startwall].pt.y > subwaytracky1[subwaytrackcnt])
								if (wall[startwall].pt.x < subwaytrackx2[subwaytrackcnt])
									if (wall[startwall].pt.y < subwaytracky2[subwaytrackcnt])
									{
										if (g_sector[j].floorz != g_sector[i].floorz)
										{
											g_sector[j].ceilingstat |= 64;
											g_sector[j].floorstat |= 64;
										}
										subwaytracksector[subwaytrackcnt][subwaynumsectors[subwaytrackcnt]] = j;
										subwaynumsectors[subwaytrackcnt]++;
									}
					}

				subwayvel[subwaytrackcnt] = 64;
				subwaypausetime[subwaytrackcnt] = 720;

				startwall = g_sector[i].wallptr;
				endwall = startwall+g_sector[i].wallnum;
				for(k=startwall;k<endwall;k++)
					if (wall[k].pt.x > subwaytrackx1[subwaytrackcnt])
						if (wall[k].pt.y > subwaytracky1[subwaytrackcnt])
							if (wall[k].pt.x < subwaytrackx2[subwaytrackcnt])
								if (wall[k].pt.y < subwaytracky2[subwaytrackcnt])
									setinterpolation(&wall[k].pt.x);

				for(j=1;j<subwaynumsectors[subwaytrackcnt];j++)
				{
					dasector = subwaytracksector[subwaytrackcnt][j];

					startwall = g_sector[dasector].wallptr;
					endwall = startwall+g_sector[dasector].wallnum;
					for(k=startwall;k<endwall;k++)
						setinterpolation(&wall[k].pt.x);

					for(k=headspritesect[dasector];k>=0;k=nextspritesect[k])
						if (statrate[sprite[k].statnum] < 0)
							setinterpolation(&sprite[k].x);
				}


				subwaytrackcnt++;
				break;
		}
		if (g_sector[i].floorpicnum == FLOORMIRROR)
			floormirrorsector[mirrorcnt++] = i;
		//if (g_sector[i].ceilingpicnum == FLOORMIRROR) floormirrorsector[mirrorcnt++] = i; //SOS
	}

		//Scan wall tags

	mirrorcnt = 0;
	tilesizx[MIRROR] = 0;
	tilesizy[MIRROR] = 0;
	for(i=0;i<MAXMIRRORS;i++)
	{
		tilesizx[i+MIRRORLABEL] = 0;
		tilesizy[i+MIRRORLABEL] = 0;
	}

	ypanningwallcnt = 0;
	for(i=0;i<numwalls;i++)
	{
		if (wall[i].lotag == 1) ypanningwalllist[ypanningwallcnt++] = i;
		s = wall[i].nextsector;
		if ((s >= 0) && (wall[i].overpicnum == MIRROR) && (wall[i].cstat&32))
		{
			if ((g_sector[s].floorstat&1) == 0)
			{
				wall[i].overpicnum = MIRRORLABEL+mirrorcnt;
				g_sector[s].ceilingpicnum = MIRRORLABEL+mirrorcnt;
				g_sector[s].floorpicnum = MIRRORLABEL+mirrorcnt;
				g_sector[s].floorstat |= 1;
				mirrorwall[mirrorcnt] = i;
				mirrorsector[mirrorcnt] = s;
				mirrorcnt++;
			}
			else
				wall[i].overpicnum = g_sector[s].ceilingpicnum;
		}
	}

		//Invalidate textures in g_sector behind mirror
	for(i=0;i<mirrorcnt;i++)
	{
		k = mirrorsector[i];
		startwall = g_sector[k].wallptr;
		endwall = startwall + g_sector[k].wallnum;
		for(j=startwall;j<endwall;j++)
		{
			wall[j].picnum = MIRROR;
			wall[j].overpicnum = MIRROR;
		}
	}

		//Scan sprite tags&picnum's

	turnspritecnt = 0;
	for(i=0;i<MAXSPRITES;i++)
	{
		if (sprite[i].lotag == 3) turnspritelist[turnspritecnt++] = i;

		if (sprite[i].statnum < MAXSTATUS)    //That is, if sprite exists
			switch(sprite[i].picnum)
			{
				case BROWNMONSTER:              //All cases here put the sprite
					if ((sprite[i].cstat&128) == 0)
					{
						sprite[i].z -= ((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);
						sprite[i].cstat |= 128;
					}
					sprite[i].extra = sprite[i].ang;
					sprite[i].clipdist = mulscalen<7>(sprite[i].xrepeat,tilesizx[sprite[i].picnum]);
					if (sprite[i].statnum != 1) changespritestat(i,2);   //on waiting for you (list 2)
					sprite[i].lotag = mulscalen<5>(sprite[i].xrepeat,sprite[i].yrepeat);
					sprite[i].cstat |= 0x101;    //Set the hitscan sensitivity bit
					break;
				case AL:
					sprite[i].cstat |= 0x101;    //Set the hitscan sensitivity bit
					sprite[i].lotag = 0x60;
					changespritestat(i,0);
					break;
				case EVILAL:
					sprite[i].cstat |= 0x101;    //Set the hitscan sensitivity bit
					sprite[i].lotag = 0x60;
					changespritestat(i,10);
					break;
			}
	}

	for(i=MAXSPRITES-1;i>=0;i--) copybuf(&sprite[i].x,&osprite[i].x,3);

	searchmap(cursectnum[connecthead]);

	lockclock = 0;
	ototalclock = 0;
	gotlastpacketclock = 0;

	screensize = xdim;
	dax = ((xdim-screensize)>>1);
	dax2 = dax+screensize-1;
	day = (((ydim-32)-scale(screensize,ydim-32,xdim))>>1);
	day2 = day + scale(screensize,ydim-32,xdim)-1;
	setview(dax,day,dax2,day2);

	startofdynamicinterpolations = numinterpolations;

	/*
	for(i=connecthead;i>=0;i=connectpoint2[i]) myminlag[i] = 0;
	otherminlag = mymaxlag = 0;
	*/
}

void checktouchsprite(short snum, short sectnum)
{
	int i;
	int nexti;

	if ((sectnum < 0) || (sectnum >= numsectors)) return;

	for(i=headspritesect[sectnum];i>=0;i=nexti)
	{
		nexti = nextspritesect[i];
		if (sprite[i].cstat&0x8000) continue;
		if ((std::abs(posx[snum]-sprite[i].x)+std::abs(posy[snum]-sprite[i].y) < 512) && (std::abs((posz[snum]>>8)-((sprite[i].z>>8)-(tilesizy[sprite[i].picnum]>>1))) <= 40))
		{
			switch(sprite[i].picnum)
			{
				case COIN:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,192L,&sprite[i].x,&sprite[i].y,0);
					changehealth(snum,5);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*60;
						changespritestat((short)i,11);
					}
					break;
				case DIAMONDS:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					changehealth(snum,15);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*120;
						changespritestat((short)i,11);
					}
					break;
				case COINSTACK:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					changehealth(snum,25);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*180;
						changespritestat((short)i,11);
					}
					break;
				case GIFTBOX:
					wsayfollow("getstuff.wav",4096L+(krand()&127)+256-mulscalen<4>(sprite[i].xrepeat,sprite[i].yrepeat),208L,&sprite[i].x,&sprite[i].y,0);
					changehealth(snum, std::max(mulscalen<8>(sprite[i].xrepeat, sprite[i].yrepeat), 1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 90*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case CANNON:
					wsayfollow("getstuff.wav",3584L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (snum == myconnectindex) keystatus[4] = 1;
					changenumbombs(snum,((sprite[i].xrepeat+sprite[i].yrepeat)>>1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 60*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case LAUNCHER:
					wsayfollow("getstuff.wav",3584L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (snum == myconnectindex) keystatus[5] = 1;
					changenummissiles(snum,((sprite[i].xrepeat+sprite[i].yrepeat)>>1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 90*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case GRABCANNON:
					wsayfollow("getstuff.wav",3584L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (snum == myconnectindex) keystatus[6] = 1;
					changenumgrabbers(snum,((sprite[i].xrepeat+sprite[i].yrepeat)>>1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case AIRPLANE:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (flytime[snum] < lockclock) flytime[snum] = lockclock;
					flytime[snum] += 60*(sprite[i].xrepeat+sprite[i].yrepeat);
					drawstatusflytime(snum);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
			}
		}
	}
}

void checkgrabbertouchsprite(short snum, short sectnum)   // Andy did this
{
	int i;
	int nexti;
	short onum;

	if ((sectnum < 0) || (sectnum >= numsectors)) return;
	onum = (sprite[snum].owner & (MAXSPRITES - 1));

	for(i=headspritesect[sectnum];i>=0;i=nexti)
	{
		nexti = nextspritesect[i];
		if (sprite[i].cstat&0x8000) continue;
		if ((std::abs(sprite[snum].x-sprite[i].x)+std::abs(sprite[snum].y-sprite[i].y) < 512) && (std::abs((sprite[snum].z>>8)-((sprite[i].z>>8)-(tilesizy[sprite[i].picnum]>>1))) <= 40))
		{
			switch(sprite[i].picnum)
			{
				case COIN:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,192L,&sprite[i].x,&sprite[i].y,0);
					changehealth(onum,5);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*60;
						changespritestat((short)i,11);
					}
					break;
				case DIAMONDS:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					changehealth(onum,15);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*120;
						changespritestat((short)i,11);
					}
					break;
				case COINSTACK:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					changehealth(onum,25);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*180;
						changespritestat((short)i,11);
					}
					break;
				case GIFTBOX:
					wsayfollow("getstuff.wav",4096L+(krand()&127)+256-mulscalen<4>(sprite[i].xrepeat,sprite[i].yrepeat),208L,&sprite[i].x,&sprite[i].y,0);
					changehealth(onum, std::max(mulscalen<8>(sprite[i].xrepeat, sprite[i].yrepeat), 1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 90*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case CANNON:
					wsayfollow("getstuff.wav",3584L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (onum == myconnectindex) keystatus[4] = 1;
					changenumbombs(onum,((sprite[i].xrepeat+sprite[i].yrepeat)>>1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 60*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case LAUNCHER:
					wsayfollow("getstuff.wav",3584L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (onum == myconnectindex) keystatus[5] = 1;
					changenummissiles(onum,((sprite[i].xrepeat+sprite[i].yrepeat)>>1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 90*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case GRABCANNON:
					wsayfollow("getstuff.wav",3584L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (onum == myconnectindex) keystatus[6] = 1;
					changenumgrabbers(onum,((sprite[i].xrepeat+sprite[i].yrepeat)>>1));
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
				case AIRPLANE:
					wsayfollow("getstuff.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					if (flytime[snum] < lockclock) flytime[snum] = lockclock;
					flytime[onum] += 60*(sprite[i].xrepeat+sprite[i].yrepeat);
					drawstatusflytime(onum);
					if (sprite[i].statnum == 12) deletesprite((short)i);
					else {
						sprite[i].cstat |= 0x8000;
						sprite[i].extra = 120*(sprite[i].xrepeat+sprite[i].yrepeat);
						changespritestat((short)i,11);
					}
					break;
			}
		}
	}
}

void shootgun(short snum, int x, int y, int z,
	short daang, int dahoriz, short dasectnum, unsigned char guntype)
{
	short hitsect;
	short hitwall;
	short hitsprite;
	short daang2;
	int j;
	int daz2;
	int hitx;
	int hity;
	int hitz;

	switch(guntype)
	{
		case 0:    //Shoot chain gun
			daang2 = ((daang + (krand()&31)-16)&2047);
			daz2 = ((100-dahoriz)*2000) + ((krand()-32768)>>1);

			hitscan(x,y,z,dasectnum,                   //Start position
				sintable[(daang2+512)&2047],            //X vector of 3D ang
				sintable[daang2&2047],                  //Y vector of 3D ang
				daz2,                                   //Z vector of 3D ang
				&hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,CLIPMASK1);

			if (wall[hitwall].picnum == KENPICTURE)
			{
				if (waloff[MAXTILES-1] != 0) wall[hitwall].picnum = MAXTILES-1;
				wsayfollow("hello.wav",4096L+(krand()&127)-64,256L,&wall[hitwall].pt.x,&wall[hitwall].pt.y,0);
			}
			else if (((hitwall < 0) && (hitsprite < 0) && (hitz >= z) && ((g_sector[hitsect].floorpicnum == SLIME) || (g_sector[hitsect].floorpicnum == FLOORMIRROR))) || ((hitwall >= 0) && (wall[hitwall].picnum == SLIME)))
			{    //If you shoot slime, make a splash
				wsayfollow("splash.wav",4096L+(krand()&511)-256,256L,&hitx,&hity,0);
				spawnsprite(j,hitx,hity,hitz,2,0,0,32,64,64,0,0,SPLASH,daang,
					0,0,0,snum+4096,hitsect,4,63,0,0); //63=time left for splash
			}
			else
			{
				wsayfollow("shoot.wav",4096L+(krand()&127)-64,256L,&hitx,&hity,0);

				if ((hitsprite >= 0) && (sprite[hitsprite].statnum < MAXSTATUS))
					switch(sprite[hitsprite].picnum)
					{
						case BROWNMONSTER:
							if (sprite[hitsprite].lotag > 0) sprite[hitsprite].lotag -= 10;
							if (sprite[hitsprite].lotag > 0)
							{
								wsayfollow("hurt.wav",4096L+(krand()&511)-256,256L,&hitx,&hity,0);
								if (sprite[hitsprite].lotag <= 25)
									sprite[hitsprite].cstat |= 2;
							}
							else
							{
								wsayfollow("mondie.wav",4096L+(krand()&127)-64,256L,&hitx,&hity,0);
								sprite[hitsprite].z += ((tilesizy[sprite[hitsprite].picnum]*sprite[hitsprite].yrepeat)<<1);
								sprite[hitsprite].picnum = GIFTBOX;
								sprite[hitsprite].cstat &= ~0x83;    //Should not clip, foot-z
								changespritestat(hitsprite,12);

								spawnsprite(j,hitx,hity,hitz+(32<<8),0,-4,0,32,64,64,
									0,0,EXPLOSION,daang,0,0,0,snum+4096,
									hitsect,5,31,0,0);
							}
							break;
						case EVILAL:
							wsayfollow("blowup.wav",4096L+(krand()&127)-64,256L,&hitx,&hity,0);
							sprite[hitsprite].picnum = EVILALGRAVE;
							sprite[hitsprite].cstat = 0;
							sprite[hitsprite].xvel = (krand()&255)-128;
							sprite[hitsprite].yvel = (krand()&255)-128;
							sprite[hitsprite].zvel = (krand()&4095)-3072;
							changespritestat(hitsprite,9);

							spawnsprite(j,hitx,hity,hitz+(32<<8),0,-4,0,32,64,64,0,
								0,EXPLOSION,daang,0,0,0,snum+4096,hitsect,5,31,0,0);
								 //31=time left for explosion

							break;
						case PLAYER:
							for(j=connecthead;j>=0;j=connectpoint2[j])
								if (playersprite[j] == hitsprite)
								{
									wsayfollow("ouch.wav",4096L+(krand()&127)-64,256L,&hitx,&hity,0);
									changehealth(j,-10);
									break;
								}
							break;
					}

				spawnsprite(j,hitx,hity,hitz+(8<<8),2,-4,0,32,16,16,0,0,
					EXPLOSION,daang,0,0,0,snum+4096,hitsect,3,63,0,0);

					//Sprite starts out with center exactly on wall.
					//This moves it back enough to see it at all angles.
				movesprite((short)j,-(((int)sintable[(512+daang)&2047]*TICSPERFRAME)<<4),-(((int)sintable[daang]*TICSPERFRAME)<<4),0L,4L<<8,4L<<8,CLIPMASK1);
			}
			break;
		case 1:    //Shoot silver sphere bullet
			spawnsprite(j,x,y,z,1+128,0,0,16,64,64,0,0,BULLET,daang,
						  sintable[(daang+512)&2047]>>5,sintable[daang&2047]>>5,
						  (100-dahoriz)<<6,snum+4096,dasectnum,6,0,0,0);
			wsayfollow("shoot2.wav",4096L+(krand()&127)-64,128L,&sprite[j].x,&sprite[j].y,1);
			break;
		case 2:    //Shoot bomb
		  spawnsprite(j,x,y,z,128,0,0,12,16,16,0,0,BOMB,daang,
			  sintable[(daang+512)&2047]*5>>8,sintable[daang&2047]*5>>8,
			  (80-dahoriz)<<6,snum+4096,dasectnum,6,0,0,0);
			wsayfollow("shoot3.wav",4096L+(krand()&127)-64,192L,&sprite[j].x,&sprite[j].y,1);
			break;
		case 3:    //Shoot missile (Andy did this)
			spawnsprite(j,x,y,z,1+128,0,0,16,32,32,0,0,MISSILE,daang,
						  sintable[(daang+512)&2047]>>4,sintable[daang&2047]>>4,
						  (100-dahoriz)<<7,snum+4096,dasectnum,6,0,0,0);
			wsayfollow("shoot3.wav",4096L+(krand()&127)-64,192L,&sprite[j].x,&sprite[j].y,1);
			break;
		case 4:    //Shoot grabber (Andy did this)
			spawnsprite(j,x,y,z,1+128,0,0,16,64,64,0,0,GRABBER,daang,
						  sintable[(daang+512)&2047]>>5,sintable[daang&2047]>>5,
						  (100-dahoriz)<<6,snum+4096,dasectnum,6,0,0,0);
			wsayfollow("shoot4.wav",4096L+(krand()&127)-64,128L,&sprite[j].x,&sprite[j].y,1);
			break;
	}
}

void analyzesprites(int dax, int day)
{
	int i;
	int j=0;
	int k;
	int *intptr;
	point3di *ospr;
	spritetype *tspr;

		//This function is called between drawrooms() and drawmasks()
		//It has a list of possible sprites that may be drawn on this frame

	for(i=0,tspr=&tsprite[0];i<spritesortcnt;i++,tspr++)
	{
		if (usevoxels && tiletovox[tspr->picnum] >= 0)
		switch(tspr->picnum)
		{
			case PLAYER:
				//   //Get which of the 8 angles of the sprite to draw (0-7)
				//   //k ranges from 0-7
				//k = getangle(tspr->x-dax,tspr->y-day);
				//k = (((tspr->ang+3072+128-k)&2047)>>8)&7;
				//   //This guy has only 5 pictures for 8 angles (3 are x-flipped)
				//if (k <= 4)
				//{
				//   tspr->picnum += (k<<2);
				//   tspr->cstat &= ~4;   //clear x-flipping bit
				//}
				//else
				//{
				//   tspr->picnum += ((8-k)<<2);
				//   tspr->cstat |= 4;    //set x-flipping bit
				//}

				if ((tspr->cstat&2) == 0)
				{
					//tspr->cstat |= 48; tspr->picnum = tiletovox[tspr->picnum];
					intptr = (int *)voxoff[ tiletovox[PLAYER] ][0];
					tspr->xrepeat = scale(tspr->xrepeat,56,intptr[2]);
					tspr->yrepeat = scale(tspr->yrepeat,56,intptr[2]);
					tspr->shade -= 6;
				}
				break;
			case BROWNMONSTER:
				//tspr->cstat |= 48; tspr->picnum = tiletovox[tspr->picnum];
				break;
		}

		k = statrate[tspr->statnum];
		if (k >= 0)  //Interpolate moving sprite
		{
			ospr = &osprite[tspr->owner];
			switch(k)
			{
				case 0: j = smoothratio; break;
				case 1: j = (smoothratio>>1)+(((nummoves-tspr->owner)&1)<<15); break;
				case 3: j = (smoothratio>>2)+(((nummoves-tspr->owner)&3)<<14); break;
				case 7: j = (smoothratio>>3)+(((nummoves-tspr->owner)&7)<<13); break;
				case 15: j = (smoothratio>>4)+(((nummoves-tspr->owner)&15)<<12); break;
			}
			k = tspr->x-ospr->x; tspr->x = ospr->x;
			if (k != 0) tspr->x += mulscalen<16>(k,j);
			k = tspr->y-ospr->y; tspr->y = ospr->y;
			if (k != 0) tspr->y += mulscalen<16>(k,j);
			k = tspr->z-ospr->z; tspr->z = ospr->z;
			if (k != 0) tspr->z += mulscalen<16>(k,j);
		}

			//Don't allow close explosion sprites to be transluscent
		k = tspr->statnum;
		if ((k == 3) || (k == 4) || (k == 5) || (k == 7))
			if (std::abs(dax-tspr->x) < 256)
				if (std::abs(day-tspr->y) < 256)
					tspr->cstat &= ~2;

		tspr->shade += 6;
		if (g_sector[tspr->sectnum].ceilingstat&1)
			tspr->shade += g_sector[tspr->sectnum].ceilingshade;
		else
			tspr->shade += g_sector[tspr->sectnum].floorshade;
	}
}

void tagcode()
{
	int i;
	int j;
	int k;
	int l;
	int s;
	int dax;
	int day;
	int cnt;
	int good;
	short startwall;
	short endwall;
	short dasector;
	short p;
	short oldang;

	for(p=connecthead;p>=0;p=connectpoint2[p])
	{
		if (g_sector[cursectnum[p]].lotag == 1)
		{
			activatehitag(g_sector[cursectnum[p]].hitag);
			g_sector[cursectnum[p]].lotag = 0;
			g_sector[cursectnum[p]].hitag = 0;
		}
		if ((g_sector[cursectnum[p]].lotag == 2) && (cursectnum[p] != ocursectnum[p]))
			activatehitag(g_sector[cursectnum[p]].hitag);
	}

	for(i=0;i<warpsectorcnt;i++)
	{
		dasector = warpsectorlist[i];
		j = ((lockclock&127)>>2);
		if (j >= 16) j = 31-j;
		{
			g_sector[dasector].ceilingshade = j;
			g_sector[dasector].floorshade = j;
			startwall = g_sector[dasector].wallptr;
			endwall = startwall+g_sector[dasector].wallnum;
			for(s=startwall;s<endwall;s++)
				wall[s].shade = j;
		}
	}

	for(p=connecthead;p>=0;p=connectpoint2[p])
		if (g_sector[cursectnum[p]].lotag == 10)  //warp sector
		{
			if (cursectnum[p] != ocursectnum[p])
			{
				warpsprite(playersprite[p]);
				posx[p] = sprite[playersprite[p]].x;
				posy[p] = sprite[playersprite[p]].y;
				posz[p] = sprite[playersprite[p]].z;
				ang[p] = sprite[playersprite[p]].ang;
				cursectnum[p] = sprite[playersprite[p]].sectnum;

				sprite[playersprite[p]].z += EYEHEIGHT;

				//warp(&posx[p],&posy[p],&posz[p],&ang[p],&cursectnum[p]);
					//Update sprite representation of player
				//setsprite(playersprite[p],posx[p],posy[p],posz[p]+EYEHEIGHT);
				//sprite[playersprite[p]].ang = ang[p];
			}
		}

	for(i=0;i<xpanningsectorcnt;i++)   //animate wall x-panning sectors
	{
		dasector = xpanningsectorlist[i];

		startwall = g_sector[dasector].wallptr;
		endwall = startwall+g_sector[dasector].wallnum;
		for(s=startwall;s<endwall;s++)
			wall[s].xpanning = ((lockclock>>2)&255);
	}

	for(i=0;i<ypanningwallcnt;i++)
		wall[ypanningwalllist[i]].ypanning = ~(lockclock&255);

	for(i=0;i<turnspritecnt;i++)
	{
		sprite[turnspritelist[i]].ang += (TICSPERFRAME<<2);
		sprite[turnspritelist[i]].ang &= 2047;
	}

	for(i=0;i<floorpanningcnt;i++)   //animate floor of slime sectors
	{
		g_sector[floorpanninglist[i]].floorxpanning = ((lockclock>>2)&255);
		g_sector[floorpanninglist[i]].floorypanning = ((lockclock>>2)&255);
	}

	for(i=0;i<dragsectorcnt;i++)
	{
		dasector = dragsectorlist[i];

		startwall = g_sector[dasector].wallptr;
		endwall = startwall+g_sector[dasector].wallnum;

		if (wall[startwall].pt.x+dragxdir[i] < dragx1[i]) dragxdir[i] = 16;
		if (wall[startwall].pt.y+dragydir[i] < dragy1[i]) dragydir[i] = 16;
		if (wall[startwall].pt.x+dragxdir[i] > dragx2[i]) dragxdir[i] = -16;
		if (wall[startwall].pt.y+dragydir[i] > dragy2[i]) dragydir[i] = -16;

		for(j=startwall;j<endwall;j++)
			dragpoint(j,wall[j].pt.x+dragxdir[i],wall[j].pt.y+dragydir[i]);
		j = g_sector[dasector].floorz;
		g_sector[dasector].floorz = dragfloorz[i]+(sintable[(lockclock<<4)&2047]>>3);

		for(p=connecthead;p>=0;p=connectpoint2[p])
			if (cursectnum[p] == dasector)
			{
				posx[p] += dragxdir[i];
				posy[p] += dragydir[i];
				if (p == myconnectindex)
					{ myx += dragxdir[i]; myy += dragydir[i]; }
				//posz[p] += (g_sector[dasector].floorz-j);

					//Update sprite representation of player
				setsprite(playersprite[p],posx[p],posy[p],posz[p]+EYEHEIGHT);
				sprite[playersprite[p]].ang = ang[p];
			}
	}

	for(i=0;i<swingcnt;i++)
	{
		if (swinganginc[i] != 0)
		{
			oldang = swingang[i];
			for(j=0;j<(TICSPERFRAME<<2);j++)
			{
				swingang[i] = ((swingang[i]+swinganginc[i])&2047);
				if (swingang[i] == swingangclosed[i])
				{
					wsayfollow("closdoor.wav",4096L+(krand()&511)-256,256L,&swingx[i][0],&swingy[i][0],0);
					swinganginc[i] = 0;
				}
				if (swingang[i] == swingangopen[i]) swinganginc[i] = 0;
			}
			for(k=1;k<=3;k++)
				rotatepoint(swingx[i][0],swingy[i][0],swingx[i][k],swingy[i][k],swingang[i],&wall[swingwall[i][k]].pt.x,&wall[swingwall[i][k]].pt.y);

			if (swinganginc[i] != 0)
			{
				for(p=connecthead;p>=0;p=connectpoint2[p])
					if ((cursectnum[p] == swingsector[i]) || (testneighborsectors(cursectnum[p],swingsector[i]) == 1))
					{
						cnt = 256;
						do
						{
							good = 1;

								//swingangopendir is -1 if forwards, 1 is backwards
							l = (swingangopendir[i] > 0);
							for(k=l+3;k>=l;k--)
								if (clipinsidebox(posx[p],posy[p],swingwall[i][k],128L) != 0)
								{
									good = 0;
									break;
								}
							if (good == 0)
							{
								if (cnt == 256)
								{
									swinganginc[i] = -swinganginc[i];
									swingang[i] = oldang;
								}
								else
								{
									swingang[i] = ((swingang[i]-swinganginc[i])&2047);
								}
								for(k=1;k<=3;k++)
									rotatepoint(swingx[i][0],swingy[i][0],swingx[i][k],swingy[i][k],swingang[i],&wall[swingwall[i][k]].pt.x,&wall[swingwall[i][k]].pt.y);
								if (swingang[i] == swingangclosed[i])
								{
									wsayfollow("closdoor.wav",4096L+(krand()&511)-256,256L,&swingx[i][0],&swingy[i][0],0);
									swinganginc[i] = 0;
									break;
								}
								if (swingang[i] == swingangopen[i])
								{
									swinganginc[i] = 0;
									break;
								}
								cnt--;
							}
						} while ((good == 0) && (cnt > 0));
					}
			}
		}
		if (swinganginc[i] == 0)
			for(j=1;j<=3;j++)
			{
				stopinterpolation(&wall[swingwall[i][j]].pt.x);
				stopinterpolation(&wall[swingwall[i][j]].pt.y);
			}
	}

	for(i=0;i<revolvecnt;i++)
	{
		startwall = g_sector[revolvesector[i]].wallptr;
		endwall = startwall + g_sector[revolvesector[i]].wallnum;

		revolveang[i] = ((revolveang[i]-(TICSPERFRAME<<2))&2047);
		for(k=startwall;k<endwall;k++)
		{
			rotatepoint(revolvepivotx[i],revolvepivoty[i],revolvex[i][k-startwall],revolvey[i][k-startwall],revolveang[i],&dax,&day);
			dragpoint(k,dax,day);
		}
	}

	for(i=0;i<subwaytrackcnt;i++)
	{
		if ((subwayvel[i] < -2) || (subwayvel[i] > 2))
		{
			dasector = subwaytracksector[i][0];
			startwall = g_sector[dasector].wallptr;
			endwall = startwall+g_sector[dasector].wallnum;
			for(k=startwall;k<endwall;k++)
				if (wall[k].pt.x > subwaytrackx1[i])
					if (wall[k].pt.y > subwaytracky1[i])
						if (wall[k].pt.x < subwaytrackx2[i])
							if (wall[k].pt.y < subwaytracky2[i])
								wall[k].pt.x += subwayvel[i];

			for(j=1;j<subwaynumsectors[i];j++)
			{
				dasector = subwaytracksector[i][j];

				startwall = g_sector[dasector].wallptr;
				endwall = startwall+g_sector[dasector].wallnum;
				for(k=startwall;k<endwall;k++)
					wall[k].pt.x += subwayvel[i];

				for(s=headspritesect[dasector];s>=0;s=nextspritesect[s])
					sprite[s].x += subwayvel[i];
			}

			for(p=connecthead;p>=0;p=connectpoint2[p])
				if (cursectnum[p] != subwaytracksector[i][0])
					if (g_sector[cursectnum[p]].floorz != g_sector[subwaytracksector[i][0]].floorz)
						if (posx[p] > subwaytrackx1[i])
							if (posy[p] > subwaytracky1[i])
								if (posx[p] < subwaytrackx2[i])
									if (posy[p] < subwaytracky2[i])
									{
										posx[p] += subwayvel[i];
										if (p == myconnectindex)
											{ myx += subwayvel[i]; }

											//Update sprite representation of player
										setsprite(playersprite[p],posx[p],posy[p],posz[p]+EYEHEIGHT);
										sprite[playersprite[p]].ang = ang[p];
									}

			subwayx[i] += subwayvel[i];
		}

		j = subwayvel[i];
		k = subwaystop[i][subwaygoalstop[i]] - subwayx[i];
		if (k > 0)
		{
			if (k > 4096)
			{
				if (subwayvel[i] < 256) subwayvel[i]++;
			}
			else
				subwayvel[i] = (k>>4)+1;
		}
		else if (k < 0)
		{
			if (k < -4096)
			{
				if (subwayvel[i] > -256) subwayvel[i]--;
			}
			else
				subwayvel[i] = (k>>4)-1;
		}
		if ((j < 0) && (subwayvel[i] >= 0)) subwayvel[i] = -1;
		if ((j > 0) && (subwayvel[i] <= 0)) subwayvel[i] = 1;

		if ((subwayvel[i] <= 2) && (subwayvel[i] >= -2) && (std::abs(k) < 2048))
		{
			  //Open / close doors
			if ((subwaypausetime[i] == 720) || ((subwaypausetime[i] >= 120) && (subwaypausetime[i]-TICSPERFRAME < 120)))
				activatehitag(g_sector[subwaytracksector[i][0]].hitag);

			subwaypausetime[i] -= TICSPERFRAME;
			if (subwaypausetime[i] < 0)
			{
				subwaypausetime[i] = 720;
				if (subwayvel[i] < 0)
				{
					subwaygoalstop[i]--;
					if (subwaygoalstop[i] < 0)
					{
						subwaygoalstop[i] = 1;
						subwayvel[i] = 1;
					}
				}
				else if (subwayvel[i] > 0)
				{
					subwaygoalstop[i]++;
					if (subwaygoalstop[i] >= subwaystopcnt[i])
					{
						subwaygoalstop[i] = subwaystopcnt[i]-2;
						subwayvel[i] = -1;
					}
				}
			}
		}
	}
}

void statuslistcode()
{
	short p;
	short target;
	short hitobject;
	short daang;
	short osectnum;
	short movestat;
	int i;
	int nexti;
	int j;
	int nextj;
	int k;
	int l;
	int dax;
	int day;
	int daz;
	int dist=0;
	int ox;
	int oy;
	int mindist;
	int doubvel;
	int xvect;
	int yvect;

		//Go through active BROWNMONSTER list
	for(i=headspritestat[1];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		k = krand();

			//Choose a target player
		mindist = 0x7fffffff; target = connecthead;
		for(p=connecthead;p>=0;p=connectpoint2[p])
		{
			dist = std::abs(sprite[i].x-posx[p])+std::abs(sprite[i].y-posy[p]);
			if (dist < mindist) mindist = dist, target = p;
		}

			//brown monster decides to shoot bullet
		if ((k&63) == 23)
		{
			if (!cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,posx[target],posy[target],posz[target],cursectnum[target]))
			{
				if ((k&0xf00) == 0xb00) changespritestat(i,2);
			}
			else
			{
				wsayfollow("monshoot.wav",5144L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,1);

				doubvel = (TICSPERFRAME<<static_cast<int>((ssync[target].bits & 256) > 0));
				xvect = 0, yvect = 0;
				if (ssync[target].fvel != 0)
				{
					xvect += ((((int)ssync[target].fvel)*doubvel*(int)sintable[(ang[target]+512)&2047])>>3);
					yvect += ((((int)ssync[target].fvel)*doubvel*(int)sintable[ang[target]&2047])>>3);
				}
				if (ssync[target].svel != 0)
				{
					xvect += ((((int)ssync[target].svel)*doubvel*(int)sintable[ang[target]&2047])>>3);
					yvect += ((((int)ssync[target].svel)*doubvel*(int)sintable[(ang[target]+1536)&2047])>>3);
				}

				ox = posx[target]; oy = posy[target];

					//distance is j
				j = std::sqrt((ox-sprite[i].x)*(ox-sprite[i].x)+(oy-sprite[i].y)*(oy-sprite[i].y));

				switch((sprite[i].extra>>11)&3)
				{
					case 1: j = -(j>>1); break;
					case 3: j = 0; break;
					case 0: case 2: break;
				}
				sprite[i].extra += 2048;

					//rate is (TICSPERFRAME<<19)
				xvect = scale(xvect,j,TICSPERFRAME<<19);
				yvect = scale(yvect,j,TICSPERFRAME<<19);
				clipmove(&ox,&oy,&posz[target],&cursectnum[target],xvect<<14,yvect<<14,128L,4<<8,4<<8,CLIPMASK0);
				ox -= sprite[i].x;
				oy -= sprite[i].y;

				daang = ((getangle(ox,oy)+(krand()&7)-4)&2047);

				dax = (sintable[(daang+512)&2047]>>6);
				day = (sintable[daang&2047]>>6);
				daz = 0;
				if (ox != 0)
					daz = scale(dax,posz[target]+(8<<8)-sprite[i].z,ox);
				else if (oy != 0)
					daz = scale(day,posz[target]+(8<<8)-sprite[i].z,oy);

				spawnsprite(j,sprite[i].x,sprite[i].y,sprite[i].z,128,0,0,
					16,sprite[i].xrepeat,sprite[i].yrepeat,0,0,BULLET,daang,dax,day,daz,i,sprite[i].sectnum,6,0,0,0);

				sprite[i].extra &= (~2047);
			}
		}

			//Move brown monster
		dax = sprite[i].x;   //Back up old x&y if stepping off cliff
		day = sprite[i].y;

		doubvel = std::max(mulscalen<7>(sprite[i].xrepeat, sprite[i].yrepeat), 4);

		osectnum = sprite[i].sectnum;
		movestat = movesprite((short)i,(int)sintable[(sprite[i].ang+512)&2047]*doubvel,(int)sintable[sprite[i].ang]*doubvel,0L,4L<<8,4L<<8,CLIPMASK0);
		if (globloz > sprite[i].z+(48<<8))
			{ sprite[i].x = dax; sprite[i].y = day; movestat = 1; }
		else
			sprite[i].z = globloz-((tilesizy[sprite[i].picnum]*sprite[i].yrepeat)<<1);

		if ((sprite[i].sectnum != osectnum) && (g_sector[sprite[i].sectnum].lotag == 10))
			{ warpsprite((short)i); movestat = 0; }

		if ((movestat != 0) || ((k&63) == 1))
		{
			if (sprite[i].ang == (sprite[i].extra&2047))
			{
				daang = (getangle(posx[target]-sprite[i].x,posy[target]-sprite[i].y)&2047);
				daang = ((daang+(krand()&1023)-512)&2047);
				sprite[i].extra = ((sprite[i].extra&(~2047))|daang);
			}
			if ((sprite[i].extra-sprite[i].ang)&1024)
			{
				sprite[i].ang = ((sprite[i].ang-32)&2047);
				if (!((sprite[i].extra-sprite[i].ang)&1024)) sprite[i].ang = (sprite[i].extra&2047);
			}
			else
			{
				sprite[i].ang = ((sprite[i].ang+32)&2047);
				if (((sprite[i].extra-sprite[i].ang)&1024)) sprite[i].ang = (sprite[i].extra&2047);
			}
		}
	}

	for(i=headspritestat[10];i>=0;i=nexti)  //EVILAL list
	{
		nexti = nextspritestat[i];

		if (sprite[i].yrepeat < 38) continue;
		if (sprite[i].yrepeat < 64)
		{
			sprite[i].xrepeat++;
			sprite[i].yrepeat++;
			continue;
		}

		if ((nummoves-i)&statrate[10]) continue;

			//Choose a target player
		mindist = 0x7fffffff; target = connecthead;
		for(p=connecthead;p>=0;p=connectpoint2[p])
		{
			dist = std::abs(sprite[i].x-posx[p])+std::abs(sprite[i].y-posy[p]);
			if (dist < mindist) mindist = dist, target = p;
		}

		k = (krand()&255);

		if ((sprite[i].lotag&32) && (k < 48))  //Al decides to reproduce
		{
			l = 0;
			if ((sprite[i].lotag&64) && (k < 2))  //Give him a chance to reproduce without seeing you
				l = 1;
			else if (cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,posx[target],posy[target],posz[target],cursectnum[target]))
				l = 1;
			if (l != 0)
			{
				spawnsprite(j,sprite[i].x,sprite[i].y,sprite[i].z,sprite[i].cstat,sprite[i].shade,sprite[i].pal,
					sprite[i].clipdist,38,38,sprite[i].xoffset,sprite[i].yoffset,sprite[i].picnum,krand()&2047,0,0,0,i,
					sprite[i].sectnum,10,sprite[i].lotag,sprite[i].hitag,sprite[i].extra);
				switch(krand()&31)  //Mutations!
				{
					case 0: sprite[i].cstat ^= 2; break;
					case 1: sprite[i].cstat ^= 512; break;
					case 2: sprite[i].shade++; break;
					case 3: sprite[i].shade--; break;
					case 4: sprite[i].pal ^= 16; break;
					case 5: case 6: case 7: sprite[i].lotag ^= (1<<(krand()&7)); break;
					case 8: sprite[i].lotag = (krand()&255); break;
				}
			}
		}
		if (k >= 208+((sprite[i].lotag&128)>>2))    //Al decides to shoot bullet
		{
			if (cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,posx[target],posy[target],posz[target],cursectnum[target]))
			{
				wsayfollow("zipguns.wav",5144L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,1);

				spawnsprite(j,sprite[i].x,sprite[i].y,
					g_sector[sprite[i].sectnum].floorz-(24<<8),
					0,0,0,16,32,32,0,0,BULLET,
					(getangle(posx[target]-sprite[j].x,
						posy[target]-sprite[j].y)+(krand()&15)-8)&2047,
					sintable[(sprite[j].ang+512)&2047]>>6,
					sintable[sprite[j].ang&2047]>>6,
					((posz[target]+(8<<8)-sprite[j].z)<<8) /
					  (std::sqrt((posx[target]-sprite[j].x) *
								(posx[target]-sprite[j].x) +
								(posy[target]-sprite[j].y) *
								(posy[target]-sprite[j].y))+1),
								i,sprite[i].sectnum,6,0,0,0);
			}
		}

			//Move Al
		l = (((sprite[i].lotag&3)+2)<<8);
		if (sprite[i].lotag&4) l = -l;
		dax = sintable[(sprite[i].ang+512)&2047]*l;
		day = sintable[sprite[i].ang]*l;

		osectnum = sprite[i].sectnum;
		movestat = movesprite((short)i,dax,day,0L,-(8L<<8),-(8L<<8),CLIPMASK0);
		sprite[i].z = globloz;
		if ((sprite[i].sectnum != osectnum) && (g_sector[sprite[i].sectnum].lotag == 10))
		{
			warpsprite((short)i);
			movestat = 0;
		}

		if (sprite[i].lotag&16)
		{
			if (((k&124) >= 120) && cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,posx[target],posy[target],posz[target],cursectnum[target]))
				sprite[i].ang = getangle(posx[target]-sprite[i].x,posy[target]-sprite[i].y);
			else
				sprite[i].ang = (krand()&2047);
		}

		if (movestat != 0)
		{
			if ((k&2) && cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,posx[target],posy[target],posz[target],cursectnum[target]))
				sprite[i].ang = getangle(posx[target]-sprite[i].x,posy[target]-sprite[i].y);
			else
				sprite[i].ang = (krand()&2047);

			if ((movestat&49152) == 49152)
				if (sprite[movestat&16383].picnum == EVILAL)
					if ((k&31) >= 30)
					{
						wsayfollow("blowup.wav",5144L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
						sprite[i].picnum = EVILALGRAVE;
						sprite[i].cstat = 0;
						sprite[i].xvel = (krand()&255)-128;
						sprite[i].yvel = (krand()&255)-128;
						sprite[i].zvel = (krand()&4095)-3072;
						changespritestat(i,9);
					}

			if (sprite[i].lotag&8)
				if ((k&31) >= 30)
				{
					wsayfollow("blowup.wav",5144L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					sprite[i].picnum = EVILALGRAVE;
					sprite[i].cstat = 0;
					sprite[i].xvel = (krand()&255)-128;
					sprite[i].yvel = (krand()&255)-128;
					sprite[i].zvel = (krand()&4095)-3072;
					changespritestat(i,9);
				}

			if (movestat == -1)
			{
				wsayfollow("blowup.wav",5144L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
				sprite[i].picnum = EVILALGRAVE;
				sprite[i].cstat = 0;
				sprite[i].xvel = (krand()&255)-128;
				sprite[i].yvel = (krand()&255)-128;
				sprite[i].zvel = (krand()&4095)-3072;
				changespritestat(i,9);
			}
		}
	}

		//Go through travelling bullet sprites
	for(i=headspritestat[6];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		if ((nummoves-i)&statrate[6]) continue;

			 //If the sprite is a bullet then...
		if ((sprite[i].picnum == BULLET) || (sprite[i].picnum == GRABBER) || (sprite[i].picnum == MISSILE) || (sprite[i].picnum == BOMB))
		{
			dax = ((((int)sprite[i].xvel)*TICSPERFRAME)<<12);
			day = ((((int)sprite[i].yvel)*TICSPERFRAME)<<12);
			daz = ((((int)sprite[i].zvel)*TICSPERFRAME)>>2);
			if (sprite[i].picnum == BOMB) daz = 0;

			osectnum = sprite[i].sectnum;
			hitobject = movesprite((short)i,dax,day,daz,4L<<8,4L<<8,CLIPMASK1);
			if ((sprite[i].sectnum != osectnum) && (g_sector[sprite[i].sectnum].lotag == 10))
			{
				warpsprite((short)i);
				hitobject = 0;
			}

			if (sprite[i].picnum == GRABBER) {   // Andy did this (& Ken) !Homing!
				checkgrabbertouchsprite(i,sprite[i].sectnum);
				l = 0x7fffffff;
				for (j = connecthead; j >= 0; j = connectpoint2[j])   // Players
					if (j != (sprite[i].owner & (MAXSPRITES - 1)))
						if (cansee(sprite[i].x,sprite[i].y,sprite[i].z,sprite[i].sectnum,posx[j],posy[j],posz[j],cursectnum[j])) {
							k = std::sqrt(sqr(posx[j] - sprite[i].x) + sqr(posy[j] - sprite[i].y) + (sqr(posz[j] - sprite[i].z) >> 8));
							if (k < l) {
								l = k;
								dax = (posx[j] - sprite[i].x);
								day = (posy[j] - sprite[i].y);
								daz = (posz[j] - sprite[i].z);
							}
						}
				for(j = headspritestat[1]; j >= 0; j = nextj) {   // Active monsters
					nextj = nextspritestat[j];
					if (cansee(sprite[i].x,sprite[i].y,sprite[i].z,sprite[i].sectnum,sprite[j].x,sprite[j].y,sprite[j].z,sprite[j].sectnum)) {
						k = std::sqrt(sqr(sprite[j].x - sprite[i].x) + sqr(sprite[j].y - sprite[i].y) + (sqr(sprite[j].z - sprite[i].z) >> 8));
						if (k < l) {
							l = k;
							dax = (sprite[j].x - sprite[i].x);
							day = (sprite[j].y - sprite[i].y);
							daz = (sprite[j].z - sprite[i].z);
						}
					}
				}
				for(j = headspritestat[2]; j >= 0; j = nextj) {   // Inactive monsters
					nextj = nextspritestat[j];
					if (cansee(sprite[i].x,sprite[i].y,sprite[i].z,sprite[i].sectnum,sprite[j].x,sprite[j].y,sprite[j].z,sprite[j].sectnum)) {
						k = std::sqrt(sqr(sprite[j].x - sprite[i].x) + sqr(sprite[j].y - sprite[i].y) + (sqr(sprite[j].z - sprite[i].z) >> 8));
						if (k < l) {
							l = k;
							dax = (sprite[j].x - sprite[i].x);
							day = (sprite[j].y - sprite[i].y);
							daz = (sprite[j].z - sprite[i].z);
						}
					}
				}
				if (l != 0x7fffffff) {
					sprite[i].xvel = (divscalen<7>(dax,l) + sprite[i].xvel);   // 1/5 of velocity is homing, 4/5 is momentum
					sprite[i].yvel = (divscalen<7>(day,l) + sprite[i].yvel);   // 1/5 of velocity is homing, 4/5 is momentum
					sprite[i].zvel = (divscalen<7>(daz,l) + sprite[i].zvel);   // 1/5 of velocity is homing, 4/5 is momentum
					l = std::sqrt((sprite[i].xvel * sprite[i].xvel) + (sprite[i].yvel * sprite[i].yvel) + ((sprite[i].zvel * sprite[i].zvel) >> 8));
					sprite[i].xvel = divscalen<9>(sprite[i].xvel,l);
					sprite[i].yvel = divscalen<9>(sprite[i].yvel,l);
					sprite[i].zvel = divscalen<9>(sprite[i].zvel,l);
					sprite[i].ang = getangle(sprite[i].xvel,sprite[i].yvel);
				}
			}

			if (sprite[i].picnum == BOMB)
			{
				j = sprite[i].sectnum;
				if ((g_sector[j].floorstat&2) && (sprite[i].z > globloz-(8<<8)))
				{
					k = g_sector[j].wallptr;
					daang = getangle(wall[wall[k].point2].pt.x-wall[k].pt.x,wall[wall[k].point2].pt.y-wall[k].pt.y);
					sprite[i].xvel += mulscalen<22>(sintable[(daang+1024)&2047],g_sector[j].floorheinum);
					sprite[i].yvel += mulscalen<22>(sintable[(daang+512)&2047],g_sector[j].floorheinum);
				}
			}

			if (sprite[i].picnum == BOMB)
			{
				sprite[i].z += sprite[i].zvel;
				sprite[i].zvel += (TICSPERFRAME<<7);
				if (sprite[i].z < globhiz+(tilesizy[BOMB]<<6))
				{
					sprite[i].z = globhiz+(tilesizy[BOMB]<<6);
					sprite[i].zvel = -(sprite[i].zvel>>1);
				}
				if (sprite[i].z > globloz-(tilesizy[BOMB]<<6))
				{
					sprite[i].z = globloz-(tilesizy[BOMB]<<6);
					sprite[i].zvel = -(sprite[i].zvel>>1);
				}
				dax = sprite[i].xvel; day = sprite[i].yvel;
				dist = dax*dax+day*day;
				if (dist < 512)
				{
					bombexplode(i);
					goto bulletisdeletedskip;
				}
				if (dist < 4096)
				{
					sprite[i].xrepeat = ((4096+2048)*16) / (dist+2048);
					sprite[i].yrepeat = sprite[i].xrepeat;
					sprite[i].xoffset = (krand()&15)-8;
					sprite[i].yoffset = (krand()&15)-8;
				}
				if (mulscalen<30>(krand(),dist) == 0)
				{
					sprite[i].xvel -= ksgn(sprite[i].xvel);
					sprite[i].yvel -= ksgn(sprite[i].yvel);
					sprite[i].zvel -= ksgn(sprite[i].zvel);
				}
			}

				//Check for bouncy objects before killing bullet
			if ((hitobject&0xc000) == 16384)  //Bullet hit a ceiling/floor
			{
				k = g_sector[hitobject&(MAXSECTORS-1)].wallptr;
				l = wall[k].point2;
				daang = getangle(wall[l].pt.x-wall[k].pt.x,wall[l].pt.y-wall[k].pt.y);
				// both k, l overwritten here
				auto klz = getzsofslope(hitobject&(MAXSECTORS-1),sprite[i].x,sprite[i].y);
				
				if (sprite[i].z < ((klz.ceilz + klz.floorz)>>1))
					klz.ceilz = g_sector[hitobject&(MAXSECTORS-1)].ceilingheinum;
				else
					klz.ceilz = g_sector[hitobject&(MAXSECTORS-1)].floorheinum;

				dax = mulscalen<14>(klz.ceilz, sintable[(daang)&2047]);
				day = mulscalen<14>(klz.ceilz, sintable[(daang+1536)&2047]);
				daz = 4096;
				// end of klz usage
				
				k = sprite[i].xvel*dax+sprite[i].yvel*day+mulscalen<4>(sprite[i].zvel,daz);
				l = dax*dax+day*day+daz*daz;
				if ((std::abs(k)>>14) < l)
				{
					k = divscalen<17>(k,l);
					sprite[i].xvel -= mulscalen<16>(dax,k);
					sprite[i].yvel -= mulscalen<16>(day,k);
					sprite[i].zvel -= mulscalen<12>(daz,k);
				}
				wsayfollow("bouncy.wav",4096L+(krand()&127)-64,255,&sprite[i].x,&sprite[i].y,1);
				hitobject = 0;
				sprite[i].owner = -1;   //Bullet turns evil!
			}
			else if ((hitobject&0xc000) == 32768)  //Bullet hit a wall
			{
				if (wall[hitobject&4095].lotag == 8)
				{
					dax = sprite[i].xvel; day = sprite[i].yvel;
					if ((sprite[i].picnum != BOMB) || (dax*dax+day*day >= 512))
					{
						k = (hitobject&4095); l = wall[k].point2;
						j = getangle(wall[l].pt.x-wall[k].pt.x,wall[l].pt.y-wall[k].pt.y)+512;

							//k = cos(ang) * sin(ang) * 2
						k = mulscalen<13>(sintable[(j+512)&2047],sintable[j&2047]);
							//l = cos(ang * 2)
						l = sintable[((j<<1)+512)&2047];

						ox = sprite[i].xvel; oy = sprite[i].yvel;
						dax = -ox; day = -oy;
						sprite[i].xvel = dmulscalen<14>(day,k,dax,l);
						sprite[i].yvel = dmulscalen<14>(dax,k,-day,l);

						if (sprite[i].picnum == BOMB)
						{
							sprite[i].xvel -= (sprite[i].xvel>>3);
							sprite[i].yvel -= (sprite[i].yvel>>3);
							sprite[i].zvel -= (sprite[i].zvel>>3);
						}
						ox -= sprite[i].xvel; oy -= sprite[i].yvel;
						dist = ((ox*ox+oy*oy)>>8);
						wsayfollow("bouncy.wav", 4096L + (krand() & 127) - 64, std::min(dist, 256), &sprite[i].x, &sprite[i].y, 1);
						hitobject = 0;
						sprite[i].owner = -1;   //Bullet turns evil!
					}
				}
			}
			else if ((hitobject&0xc000) == 49152)  //Bullet hit a sprite
			{
				if (sprite[hitobject&4095].picnum == BOUNCYMAT)
				{
					if ((sprite[hitobject&4095].cstat&48) == 0)
					{
						sprite[i].xvel = -sprite[i].xvel;
						sprite[i].yvel = -sprite[i].yvel;
						sprite[i].zvel = -sprite[i].zvel;
						dist = 255;
					}
					else if ((sprite[hitobject&4095].cstat&48) == 16)
					{
						j = sprite[hitobject&4095].ang;

							//k = cos(ang) * sin(ang) * 2
						k = mulscalen<13>(sintable[(j+512)&2047],sintable[j&2047]);
							//l = cos(ang * 2)
						l = sintable[((j<<1)+512)&2047];

						ox = sprite[i].xvel; oy = sprite[i].yvel;
						dax = -ox; day = -oy;
						sprite[i].xvel = dmulscalen<14>(day,k,dax,l);
						sprite[i].yvel = dmulscalen<14>(dax,k,-day,l);

						ox -= sprite[i].xvel; oy -= sprite[i].yvel;
						dist = ((ox*ox+oy*oy)>>8);
					}
					sprite[i].owner = -1;   //Bullet turns evil!
					wsayfollow("bouncy.wav", 4096L + (krand() & 127) - 64, std::min(dist, 256), &sprite[i].x, &sprite[i].y, 1);
					hitobject = 0;
				}
			}

			if (hitobject != 0)
			{
				if ((sprite[i].picnum == MISSILE) || (sprite[i].picnum == BOMB))
				{
					if ((hitobject&0xc000) == 49152)
						if (sprite[hitobject&4095].lotag == 5)  //Basketball hoop
						{
							wsayfollow("niceshot.wav",3840L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
							deletesprite((short)i);
							goto bulletisdeletedskip;
						}

					bombexplode(i);
					goto bulletisdeletedskip;
				}

				if ((hitobject&0xc000) == 16384)  //Hits a ceiling / floor
				{
					wsayfollow("bullseye.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
					deletesprite((short)i);
					goto bulletisdeletedskip;
				}
				else if ((hitobject&0xc000) == 32768)  //Bullet hit a wall
				{
					if (wall[hitobject&4095].picnum == KENPICTURE)
					{
						if (waloff[MAXTILES-1] != 0)
							wall[hitobject&4095].picnum = MAXTILES-1;
						wsayfollow("hello.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);   //Ken says, "Hello... how are you today!"
					}
					else
						wsayfollow("bullseye.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);

					deletesprite((short)i);
					goto bulletisdeletedskip;
				}
				else if ((hitobject&0xc000) == 49152)  //Bullet hit a sprite
				{
					if ((sprite[hitobject&4095].lotag == 5) && (sprite[i].picnum == GRABBER)) {  // Basketball hoop (Andy's addition)
						wsayfollow("niceshot.wav",3840L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
						switch (krand() & 63) {
							case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
								sprite[i].picnum = COIN; break;
							case 10: case 11: case 12: case 13: case 14: case 15: case 16:
								sprite[i].picnum = DIAMONDS; break;
							case 17: case 18: case 19:
								sprite[i].picnum = COINSTACK; break;
							case 20: case 21: case 22: case 23:
								sprite[i].picnum = GIFTBOX; break;
							case 24: case 25:
								sprite[i].picnum = GRABCANNON; break;
							case 26: case 27:
								sprite[i].picnum = LAUNCHER; break;
							case 28: case 29: case 30:
								sprite[i].picnum = CANNON; break;
							case 31:
								sprite[i].picnum = AIRPLANE; break;
							default:
								deletesprite((short)i);
								goto bulletisdeletedskip;
						}
						sprite[i].xvel = sprite[i].yvel = sprite[i].zvel = 0;
						sprite[i].cstat &= ~0x83;    //Should not clip, foot-z
						changespritestat(i,12);
						goto bulletisdeletedskip;
					}

						//Check if bullet hit a player & find which player it was...
					if (sprite[hitobject&4095].picnum == PLAYER)
						for(j=connecthead;j>=0;j=connectpoint2[j])
							if (sprite[i].owner != j+4096)
								if (playersprite[j] == (hitobject&4095))
								{
									wsayfollow("ouch.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
									if (sprite[i].picnum == GRABBER) {   // Andy did this
										k = ((sprite[i].xrepeat * sprite[i].yrepeat) * 3) >> 9;
										changehealth((sprite[i].owner - 4096),k);
										changehealth(j,-k);
									}
									else changehealth(j,-mulscalen<8>(sprite[i].xrepeat,sprite[i].yrepeat));
									deletesprite((short)i);
									goto bulletisdeletedskip;
								}

						//Check if bullet hit any monsters...
					j = (hitobject&4095);     //j is the spritenum that the bullet (spritenum i) hit
					if (sprite[i].owner != j)
					{
						switch(sprite[j].picnum)
						{
							case BROWNMONSTER:
								if (sprite[j].lotag > 0) {
									if (sprite[i].picnum == GRABBER) {   // Andy did this
										k = ((sprite[i].xrepeat * sprite[i].yrepeat) * 3) >> 9;
										changehealth((sprite[i].owner - 4096),k);
										sprite[j].lotag -= k;
									}
									sprite[j].lotag -= mulscalen<8>(sprite[i].xrepeat,sprite[i].yrepeat);
								}
								if (sprite[j].lotag > 0)
								{
									if (sprite[j].lotag <= 25) sprite[j].cstat |= 2;
									wsayfollow("hurt.wav",4096L+(krand()&511)-256,256L,&sprite[i].x,&sprite[i].y,1);
								}
								else
								{
									wsayfollow("mondie.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
									sprite[j].z += ((tilesizy[sprite[j].picnum]*sprite[j].yrepeat)<<1);
									sprite[j].picnum = GIFTBOX;
									sprite[j].cstat &= ~0x83;    //Should not clip, foot-z

									spawnsprite(k,sprite[j].x,sprite[j].y,sprite[j].z,
										0,-4,0,32,64,64,0,0,EXPLOSION,sprite[j].ang,
										0,0,0,j,sprite[j].sectnum,5,31,0,0);
											//31=Time left for explosion to stay

									changespritestat(j,12);
								}
								deletesprite((short)i);
								goto bulletisdeletedskip;
							case EVILAL:
								wsayfollow("blowup.wav",5144L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
								sprite[j].picnum = EVILALGRAVE;
								sprite[j].cstat = 0;
								sprite[j].xvel = (krand()&255)-128;
								sprite[j].yvel = (krand()&255)-128;
								sprite[j].zvel = (krand()&4095)-3072;
								changespritestat(j,9);

								deletesprite((short)i);
								goto bulletisdeletedskip;
							case AL:
								wsayfollow("blowup.wav",5144L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
								sprite[j].xrepeat += 2;
								sprite[j].yrepeat += 2;
								if (sprite[j].yrepeat >= 38)
								{
									sprite[j].picnum = EVILAL;
									//sprite[j].cstat |= 2;      //Make him transluscent
									changespritestat(j,10);
								}
								deletesprite((short)i);
								goto bulletisdeletedskip;
							default:
								wsayfollow("bullseye.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
								deletesprite((short)i);
								goto bulletisdeletedskip;
						}
					}
				}
			}
		}
bulletisdeletedskip: continue;
	}

		//Go through monster waiting for you list
	for(i=headspritestat[2];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		if ((nummoves-i)&15) continue;

			//Use dot product to see if monster's angle is towards a player
		for(p=connecthead;p>=0;p=connectpoint2[p])
			if (sintable[(sprite[i].ang+512)&2047]*(posx[p]-sprite[i].x) + sintable[sprite[i].ang&2047]*(posy[p]-sprite[i].y) >= 0)
				if (cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,posx[p],posy[p],posz[p],cursectnum[p]))
				{
					changespritestat(i,1);
					//if (sprite[i].lotag == 100)
					//{
						wsayfollow("iseeyou.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,1);
					//   sprite[i].lotag = 99;
					//}
				}
	}

		//Go through smoke sprites
	for(i=headspritestat[3];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		sprite[i].z -= (TICSPERFRAME<<6);
		sprite[i].lotag -= TICSPERFRAME;
		if (sprite[i].lotag < 0) deletesprite(i);
	}

		//Go through splash sprites
	for(i=headspritestat[4];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		sprite[i].lotag -= TICSPERFRAME;
		sprite[i].picnum = SPLASH + ((63-sprite[i].lotag)>>4);
		if (sprite[i].lotag < 0) deletesprite(i);
	}

		//Go through explosion sprites
	for(i=headspritestat[5];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		sprite[i].lotag -= TICSPERFRAME;
		if (sprite[i].lotag < 0) deletesprite(i);
	}

		//Go through bomb spriral-explosion sprites
	for(i=headspritestat[7];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		sprite[i].xrepeat = (sprite[i].lotag>>2);
		sprite[i].yrepeat = (sprite[i].lotag>>2);
		sprite[i].lotag -= (TICSPERFRAME<<2);
		if (sprite[i].lotag < 0) { deletesprite(i); continue; }

		if ((nummoves-i)&statrate[7]) continue;

		sprite[i].x += ((sprite[i].xvel*TICSPERFRAME)>>2);
		sprite[i].y += ((sprite[i].yvel*TICSPERFRAME)>>2);
		sprite[i].z += ((sprite[i].zvel*TICSPERFRAME)>>2);

		sprite[i].zvel += (TICSPERFRAME<<9);
		if (sprite[i].z < g_sector[sprite[i].sectnum].ceilingz+(4<<8))
		{
			sprite[i].z = g_sector[sprite[i].sectnum].ceilingz+(4<<8);
			sprite[i].zvel = -(sprite[i].zvel>>1);
		}
		if (sprite[i].z > g_sector[sprite[i].sectnum].floorz-(4<<8))
		{
			sprite[i].z = g_sector[sprite[i].sectnum].floorz-(4<<8);
			sprite[i].zvel = -(sprite[i].zvel>>1);
		}
	}

		//EVILALGRAVE shrinking list
	for(i=headspritestat[9];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		sprite[i].xrepeat = (sprite[i].lotag>>2);
		sprite[i].yrepeat = (sprite[i].lotag>>2);
		sprite[i].lotag -= TICSPERFRAME;
		if (sprite[i].lotag < 0) { deletesprite(i); continue; }

		if ((nummoves-i)&statrate[9]) continue;

		sprite[i].x += (sprite[i].xvel*TICSPERFRAME);
		sprite[i].y += (sprite[i].yvel*TICSPERFRAME);
		sprite[i].z += (sprite[i].zvel*TICSPERFRAME);

		sprite[i].zvel += (TICSPERFRAME<<8);
		if (sprite[i].z < g_sector[sprite[i].sectnum].ceilingz)
		{
			sprite[i].z = g_sector[sprite[i].sectnum].ceilingz;
			sprite[i].xvel -= (sprite[i].xvel>>2);
			sprite[i].yvel -= (sprite[i].yvel>>2);
			sprite[i].zvel = -(sprite[i].zvel>>1);
		}
		if (sprite[i].z > g_sector[sprite[i].sectnum].floorz)
		{
			sprite[i].z = g_sector[sprite[i].sectnum].floorz;
			sprite[i].xvel -= (sprite[i].xvel>>2);
			sprite[i].yvel -= (sprite[i].yvel>>2);
			sprite[i].zvel = -(sprite[i].zvel>>1);
		}
	}

		//Re-spawning sprite list
	for(i=headspritestat[11];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];

		sprite[i].extra -= TICSPERFRAME;
		if (sprite[i].extra < 0)
		{
			wsayfollow("warp.wav",6144L+(krand()&127)-64,128L,&sprite[i].x,&sprite[i].y,0);
			sprite[i].cstat &= ~0x8000;
			sprite[i].extra = -1;
			changespritestat((short)i,0);
		}
	}
}

void activatehitag(short dahitag)
{
	int i;
	int nexti;

	for(i=0;i<numsectors;i++)
		if (g_sector[i].hitag == dahitag) operatesector(i);

	for(i=headspritestat[0];i>=0;i=nexti)
	{
		nexti = nextspritestat[i];
		if (sprite[i].hitag == dahitag) operatesprite(i);
	}
}

void bombexplode(int i)
{
	int j;
	int nextj;
	int k;
	int daang;
	int dax;
	int day;
	int dist;

	spawnsprite(j,sprite[i].x,sprite[i].y,sprite[i].z,0,-4,0,
		32,64,64,0,0,EXPLOSION,sprite[i].ang,
		0,0,0,sprite[i].owner,sprite[i].sectnum,5,31,0,0);
		  //31=Time left for explosion to stay

	for(k=0;k<12;k++)
	{
		spawnsprite(j,sprite[i].x,sprite[i].y,sprite[i].z+(8<<8),2,-4,0,
			32,24,24,0,0,EXPLOSION,sprite[i].ang,
			(krand()>>7)-256,(krand()>>7)-256,(krand()>>2)-8192,
			sprite[i].owner,sprite[i].sectnum,7,96,0,0);
				//96=Time left for smoke to be alive
	}

	for(j=connecthead;j>=0;j=connectpoint2[j])
	{
		dist = (posx[j]-sprite[i].x)*(posx[j]-sprite[i].x);
		dist += (posy[j]-sprite[i].y)*(posy[j]-sprite[i].y);
		dist += ((posz[j]-sprite[i].z)>>4)*((posz[j]-sprite[i].z)>>4);
		if (dist < 4194304)
			if (cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,posx[j],posy[j],posz[j],cursectnum[j]))
			{
				k = ((32768/((dist>>16)+4))>>5);
				if (j == myconnectindex)
				{
					daang = getangle(posx[j]-sprite[i].x,posy[j]-sprite[i].y);
					dax = ((k*sintable[(daang+512)&2047])>>14);
					day = ((k*sintable[daang&2047])>>14);
					fvel += ((dax*sintable[(ang[j]+512)&2047]+day*sintable[ang[j]&2047])>>14);
					svel += ((day*sintable[(ang[j]+512)&2047]-dax*sintable[ang[j]&2047])>>14);
				}
				changehealth(j,-k);    //if changehealth returns 1, you're dead
			}
	}

	for(k=1;k<=2;k++)         //Check for hurting monsters
	{
		for(j=headspritestat[k];j>=0;j=nextj)
		{
			nextj = nextspritestat[j];

			dist = (sprite[j].x-sprite[i].x)*(sprite[j].x-sprite[i].x);
			dist += (sprite[j].y-sprite[i].y)*(sprite[j].y-sprite[i].y);
			dist += ((sprite[j].z-sprite[i].z)>>4)*((sprite[j].z-sprite[i].z)>>4);
			if (dist >= 4194304) continue;
			if (!cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,sprite[j].x,sprite[j].y,sprite[j].z-(tilesizy[sprite[j].picnum]<<7),sprite[j].sectnum))
				continue;

			if (sprite[j].picnum == BROWNMONSTER)
			{
				sprite[j].z += ((tilesizy[sprite[j].picnum]*sprite[j].yrepeat)<<1);
				sprite[j].picnum = GIFTBOX;
				sprite[j].cstat &= ~0x83;    //Should not clip, foot-z
				changespritestat(j,12);
			}
		}
	}

	for(j=headspritestat[10];j>=0;j=nextj)   //Check for EVILAL's
	{
		nextj = nextspritestat[j];

		dist = (sprite[j].x-sprite[i].x)*(sprite[j].x-sprite[i].x);
		dist += (sprite[j].y-sprite[i].y)*(sprite[j].y-sprite[i].y);
		dist += ((sprite[j].z-sprite[i].z)>>4)*((sprite[j].z-sprite[i].z)>>4);
		if (dist >= 4194304) continue;
		if (!cansee(sprite[i].x,sprite[i].y,sprite[i].z-(tilesizy[sprite[i].picnum]<<7),sprite[i].sectnum,sprite[j].x,sprite[j].y,sprite[j].z-(tilesizy[sprite[j].picnum]<<7),sprite[j].sectnum))
			continue;

		sprite[j].picnum = EVILALGRAVE;
		sprite[j].cstat = 0;
		sprite[j].xvel = (krand()&255)-128;
		sprite[j].yvel = (krand()&255)-128;
		sprite[j].zvel = (krand()&4095)-3072;
		changespritestat(j,9);
	}

	wsayfollow("blowup.wav",3840L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,0);
	deletesprite((short)i);
}

void processinput(short snum)
{
	int i;
	int j;
	int k;
	int doubvel;
	int xvect;
	int yvect;
	int goalz;
	int dax;
	int day;

		//SHARED KEYS:
		//Movement code
	if ((ssync[snum].fvel|ssync[snum].svel) != 0)
	{
		doubvel = (TICSPERFRAME << static_cast<int>((ssync[snum].bits & 256) > 0));

		xvect = 0, yvect = 0;
		if (ssync[snum].fvel != 0)
		{
			xvect += ((((int)ssync[snum].fvel)*doubvel*(int)sintable[(ang[snum]+512)&2047])>>3);
			yvect += ((((int)ssync[snum].fvel)*doubvel*(int)sintable[ang[snum]&2047])>>3);
		}
		if (ssync[snum].svel != 0)
		{
			xvect += ((((int)ssync[snum].svel)*doubvel*(int)sintable[ang[snum]&2047])>>3);
			yvect += ((((int)ssync[snum].svel)*doubvel*(int)sintable[(ang[snum]+1536)&2047])>>3);
		}
		if (flytime[snum] > lockclock) { xvect += xvect; yvect += yvect; }   // DOuble flying speed
		clipmove(&posx[snum],&posy[snum],&posz[snum],&cursectnum[snum],xvect,yvect,128L,4<<8,4<<8,CLIPMASK0);
		revolvedoorstat[snum] = 1;
	}
	else
	{
		revolvedoorstat[snum] = 0;
	}

	sprite[playersprite[snum]].cstat &= ~1;
		//Push player away from walls if clipmove doesn't work
	if (pushmove(&posx[snum],&posy[snum],&posz[snum],&cursectnum[snum],128L,4<<8,4<<8,CLIPMASK0) < 0)
		changehealth(snum,-1000);  //If this screws up, then instant death!!!

			// Getzrange returns the highest and lowest z's for an entire box,
			// NOT just a point.  This prevents you from falling off cliffs
			// when you step only slightly over the cliff.
	getzrange(posx[snum],posy[snum],posz[snum],cursectnum[snum],&globhiz,&globhihit,&globloz,&globlohit,128L,CLIPMASK0);
	sprite[playersprite[snum]].cstat |= 1;

	if (ssync[snum].avel != 0)          //ang += avel * constant
	{                         //ENGINE calculates avel for you
		doubvel = TICSPERFRAME;
		if ((ssync[snum].bits&256) > 0)  //Lt. shift makes turn velocity 50% faster
			doubvel += (TICSPERFRAME>>1);
		ang[snum] += ((((int)ssync[snum].avel)*doubvel)>>4);
		ang[snum] &= 2047;
	}

	if (health[snum] < 0)
	{
		health[snum] -= TICSPERFRAME;
		if (health[snum] <= -160)
		{
			hvel[snum] = 0;
			if (snum == myconnectindex)
				fvel = 0, svel = 0, avel = 0, keystatus[3] = 1;

			deaths[snum]++;
			health[snum] = 100;
			numbombs[snum] = 0;
			numgrabbers[snum] = 0;
			nummissiles[snum] = 0;
			flytime[snum] = 0;

			findrandomspot(&posx[snum],&posy[snum],&cursectnum[snum]);
			posz[snum] = getflorzofslope(cursectnum[snum],posx[snum],posy[snum])-(1<<8);
			horiz[snum] = 100;
			ang[snum] = (krand()&2047);

			sprite[playersprite[snum]].x = posx[snum];
			sprite[playersprite[snum]].y = posy[snum];
			sprite[playersprite[snum]].z = posz[snum]+EYEHEIGHT;
			sprite[playersprite[snum]].picnum = PLAYER;
			sprite[playersprite[snum]].ang = ang[snum];
			sprite[playersprite[snum]].xrepeat = 64;
			sprite[playersprite[snum]].yrepeat = 64;
			changespritesect(playersprite[snum],cursectnum[snum]);

			drawstatusbar(snum);   // Andy did this

			i = playersprite[snum];
			wsayfollow("zipguns.wav",4096L+(krand()&127)-64,256L,&sprite[i].x,&sprite[i].y,1);
			for(k=0;k<16;k++)
			{
				spawnsprite(j,sprite[i].x,sprite[i].y,sprite[i].z+(8<<8),2,-4,0,
					32,24,24,0,0,EXPLOSION,sprite[i].ang,
					(krand()&511)-256,(krand()&511)-256,(krand()&16384)-8192,
					sprite[i].owner,sprite[i].sectnum,7,96,0,0);
						//96=Time left for smoke to be alive
			}
		}
		else
		{
			sprite[playersprite[snum]].xrepeat = std::max(((128 + health[snum]) >> 1), 0);
			sprite[playersprite[snum]].yrepeat = std::max(((128 + health[snum]) >> 1), 0);

			hvel[snum] += (TICSPERFRAME<<2);
			horiz[snum] = std::max(horiz[snum] - 4, 0);
			posz[snum] += hvel[snum];
			if (posz[snum] > globloz-(4<<8))
			{
				posz[snum] = globloz-(4<<8);
				horiz[snum] = std::min(horiz[snum] + 5, 200);
				hvel[snum] = 0;
			}
		}
	}

	if (((ssync[snum].bits&8) > 0) && (horiz[snum] > 100-(200>>1))) horiz[snum] -= 4;     //-
	if (((ssync[snum].bits&4) > 0) && (horiz[snum] < 100+(200>>1))) horiz[snum] += 4;   //+

	goalz = globloz-EYEHEIGHT;
	if (g_sector[cursectnum[snum]].lotag == 4)   //slime sector
		if ((globlohit&0xc000) != 49152)            //You're not on a sprite
		{
			goalz = globloz-(8<<8);
			if (posz[snum] >= goalz-(2<<8))
			{
				clipmove(&posx[snum],&posy[snum],&posz[snum],&cursectnum[snum],-(TICSPERFRAME<<14),-(TICSPERFRAME<<14),128L,4<<8,4<<8,CLIPMASK0);

				if (slimesoundcnt[snum] >= 0)
				{
					slimesoundcnt[snum] -= TICSPERFRAME;
					while (slimesoundcnt[snum] < 0)
					{
						slimesoundcnt[snum] += 120;
						wsayfollow("slime.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
					}
				}
			}
		}
	if (goalz < globhiz+(16<<8))   //ceiling&floor too close
		goalz = ((globloz+globhiz)>>1);
	//goalz += mousz;
	if (health[snum] >= 0)
	{
		if ((ssync[snum].bits&1) > 0)                         //A (stand high)
		{
			if (flytime[snum] <= lockclock)
			{
				if (posz[snum] >= globloz-(32<<8))
				{
					goalz -= (16<<8);
					if (ssync[snum].bits&256) goalz -= (24<<8);
				}
			}
			else
			{
				hvel[snum] -= 192;
				if (ssync[snum].bits&256) hvel[snum] -= 192;
			}
		}
		if ((ssync[snum].bits&2) > 0)                         //Z (stand low)
		{
			if (flytime[snum] <= lockclock)
			{
				goalz += (12<<8);
				if (ssync[snum].bits&256) goalz += (12<<8);
			}
			else
			{
				hvel[snum] += 192;
				if (ssync[snum].bits&256) hvel[snum] += 192;
			}
		}
	}

	if (flytime[snum] <= lockclock)
	{
		if (posz[snum] < goalz)
			hvel[snum] += (TICSPERFRAME<<4);
		else
			hvel[snum] = (((goalz-posz[snum])*TICSPERFRAME)>>5);
	}
	else
	{
		hvel[snum] -= (hvel[snum]>>2);
		hvel[snum] -= ksgn(hvel[snum]);
	}

	posz[snum] += hvel[snum];
	if (posz[snum] > globloz-(4<<8)) posz[snum] = globloz-(4<<8), hvel[snum] = 0;
	if (posz[snum] < globhiz+(4<<8)) posz[snum] = globhiz+(4<<8), hvel[snum] = 0;

	if (dimensionmode[snum] != 3)
	{
		if (((ssync[snum].bits&32) > 0) && (zoom[snum] > 48)) zoom[snum] -= (zoom[snum]>>4);
		if (((ssync[snum].bits&16) > 0) && (zoom[snum] < 4096)) zoom[snum] += (zoom[snum]>>4);
	}

		//Update sprite representation of player
		//   -should be after movement, but before shooting code
	setsprite(playersprite[snum],posx[snum],posy[snum],posz[snum]+EYEHEIGHT);
	sprite[playersprite[snum]].ang = ang[snum];

	if (health[snum] >= 0)
	{
		if ((cursectnum[snum] < 0) || (cursectnum[snum] >= numsectors))
		{       //How did you get in the wrong sector?
			wsayfollow("ouch.wav",4096L+(krand()&127)-64,64L,&posx[snum],&posy[snum],1);
			changehealth(snum,-TICSPERFRAME);
		}
		else if (globhiz+(8<<8) > globloz)
		{       //Ceiling and floor are smooshing you!
			wsayfollow("ouch.wav",4096L+(krand()&127)-64,64L,&posx[snum],&posy[snum],1);
			changehealth(snum,-TICSPERFRAME);
		}
	}

	if ((waterfountainwall[snum] >= 0) && (health[snum] >= 0))
		if ((wall[neartagwall].lotag != 7) || ((ssync[snum].bits&1024) == 0))
		{
			i = waterfountainwall[snum];
			if (wall[i].overpicnum == USEWATERFOUNTAIN)
				wall[i].overpicnum = WATERFOUNTAIN;
			else if (wall[i].picnum == USEWATERFOUNTAIN)
				wall[i].picnum = WATERFOUNTAIN;

			waterfountainwall[snum] = -1;
		}

	if ((ssync[snum].bits&1024) > 0)  //Space bar
	{
			//Continuous triggers...

		neartag(posx[snum],posy[snum],posz[snum],cursectnum[snum],ang[snum],&neartagsector,&neartagwall,&neartagsprite,&neartaghitdist,1024L,3);
		if (neartagsector == -1)
		{
			i = cursectnum[snum];
			if ((g_sector[i].lotag|g_sector[i].hitag) != 0)
				neartagsector = i;
		}

		if (wall[neartagwall].lotag == 7)  //Water fountain
		{
			if (wall[neartagwall].overpicnum == WATERFOUNTAIN)
			{
				wsayfollow("water.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
				wall[neartagwall].overpicnum = USEWATERFOUNTAIN;
				waterfountainwall[snum] = neartagwall;
			}
			else if (wall[neartagwall].picnum == WATERFOUNTAIN)
			{
				wsayfollow("water.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
				wall[neartagwall].picnum = USEWATERFOUNTAIN;
				waterfountainwall[snum] = neartagwall;
			}

			if (waterfountainwall[snum] >= 0)
			{
				waterfountaincnt[snum] -= TICSPERFRAME;
				while (waterfountaincnt[snum] < 0)
				{
					waterfountaincnt[snum] += 120;
					wsayfollow("water.wav",4096L+(krand()&127)-64,256L,&posx[snum],&posy[snum],1);
					changehealth(snum,2);
				}
			}
		}

			//1-time triggers...
		if ((oflags[snum]&1024) == 0)
		{
			if (neartagsector >= 0)
				if (g_sector[neartagsector].hitag == 0)
					operatesector(neartagsector);

			if (neartagwall >= 0)
				if (wall[neartagwall].lotag == 2)  //Switch
				{
					activatehitag(wall[neartagwall].hitag);

					j = wall[neartagwall].overpicnum;
					if (j == SWITCH1ON)                     //1-time switch
					{
						wall[neartagwall].overpicnum = GIFTBOX;
						wall[neartagwall].lotag = 0;
						wall[neartagwall].hitag = 0;
					}
					if (j == GIFTBOX)                       //1-time switch
					{
						wall[neartagwall].overpicnum = SWITCH1ON;
						wall[neartagwall].lotag = 0;
						wall[neartagwall].hitag = 0;
					}
					if (j == SWITCH2ON) wall[neartagwall].overpicnum = SWITCH2OFF;
					if (j == SWITCH2OFF) wall[neartagwall].overpicnum = SWITCH2ON;
					if (j == SWITCH3ON) wall[neartagwall].overpicnum = SWITCH3OFF;
					if (j == SWITCH3OFF) wall[neartagwall].overpicnum = SWITCH3ON;

					i = wall[neartagwall].point2;
					dax = ((wall[neartagwall].pt.x+wall[i].pt.x)>>1);
					day = ((wall[neartagwall].pt.y+wall[i].pt.y)>>1);
					wsayfollow("switch.wav", 4096L + (krand() & 255) - 128, 256L, &dax, &day, 0);
				}

			if (neartagsprite >= 0)
			{
				if (sprite[neartagsprite].lotag == 1)
				{  //if you're shoving innocent little AL around, he gets mad!
					if (sprite[neartagsprite].picnum == AL)
					{
						sprite[neartagsprite].picnum = EVILAL;
						sprite[neartagsprite].cstat |= 2;   //Make him transluscent
						sprite[neartagsprite].xrepeat = 38;
						sprite[neartagsprite].yrepeat = 38;
						changespritestat(neartagsprite,10);
					}
				}
				if (sprite[neartagsprite].lotag == 4)
				{
					activatehitag(sprite[neartagsprite].hitag);

					j = sprite[neartagsprite].picnum;
					if (j == SWITCH1ON)                     //1-time switch
					{
						sprite[neartagsprite].picnum = GIFTBOX;
						sprite[neartagsprite].lotag = 0;
						sprite[neartagsprite].hitag = 0;
					}
					if (j == GIFTBOX)                       //1-time switch
					{
						sprite[neartagsprite].picnum = SWITCH1ON;
						sprite[neartagsprite].lotag = 0;
						sprite[neartagsprite].hitag = 0;
					}
					if (j == SWITCH2ON) sprite[neartagsprite].picnum = SWITCH2OFF;
					if (j == SWITCH2OFF) sprite[neartagsprite].picnum = SWITCH2ON;
					if (j == SWITCH3ON) sprite[neartagsprite].picnum = SWITCH3OFF;
					if (j == SWITCH3OFF) sprite[neartagsprite].picnum = SWITCH3ON;

					dax = sprite[neartagsprite].x;
					day = sprite[neartagsprite].y;
					wsayfollow("switch.wav", 4096L + (krand() & 255) - 128, 256L, &dax, &day, 0);
				}
			}
		}
	}

	if ((ssync[snum].bits & 2048) > 0) {   // Shoot a bullet
		if ((numbombs[snum] == 0) && (((ssync[snum].bits >> 13) & 7) == 2) && (myconnectindex == snum))
			locselectedgun = 0;
		if ((nummissiles[snum] == 0) && (((ssync[snum].bits >> 13) & 7) == 3) && (myconnectindex == snum))
			locselectedgun = 1;
		if ((numgrabbers[snum] == 0) && (((ssync[snum].bits >> 13) & 7) == 4) && (myconnectindex == snum))
			locselectedgun = 1;

		if ((health[snum] >= 0) || ((krand() & 127) > -health[snum]))
			switch((ssync[snum].bits >> 13) & 7) {
				case 0:
					if (lockclock > lastchaingun[snum]+8) {
						lastchaingun[snum] = lockclock;
						shootgun(snum,posx[snum],posy[snum],posz[snum],ang[snum],horiz[snum],cursectnum[snum],0);
					}
					break;
				case 1:
					if ((oflags[snum] & 2048) == 0)
						shootgun(snum,posx[snum],posy[snum],posz[snum],ang[snum],horiz[snum],cursectnum[snum],1);
					break;
				case 2:
					if ((oflags[snum] & 2048) == 0)
						if (numbombs[snum] > 0) {
							shootgun(snum,posx[snum],posy[snum],posz[snum],ang[snum],horiz[snum],cursectnum[snum],2);
							changenumbombs(snum,-1);
						}
					break;
				case 3:
					if ((oflags[snum] & 2048) == 0)
						if (nummissiles[snum] > 0) {
							shootgun(snum,posx[snum],posy[snum],posz[snum],ang[snum],horiz[snum],cursectnum[snum],3);
							changenummissiles(snum,-1);
						}
					break;
				case 4:
					if ((oflags[snum] & 2048) == 0)
						if (numgrabbers[snum] > 0) {
							shootgun(snum,posx[snum],posy[snum],posz[snum],ang[snum],horiz[snum],cursectnum[snum],4);
							changenumgrabbers(snum,-1);
						}
					break;
			}
	}

	if ((ssync[snum].bits&4096) > (oflags[snum]&4096))  //Keypad enter
	{
		dimensionmode[snum]++;
		if (dimensionmode[snum] > 3) dimensionmode[snum] = 1;
	}

	oflags[snum] = ssync[snum].bits;
}

void view(short snum, int *vx, int *vy, int *vz, short *vsectnum, short ang, int horiz)
{
	spritetype *sp;
	int i;
	int nx;
	int ny;
	int nz;
	int hx;
	int hy;
	int hitx;
	int hity;
	int hitz;
	short bakcstat;
	short hitsect;
	short hitwall;
	short hitsprite;
	short daang;

	nx = (sintable[(ang+1536)&2047]>>4);
	ny = (sintable[(ang+1024)&2047]>>4);
	nz = (horiz-100)*128;

	sp = &sprite[snum];

	bakcstat = sp->cstat;
	sp->cstat &= (short)~0x101;

	updatesectorz(*vx,*vy,*vz,vsectnum);
	hitscan(*vx,*vy,*vz,*vsectnum,nx,ny,nz,&hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,CLIPMASK1);
	hx = hitx-(*vx); hy = hity-(*vy);
	if (std::abs(nx)+std::abs(ny) > std::abs(hx)+std::abs(hy))
	{
		*vsectnum = hitsect;
		if (hitwall >= 0)
		{
			daang = getangle(wall[wall[hitwall].point2].pt.x-wall[hitwall].pt.x,
				  wall[wall[hitwall].point2].pt.y-wall[hitwall].pt.y);

			i = nx*sintable[daang]+ny*sintable[(daang+1536)&2047];
			if (std::abs(nx) > std::abs(ny)) hx -= mulscalen<28>(nx,i);
			else hy -= mulscalen<28>(ny,i);
		}
		else if (hitsprite < 0)
		{
			if (std::abs(nx) > std::abs(ny)) hx -= (nx>>5);
			else hy -= (ny>>5);
		}
		if (std::abs(nx) > std::abs(ny)) i = divscalen<16>(hx,nx);
		else i = divscalen<16>(hy,ny);
		if (i < cameradist) cameradist = i;
	}
	*vx = (*vx)+mulscalen<16>(nx,cameradist);
	*vy = (*vy)+mulscalen<16>(ny,cameradist);
	*vz = (*vz)+mulscalen<16>(nz,cameradist);

	updatesectorz(*vx,*vy,*vz,vsectnum);

	sp->cstat = bakcstat;
}

void drawscreen(short snum, int dasmoothratio)
{
	int k=0;
	int l;
	int charsperline;
	int tempint;
	int x1;
	int y1;
	int x2;
	int y2;
	int ox1;
	int oy1;
	int ox2;
	int oy2;
	int dist;
	int maxdist;
	int cposx;
	int cposy;
	int cposz;
	int choriz;
	int czoom;
	int tposx;
	int tposy;
	int tiltlock;
	int *intptr;
	int ovisibility;
	int oparallaxvisibility;
	short cang;
	short tang;
	short csect;
	unsigned char ch;
	unsigned char *ptr;
	unsigned char *ptr2;
	unsigned char *ptr3;
	unsigned char *ptr4;

	smoothratio = std::max(std::min(dasmoothratio, 65536), 0);

	dointerpolations();

	if ((snum == myconnectindex) && ((networkmode == 1) || (myconnectindex != connecthead)))
	{
		cposx = omyx+mulscalen<16>(myx-omyx,smoothratio);
		cposy = omyy+mulscalen<16>(myy-omyy,smoothratio);
		cposz = omyz+mulscalen<16>(myz-omyz,smoothratio);
		choriz = omyhoriz+mulscalen<16>(myhoriz-omyhoriz,smoothratio);
		cang = omyang+mulscalen<16>((int)(((myang+1024-omyang)&2047)-1024),smoothratio);
	}
	else
	{
		cposx = oposx[snum]+mulscalen<16>(posx[snum]-oposx[snum],smoothratio);
		cposy = oposy[snum]+mulscalen<16>(posy[snum]-oposy[snum],smoothratio);
		cposz = oposz[snum]+mulscalen<16>(posz[snum]-oposz[snum],smoothratio);
		choriz = ohoriz[snum]+mulscalen<16>(horiz[snum]-ohoriz[snum],smoothratio);
		cang = oang[snum]+mulscalen<16>(((ang[snum]+1024-oang[snum])&2047)-1024,smoothratio);
	}
	czoom = ozoom[snum]+mulscalen<16>(zoom[snum]-ozoom[snum],smoothratio);

	setears(cposx,cposy,(int)sintable[(cang+512)&2047]<<14,(int)sintable[cang&2047]<<14);

	if (dimensionmode[myconnectindex] == 3)
	{
		bool apply{false};
		tempint = screensize;

		if (((loc.bits&32) > (screensizeflag&32)) && (screensize > 64))
		{
			apply = true;
			screensize -= (screensize>>3);
		}
		if (((loc.bits&16) > (screensizeflag&16)) && (screensize <= xdim))
		{
			apply = true;
			screensize += (screensize>>3);
			if ((screensize > xdim) && (tempint == xdim))
			{
				screensize = xdim+1;
			}
			else
			{
				if (screensize > xdim) screensize = xdim;
			}
		}
		if (apply) {
			ox1 = 0;
			oy1 = 0;
			ox2 = xdim-1;
			oy2 = ydim-32-1;

			flushperms();

			if (screensize <= xdim)
			{
				rotatesprite((xdim-320)<<15,(ydim-32)<<16,65536L,0,STATUSBAR,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L);
				int i = ((xdim-320)>>1);
				while (i >= 8) i -= 8, rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL8,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L);
				if (i >= 4) i -= 4, rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL4,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L);
				i = ((xdim-320)>>1)+320;
				while (i <= xdim-8) rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL8,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L), i += 8;
				if (i <= xdim-4) rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL4,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L), i += 4;

				drawstatusbar(screenpeek);   // Andy did this
			}

			if (screensize > xdim)
			{
				x1 = 0; y1 = 0;
				x2 = xdim-1; y2 = ydim-1;
			}
			else
			{
				x1 = ((xdim-screensize)>>1);
				x2 = x1+screensize-1;
				y1 = (((ydim-32)-scale(screensize,ydim-32,xdim))>>1);
				y2 = y1 + scale(screensize,ydim-32,xdim)-1;
			}
			setview(x1,y1,x2,y2);

			// (ox1,oy1)⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
			//          ≥  (x1,y1)        ≥
			//          ≥     ⁄ƒƒƒƒƒø     ≥
			//          ≥     ≥     ≥     ≥
			//          ≥     ¿ƒƒƒƒƒŸ     ≥
			//          ≥        (x2,y2)  ≥
			//          ¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ(ox2,oy2)

			drawtilebackground(0L,0L,BACKGROUND,8,ox1,oy1,x1-1,oy2,0);
			drawtilebackground(0L,0L,BACKGROUND,8,x2+1,oy1,ox2,oy2,0);
			drawtilebackground(0L,0L,BACKGROUND,8,x1,oy1,x2,y1-1,0);
			drawtilebackground(0L,0L,BACKGROUND,8,x1,y2+1,x2,oy2,0);
		}
		screensizeflag = loc.bits;
	}

	if (dimensionmode[snum] != 2)
	{
		if ((numplayers > 1) && (option[4] == 0))
		{
				//Do not draw other views constantly if they're staying still
				//It's a shame this trick will only work in screen-buffer mode
				//At least screen-buffer mode covers all the HI hi-res modes
			//if (vidoption == 2)
			//{
				for(int i{connecthead}; i >= 0; i = connectpoint2[i]) {
					frame2draw[i] = 0;
				}

				frame2draw[snum] = 1;

					//2-1,3-1,4-2
					//5-2,6-2,7-2,8-3,9-3,10-3,11-3,12-4,13-4,14-4,15-4,16-5
				x1 = posx[snum]; y1 = posy[snum];
				for(int j = (numplayers >> 2) + 1; j > 0; --j)
				{
					maxdist = 0x80000000;
					for(int i{connecthead}; i >= 0; i = connectpoint2[i]) {
						if (frame2draw[i] == 0)
						{
							x2 = posx[i]-x1; y2 = posy[i]-y1;
							dist = dmulscalen<12>(x2,x2,y2,y2);

							if (dist < 64) dist = 16384;
							else if (dist > 16384) dist = 64;
							else dist = 1048576 / dist;

							dist *= frameskipcnt[i];

								//Increase frame rate if screen is moving
							if ((posx[i] != oposx[i]) || (posy[i] != oposy[i]) ||
								 (posz[i] != oposz[i]) || (ang[i] != oang[i]) ||
								 (horiz[i] != ohoriz[i])) dist += dist;

							if (dist > maxdist) maxdist = dist, k = i;
						}
					}

					for(int i{connecthead}; i >= 0; i = connectpoint2[i]) {
						frameskipcnt[i] += (frameskipcnt[i]>>3)+1;
					}

					frameskipcnt[k] = 0;

					frame2draw[k] = 1;
				}
			//}
			//else
			//{
			//   for(i=connecthead;i>=0;i=connectpoint2[i]) frame2draw[i] = 1;
			//}

			for(int i{connecthead}, j = 0; i >= 0; i = connectpoint2[i], ++j) {
				if (frame2draw[i] != 0)
				{
					if (numplayers <= 4)
					{
						switch(j)
						{
							case 0: setview(0,0,(xdim>>1)-1,(ydim>>1)-1); break;
							case 1: setview((xdim>>1),0,xdim-1,(ydim>>1)-1); break;
							case 2: setview(0,(ydim>>1),(xdim>>1)-1,ydim-1); break;
							case 3: setview((xdim>>1),(ydim>>1),xdim-1,ydim-1); break;
						}
					}
					else
					{
						switch(j)
						{
							case 0: setview(0,0,(xdim>>2)-1,(ydim>>2)-1); break;
							case 1: setview(xdim>>2,0,(xdim>>1)-1,(ydim>>2)-1); break;
							case 2: setview(xdim>>1,0,xdim-(xdim>>2)-1,(ydim>>2)-1); break;
							case 3: setview(xdim-(xdim>>2),0,xdim-1,(ydim>>2)-1); break;
							case 4: setview(0,ydim>>2,(xdim>>2)-1,(ydim>>1)-1); break;
							case 5: setview(xdim>>2,ydim>>2,(xdim>>1)-1,(ydim>>1)-1); break;
							case 6: setview(xdim>>1,ydim>>2,xdim-(xdim>>2)-1,(ydim>>1)-1); break;
							case 7: setview(xdim-(xdim>>2),ydim>>2,xdim-1,(ydim>>1)-1); break;
							case 8: setview(0,ydim>>1,(xdim>>2)-1,ydim-(ydim>>2)-1); break;
							case 9: setview(xdim>>2,ydim>>1,(xdim>>1)-1,ydim-(ydim>>2)-1); break;
							case 10: setview(xdim>>1,ydim>>1,xdim-(xdim>>2)-1,ydim-(ydim>>2)-1); break;
							case 11: setview(xdim-(xdim>>2),ydim>>1,xdim-1,ydim-(ydim>>2)-1); break;
							case 12: setview(0,ydim-(ydim>>2),(xdim>>2)-1,ydim-1); break;
							case 13: setview(xdim>>2,ydim-(ydim>>2),(xdim>>1)-1,ydim-1); break;
							case 14: setview(xdim>>1,ydim-(ydim>>2),xdim-(xdim>>2)-1,ydim-1); break;
							case 15: setview(xdim-(xdim>>2),ydim-(ydim>>2),xdim-1,ydim-1); break;
						}
					}

					if (i == snum)
					{
						sprite[playersprite[snum]].cstat |= 0x8000;
						drawrooms(cposx,cposy,cposz,cang,choriz,cursectnum[i]);
						sprite[playersprite[snum]].cstat &= ~0x8000;
						analyzesprites(cposx,cposy);
					}
					else
					{
						sprite[playersprite[i]].cstat |= 0x8000;
						drawrooms(posx[i],posy[i],posz[i],ang[i],horiz[i],cursectnum[i]);
						sprite[playersprite[i]].cstat &= ~0x8000;
						analyzesprites(posx[i],posy[i]);
					}
					drawmasks();
					if ((numgrabbers[i] > 0) || (nummissiles[i] > 0) || (numbombs[i] > 0))
						rotatesprite(160<<16,184L<<16,65536,0,GUNONBOTTOM,g_sector[cursectnum[i]].floorshade,0,2,windowx1,windowy1,windowx2,windowy2);

					if (lockclock < 384)
					{
						if (lockclock < 128)
							rotatesprite(320<<15,200<<15,lockclock<<9,lockclock<<4,DEMOSIGN,(128-lockclock)>>2,0,1+2,windowx1,windowy1,windowx2,windowy2);
						else if (lockclock < 256)
							rotatesprite(320<<15,200<<15,65536,0,DEMOSIGN,0,0,2,windowx1,windowy1,windowx2,windowy2);
						else
							rotatesprite(320<<15,200<<15,(384-lockclock)<<9,lockclock<<4,DEMOSIGN,(lockclock-256)>>2,0,1+2,windowx1,windowy1,windowx2,windowy2);
					}

					if (health[i] <= 0)
						rotatesprite(320<<15,200<<15,(-health[i])<<11,(-health[i])<<5,NO,0,0,2,windowx1,windowy1,windowx2,windowy2);
				}
			}
		}
		else
		{
				//Init for screen rotation
#if USE_POLYMOST
			if (POLYMOST_RENDERMODE_POLYMOST()) {
				tiltlock = screentilt;
					// Ken loves to interpolate
				setrollangle(oscreentilt + mulscalen<16>(((screentilt-oscreentilt+1024)&2047)-1024,smoothratio));
			} else
#endif
			{
				tiltlock = screentilt;
				if ((tiltlock) || (detailmode))
				{
					walock[MAXTILES-2] = 255;
					if (waloff[MAXTILES-2] == 0)
						allocache((void **)&waloff[MAXTILES-2],320L*320L,&walock[MAXTILES-2]);
					if ((tiltlock&1023) == 0)
						setviewtotile(MAXTILES-2,200L>>detailmode,320L>>detailmode);
					else
						setviewtotile(MAXTILES-2,320L>>detailmode,320L>>detailmode);
					if ((tiltlock&1023) == 512)
					{     //Block off unscreen section of 90¯ tilted screen
						const int j = ((320 - 60) >> detailmode);
						for(int i = (60 >> detailmode) - 1; i >= 0; --i)
						{
							startumost[i] = 1;
							startumost[i + j] = 1;
							startdmost[i] = 0;
							startdmost[i + j] = 0;
						}
					}

					int i = (tiltlock & 511);
					
					if (i > 256)
						i = 512 - i;
					
					i = sintable[i + 512] * 8 + sintable[i] * 5L;
					setaspect(i >> 1, yxaspect);
				}
			}

			if ((gotpic[FLOORMIRROR>>3]&(1<<(FLOORMIRROR&7))) > 0)
			{
				dist = 0x7fffffff;
				int i{0};

				for(k=floormirrorcnt-1;k>=0;k--)
				{
					int j = std::abs(wall[g_sector[floormirrorsector[k]].wallptr].pt.x-cposx);
					j += std::abs(wall[g_sector[floormirrorsector[k]].wallptr].pt.y-cposy);

					if (j < dist) {
						dist = j;
						i = k;
					}
				}

				//if (cposz > g_sector[floormirrorsector[i]].ceilingz) i = 1-i; //SOS

				const int fmsect = floormirrorsector[i];

				if (cameradist < 0) sprite[playersprite[snum]].cstat |= 0x8000;
				drawrooms(cposx,cposy,(g_sector[fmsect].floorz<<1)-cposz,cang,201-choriz,fmsect); //SOS
				//drawrooms(cposx,cposy,cposz,cang,choriz,j+MAXSECTORS); //SOS
				sprite[playersprite[snum]].cstat &= ~0x8000;
				analyzesprites(cposx,cposy);
				drawmasks();

					//Temp horizon
				if (POLYMOST_RENDERMODE_CLASSIC())
				{
					l = scale(choriz-100,windowx2-windowx1,320)+((windowy1+windowy2)>>1);
					for(y1=windowy1,y2=windowy2;y1<y2;y1++,y2--)
					{
						ptr = (unsigned char *)(frameplace+ylookup[y1]);
						ptr2 = (unsigned char *)(frameplace+ylookup[y2]);
						ptr3 = palookup[18].data();
						ptr3 += (std::min(std::abs(y1 - l) >> 2, 31) << 8);
						ptr4 = palookup[18].data();
						ptr4 += (std::min(std::abs(y2 - l) >> 2, 31) << 8);

						int j = sintable[((y2+totalclock)<<6)&2047];
						j += sintable[((y2-totalclock)<<7)&2047];
						j >>= 14;

						//ptr2 += j;

						//for(x1=windowx1;x1<=windowx2;x1++)
						//	{ ch = ptr[x1]; ptr[x1] = ptr3[ptr2[x1]]; ptr2[x1] = ptr4[ch]; }

						ox1 = windowx1 - std::min(j, 0);
						ox2 = windowx2 - std::max(j, 0);

						for(x1=windowx1;x1<ox1;x1++)
							{ ch = ptr[x1]; ptr[x1] = ptr3[ptr2[x1]]; ptr2[x1] = ptr4[ch]; }
						for(x1=ox2+1;x1<=windowx2;x1++)
							{ ch = ptr[x1]; ptr[x1] = ptr3[ptr2[x1]]; ptr2[x1] = ptr4[ch]; }

						ptr2 += j;
						for(x1=ox1;x1<=ox2;x1++)
							{ ch = ptr[x1]; ptr[x1] = ptr3[ptr2[x1]]; ptr2[x1] = ptr4[ch]; }
					}
				}
				gotpic[FLOORMIRROR>>3] &= ~(1<<(FLOORMIRROR&7));
			}

			if (gotpic[DAYSKY>>3]&(1<<(DAYSKY&7)))
			{
				gotpic[DAYSKY>>3] &= ~(1<<(DAYSKY&7));
				pskyoff[0] = 0; pskyoff[1] = 0; pskybits = 1;
			}
			else if (gotpic[NIGHTSKY>>3]&(1<<(NIGHTSKY&7)))
			{
				gotpic[NIGHTSKY>>3] &= ~(1<<(NIGHTSKY&7));
				pskyoff[0] = 0; pskyoff[1] = 0; pskyoff[2] = 0; pskyoff[3] = 0;
				pskyoff[4] = 0; pskyoff[5] = 0; pskyoff[6] = 0; pskyoff[7] = 0;
				pskybits = 3;
			}


				//Over the shoulder mode
			csect = cursectnum[snum];
			if (cameradist >= 0)
			{
				cang += cameraang;
				view(playersprite[snum],&cposx,&cposy,&cposz,&csect,cang,choriz);
			}

				//WARNING!  Assuming (MIRRORLABEL&31) = 0 and MAXMIRRORS = 64
			intptr = (int *)&gotpic[MIRRORLABEL>>3];   // CHECK!
			if (intptr[0]|intptr[1])
				for(int i = MAXMIRRORS - 1; i >= 0; --i) {
					if (gotpic[(i+MIRRORLABEL)>>3]&(1<<(i&7)))
					{
						gotpic[(i+MIRRORLABEL)>>3] &= ~(1<<(i&7));

							//Prepare drawrooms for drawing mirror and calculate reflected
							//position into tposx, tposy, and tang (tposz == cposz)
							//Must call preparemirror before drawrooms and
							//          completemirror after drawrooms
						preparemirror(cposx,cposy,cposz,cang,choriz,
									  mirrorwall[i],mirrorsector[i],&tposx,&tposy,&tang);

						ovisibility = visibility;
						oparallaxvisibility = parallaxvisibility;
						visibility <<= 1;
						parallaxvisibility <<= 1;
						std::swap(palookup[0], palookup[17]);

						drawrooms(tposx,tposy,cposz,tang,choriz,mirrorsector[i]|MAXSECTORS);
						
						int j{0};
						
						for(auto* tspr = &tsprite[0]; j < spritesortcnt; ++j, ++tspr) {
							if ((tspr->cstat&48) == 0)
								tspr->cstat |= 4;
						}

						analyzesprites(tposx,tposy);
						drawmasks();

						std::swap(palookup[0], palookup[17]);
						visibility = ovisibility;
						parallaxvisibility = oparallaxvisibility;

						completemirror();   //Reverse screen x-wise in this function

						break;
					}
			}

			if (cameradist < 0)
				sprite[playersprite[snum]].cstat |= 0x8000;

			drawrooms(cposx,cposy,cposz,cang,choriz,csect);
			sprite[playersprite[snum]].cstat &= ~0x8000;
			analyzesprites(cposx,cposy);
			drawmasks();

				//Finish for screen rotation
			if (POLYMOST_RENDERMODE_CLASSIC())
			{
				if ((tiltlock) || (detailmode))
				{
					setviewback();
					int i = (tiltlock & 511);
					
					if (i > 256)
						i = 512 - i;

					i = sintable[i+512]*8 + sintable[i]*5L;
					
					if (detailmode == 0)
						i >>= 1;

					rotatesprite(320<<15,200<<15,i,tiltlock+512,MAXTILES-2,0,0,2+4+64,windowx1,windowy1,windowx2,windowy2);
					walock[MAXTILES-2] = 1;
				}
			}

			if (((numgrabbers[screenpeek] > 0) || (nummissiles[screenpeek] > 0) || (numbombs[screenpeek] > 0)) && (cameradist < 0))
			{
					//Reset startdmost to bottom of screen
				if ((windowx1 == 0) && (windowx2 == 319) && (yxaspect == 65536) && (tiltlock == 0))
				{
					x1 = 160L-(tilesizx[GUNONBOTTOM]>>1); y1 = windowy2+1;
					for(int i{0}; i < tilesizx[GUNONBOTTOM]; ++i) {
						startdmost[i + x1] = y1;
					}
				}
				rotatesprite(160<<16,184L<<16,65536,0,GUNONBOTTOM,g_sector[cursectnum[screenpeek]].floorshade,0,2,windowx1,windowy1,windowx2,windowy2);
			}

			if (cachecount != 0)
			{
				rotatesprite((320-16)<<16,16<<16,32768,0,BUILDDISK,0,0,2+64,windowx1,windowy1,windowx2,windowy2);
				cachecount = 0;
			}

			if (lockclock < 384)
			{
				if (lockclock < 128)
					rotatesprite(320<<15,200<<15,lockclock<<9,lockclock<<4,DEMOSIGN,(128-lockclock)>>2,0,1+2,windowx1,windowy1,windowx2,windowy2);
				else if (lockclock < 256)
					rotatesprite(320<<15,200<<15,65536,0,DEMOSIGN,0,0,2,windowx1,windowy1,windowx2,windowy2);
				else
					rotatesprite(320<<15,200<<15,(384-lockclock)<<9,lockclock<<4,DEMOSIGN,(lockclock-256)>>2,0,1+2,windowx1,windowy1,windowx2,windowy2);
			}

			if (health[screenpeek] <= 0)
				rotatesprite(320<<15,200<<15,(-health[screenpeek])<<11,(-health[screenpeek])<<5,NO,0,0,2,windowx1,windowy1,windowx2,windowy2);
		}
	}

		//Only animate lava if its picnum is on screen
		//gotpic is a bit array where the tile number's bit is set
		//whenever it is drawn (ceilings, walls, sprites, etc.)
	if ((gotpic[SLIME>>3]&(1<<(SLIME&7))) > 0)
	{
		gotpic[SLIME>>3] &= ~(1<<(SLIME&7));
		if (waloff[SLIME] != 0) {
			movelava((unsigned char *)waloff[SLIME]);
#if USE_POLYMOST && USE_OPENGL
			invalidatetile(SLIME,0,1);
#endif
		}
	}

	if ((show2dsector[cursectnum[snum]>>3]&(1<<(cursectnum[snum]&7))) == 0)
		searchmap(cursectnum[snum]);

	if (dimensionmode[snum] != 3)
	{
			//Move back pivot point
		const int i = scale(czoom,screensize,320);

		if (dimensionmode[snum] == 2)
		{
			clearview(0L);  //Clear screen to specified color
			drawmapview(cposx, cposy, i, cang);
		}
		drawoverheadmap(cposx, cposy, i, cang);
	}

	if (typemode != 0)
	{
		charsperline = 40;
		//if (dimensionmode[snum] == 2) charsperline = 80;

		for(int i{0}; i <= typemessageleng; i += charsperline)
		{
			for(int j{0}; j < charsperline; ++j) {
				tempbuf[j] = typemessage[i+j];
			}

			if (typemessageleng < i + charsperline)
			{
				tempbuf[(typemessageleng-i)] = '_';
				tempbuf[(typemessageleng-i)+1] = 0;
			}
			else
				tempbuf[charsperline] = 0;

			printext256(0L,(i/charsperline)<<3,31/*183*/,-1,tempbuf,0);
		}
	}

	if (getmessageleng > 0)
	{
		charsperline = 40;
		//if (dimensionmode[snum] == 2) charsperline = 80;

		for(int i{0}; i <= getmessageleng; i += charsperline)
		{
			for(int j{0}; j < charsperline; ++j) {
				tempbuf[j] = getmessage[i + j];
			}

			if (getmessageleng < i+charsperline)
				tempbuf[(getmessageleng-i)] = 0;
			else
				tempbuf[charsperline] = 0;

			printext256(0L,((i/charsperline)<<3)+(ydim-32-8)-(((getmessageleng-1)/charsperline)<<3),31/*151*/,-1,tempbuf,0);
		}
		if (totalclock > getmessagetimeoff)
			getmessageleng = 0;
	}
	if ((numplayers >= 2) && (screenpeek != myconnectindex))
	{
		int j{1};

		for(int i = connecthead; i >= 0; i = connectpoint2[i])
		{
			if (i == screenpeek)
				break;

			++j;
		}

		std::sprintf(tempbuf,"(Player %d's view)",j);
		printext256((xdim>>1)-(int)(std::strlen(tempbuf)<<2),0,24,-1,tempbuf,0);
	}

	if (syncstat != 0) printext256(68L,84L,31,0,"OUT OF SYNC!",0);
	if (syncstate != 0) printext256(68L,92L,31,0,"Missed Network packet!",0);

//   //Uncomment this to test cache locks
//extern int cacnum;
//typedef struct { int *hand, leng; unsigned char *lock; } cactype;
//extern cactype cac[];
//
//   j = 0;
//   for(i=0;i<cacnum;i++)
//      if ((*cac[i].lock) >= 200)
//      {
//         std::sprintf(tempbuf,"Locked- %ld: Leng:%ld, Lock:%ld",i,cac[i].leng,*cac[i].lock);
//         printext256(0L,j,31,-1,tempbuf,1); j += 6;
//      }

	nextpage();   // send completed frame to display

	while (ready2send && totalclock >= ototalclock+(TIMERINTSPERSECOND/MOVESPERSECOND))
		faketimerhandler();

	if (keystatus[0x3f])   //F5
	{
		keystatus[0x3f] = 0;
		detailmode ^= 1;
		//setrendermode(3);
	}
	if (keystatus[0x58])   //F12
	{
		keystatus[0x58] = 0;
		screencapture("captxxxx.tga", keystatus[0x2a]|keystatus[0x36]);
	}
	if (keystatus[0x3e])  //F4 - screen re-size
	{
		keystatus[0x3e] = 0;

		if (keystatus[0x2a]|keystatus[0x36]) {
			setgamemode(!fullscreen, xdim, ydim, bpp);
		}
		else {

			//cycle through all modes
			int j{-1};

			// work out a mask to select the mode
			for (int i{0}; const auto& vmode : validmode) {
				if ((vmode.xdim == xdim) &&
				    (vmode.ydim == ydim) &&
					(vmode.fs == fullscreen) &&
					(vmode.bpp == bpp)) {
						j = i;
						break;
				}

				++i;
			}

			k = 0;

			for (const auto& vmode : validmode) {
				if (vmode.fs == fullscreen && vmode.bpp == bpp)
					break;
				
				++k;
			}

			if (j==-1)
				j = k;
			else {
				++j;
				if (j == validmode.size())
					j = k;
			}

			setgamemode(fullscreen,validmode[j].xdim,validmode[j].ydim,bpp);
		}
		screensize = xdim+1;

		std::sprintf(getmessage,"Video mode: %d x %d",xdim,ydim);
		getmessageleng = (int)std::strlen(getmessage);
		getmessagetimeoff = totalclock+120*5;
	}
	if (keystatus[0x57])  //F11 - brightness
	{
		keystatus[0x57] = 0;
		brightness++;
		if (brightness > 8) brightness = 0;
		setbrightness(brightness, palette, 0);
	}

	if (option[4] == 0)           //Single player only keys
	{
		if (keystatus[0xd2])   //Insert - Insert player
		{
			keystatus[0xd2] = 0;
			if (numplayers < MAXPLAYERS)
			{
				connectpoint2[numplayers-1] = numplayers;
				connectpoint2[numplayers] = -1;

				movefifoend[numplayers] = movefifoend[0];   //HACK 01/05/2000

				initplayersprite(numplayers);

				clearallviews(0L);  //Clear screen to specified color

				numplayers++;
			}
		}
		if (keystatus[0xd3])   //Delete - Delete player
		{
			keystatus[0xd3] = 0;
			if (numplayers > 1)
			{
				numplayers--;
				connectpoint2[numplayers-1] = -1;

				deletesprite(playersprite[numplayers]);
				playersprite[numplayers] = -1;

				if (myconnectindex >= numplayers) myconnectindex = 0;
				if (screenpeek >= numplayers) screenpeek = 0;

				if (numplayers < 2)
					setup3dscreen();
				else
					clearallviews(0L);  //Clear screen to specified color
			}
		}
		if (keystatus[0x46])   //Scroll Lock
		{
			keystatus[0x46] = 0;

			myconnectindex = connectpoint2[myconnectindex];
			if (myconnectindex < 0) myconnectindex = connecthead;
			screenpeek = myconnectindex;
		}
	}

	restoreinterpolations();
}

void movethings()
{
	int i;

	gotlastpacketclock = totalclock;
	for(i=connecthead;i>=0;i=connectpoint2[i])
	{
		copybufbyte(&ffsync[i],&baksync[movefifoend[i]][i],sizeof(input));
		movefifoend[i] = ((movefifoend[i]+1)&(MOVEFIFOSIZ-1));
	}
}

void fakedomovethings()
{
	input *syn;
	int doubvel;
	int xvect;
	int yvect;
	int goalz;
	short bakcstat;

	syn = (input *)&baksync[fakemovefifoplc][myconnectindex];

	omyx = myx;
	omyy = myy;
	omyz = myz;
	omyang = myang;
	omyhoriz = myhoriz;

	bakcstat = sprite[playersprite[myconnectindex]].cstat;
	sprite[playersprite[myconnectindex]].cstat &= ~0x101;

	if ((syn->fvel|syn->svel) != 0)
	{
		doubvel = (TICSPERFRAME << static_cast<int>((syn->bits & 256) > 0));

		xvect = 0, yvect = 0;
		if (syn->fvel != 0)
		{
			xvect += ((((int)syn->fvel)*doubvel*(int)sintable[(myang+512)&2047])>>3);
			yvect += ((((int)syn->fvel)*doubvel*(int)sintable[myang&2047])>>3);
		}
		if (syn->svel != 0)
		{
			xvect += ((((int)syn->svel)*doubvel*(int)sintable[myang&2047])>>3);
			yvect += ((((int)syn->svel)*doubvel*(int)sintable[(myang+1536)&2047])>>3);
		}
		if (flytime[myconnectindex] > lockclock) { xvect += xvect; yvect += yvect; }   // DOuble flying speed
		clipmove(&myx,&myy,&myz,&mycursectnum,xvect,yvect,128L,4<<8,4<<8,CLIPMASK0);
	}

	pushmove(&myx,&myy,&myz,&mycursectnum,128L,4<<8,4<<8,CLIPMASK0);
	getzrange(myx,myy,myz,mycursectnum,&globhiz,&globhihit,&globloz,&globlohit,128L,CLIPMASK0);

	if (syn->avel != 0)          //ang += avel * constant
	{                         //ENGINE calculates avel for you
		doubvel = TICSPERFRAME;
		if ((syn->bits&256) > 0)  //Lt. shift makes turn velocity 50% faster
			doubvel += (TICSPERFRAME>>1);
		myang += ((((int)syn->avel)*doubvel)>>4);
		myang &= 2047;
	}

	if (((syn->bits&8) > 0) && (myhoriz > 100-(200>>1))) myhoriz -= 4;   //-
	if (((syn->bits&4) > 0) && (myhoriz < 100+(200>>1))) myhoriz += 4;   //+

	goalz = globloz-EYEHEIGHT;
	if (g_sector[mycursectnum].lotag == 4)   //slime sector
		if ((globlohit&0xc000) != 49152)            //You're not on a sprite
		{
			goalz = globloz-(8<<8);
			if (myz >= goalz-(2<<8))
				clipmove(&myx,&myy,&myz,&mycursectnum,-(TICSPERFRAME<<14),-(TICSPERFRAME<<14),128L,4<<8,4<<8,CLIPMASK0);
		}
	if (goalz < globhiz+(16<<8))   //ceiling&floor too close
		goalz = ((globloz+globhiz)>>1);

	if (health[myconnectindex] >= 0)
	{
		if ((syn->bits&1) > 0)                         //A (stand high)
		{
			if (flytime[myconnectindex] <= lockclock)
			{
				if (myz >= globloz-(32<<8))
				{
					goalz -= (16<<8);
					if (syn->bits&256) goalz -= (24<<8);
				}
			}
			else
			{
				myzvel -= 192;
				if (syn->bits&256) myzvel -= 192;
			}
		}
		if ((syn->bits&2) > 0)                         //Z (stand low)
		{
			if (flytime[myconnectindex] <= lockclock)
			{
				goalz += (12<<8);
				if (syn->bits&256) goalz += (12<<8);
			}
			else
			{
				myzvel += 192;
				if (syn->bits&256) myzvel += 192;
			}
		}
	}

	if (flytime[myconnectindex] <= lockclock)
	{
		if (myz < goalz)
			myzvel += (TICSPERFRAME<<4);
		else
			myzvel = (((goalz-myz)*TICSPERFRAME)>>5);
	}
	else
	{
		myzvel -= (myzvel>>2);
		myzvel -= ksgn(myzvel);
	}

	myz += myzvel;
	if (myz > globloz-(4<<8)) myz = globloz-(4<<8), myzvel = 0;
	if (myz < globhiz+(4<<8)) myz = globhiz+(4<<8), myzvel = 0;

	sprite[playersprite[myconnectindex]].cstat = bakcstat;

	myxbak[fakemovefifoplc] = myx;
	myybak[fakemovefifoplc] = myy;
	myzbak[fakemovefifoplc] = myz;
	myangbak[fakemovefifoplc] = myang;
	myhorizbak[fakemovefifoplc] = myhoriz;
	fakemovefifoplc = (fakemovefifoplc+1)&(MOVEFIFOSIZ-1);
}

	//Prediction correction
void fakedomovethingscorrect()
{
	int i;

	if ((networkmode == 0) && (myconnectindex == connecthead)) return;

	i = ((movefifoplc-1)&(MOVEFIFOSIZ-1));

	if ((posx[myconnectindex] == myxbak[i]) &&
		 (posy[myconnectindex] == myybak[i]) &&
		 (posz[myconnectindex] == myzbak[i]) &&
		 (horiz[myconnectindex] == myhorizbak[i]) &&
		 (ang[myconnectindex] == myangbak[i]))
		 return;

		//Re-start fakedomovethings back to place of error
	myx = omyx = posx[myconnectindex];
	myy = omyy = posy[myconnectindex];
	myz = omyz = posz[myconnectindex]; myzvel = hvel[myconnectindex];
	myang = omyang = ang[myconnectindex];
	myhoriz = omyhoriz = horiz[myconnectindex];

	fakemovefifoplc = movefifoplc;
	while (fakemovefifoplc != movefifoend[myconnectindex]) fakedomovethings();
}

void domovethings()
{
	short i;
	short j;
	short startwall;
	short endwall;
	walltype *wal;

	nummoves++;

	for(i=connecthead;i>=0;i=connectpoint2[i])
		copybufbyte(&baksync[movefifoplc][i],&ssync[i],sizeof(input));
	movefifoplc = ((movefifoplc+1)&(MOVEFIFOSIZ-1));

	if (option[4] != 0)
	{
		syncval[syncvalhead] = (unsigned char)(randomseed&255);
		syncvalhead = ((syncvalhead+1)&(MOVEFIFOSIZ-1));
	}

	for(i=connecthead;i>=0;i=connectpoint2[i])
	{
		oposx[i] = posx[i];
		oposy[i] = posy[i];
		oposz[i] = posz[i];
		ohoriz[i] = horiz[i];
		ozoom[i] = zoom[i];
		oang[i] = ang[i];
	}

	for(i=NUMSTATS-1;i>=0;i--)
		if (statrate[i] >= 0)
			for(j=headspritestat[i];j>=0;j=nextspritestat[j])
				if (((nummoves-j)&statrate[i]) == 0)
					copybuf(&sprite[j].x,&osprite[j].x,3);

	for(i=connecthead;i>=0;i=connectpoint2[i])
		ocursectnum[i] = cursectnum[i];

	updateinterpolations();

	if ((numplayers <= 2) && (recstat == 1))
	{
		j = 0;
		for(i=connecthead;i>=0;i=connectpoint2[i])
		{
			copybufbyte(&ssync[i],&recsync[reccnt][j],sizeof(input));
			j++;
		}
		reccnt++; if (reccnt > 16383) reccnt = 16383;
	}

	lockclock += TICSPERFRAME;
	drawstatusflytime(screenpeek);   // Andy did this

	if (cameradist >= 0)
	{
		cameradist = std::min(cameradist + ((totalclock - cameraclock) << 10), 65536);
		if (keystatus[0x52])       //0
			cameraang -= ((totalclock-cameraclock)<<(2+(keystatus[0x2a]|keystatus[0x36])));
		if (keystatus[0x53])       //.
			cameraang += ((totalclock-cameraclock)<<(2+(keystatus[0x2a]|keystatus[0x36])));
		cameraclock = totalclock;
	}

	for(i=connecthead;i>=0;i=connectpoint2[i])
	{
		processinput(i);                        //Move player

		checktouchsprite(i,cursectnum[i]);      //Pick up coins
		startwall = g_sector[cursectnum[i]].wallptr;
		endwall = startwall + g_sector[cursectnum[i]].wallnum;
		for(j=startwall,wal=&wall[j];j<endwall;j++,wal++)
			if (wal->nextsector >= 0) checktouchsprite(i,wal->nextsector);
	}

	doanimations();
	tagcode();            //Door code, moving sector code, other stuff
	statuslistcode();     //Monster / bullet code / explosions

	fakedomovethingscorrect();

	checkmasterslaveswitch();
}

void getinput()
{
	unsigned char ch;
	int i;
	int j;
	int mousx;
	int mousy;
	int bstatus;

	if (typemode == 0)           //if normal game keys active
	{
		if (keystatus[keys[15]])
		{
			keystatus[keys[15]] = 0;

			screenpeek = connectpoint2[screenpeek];
			if (screenpeek < 0) screenpeek = connecthead;
			drawstatusbar(screenpeek);   // Andy did this
		}

		for(i=7;i>=0;i--)
			if (keystatus[i+2])
				{ keystatus[i+2] = 0; locselectedgun = i; break; }
	}


		//KEYTIMERSTUFF
	if (!keystatus[keys[5]])
	{
		if (keystatus[keys[2]]) avel = std::max(avel - 16 * TICSPERFRAME, -128);
		if (keystatus[keys[3]]) avel = std::min(avel + 16 * TICSPERFRAME,  127);
	}
	else
	{
		if (keystatus[keys[2]]) svel = std::min(svel + 8 * TICSPERFRAME,  127);
		if (keystatus[keys[3]]) svel = std::max(svel - 8 * TICSPERFRAME, -128);
	}
	if (keystatus[keys[0]]) fvel = std::min(fvel + 8 * TICSPERFRAME,  127);
	if (keystatus[keys[1]]) fvel = std::max(fvel - 8 * TICSPERFRAME, -128);
	if (keystatus[keys[12]]) svel = std::min(svel + 8 * TICSPERFRAME,  127);
	if (keystatus[keys[13]]) svel = std::max(svel - 8 * TICSPERFRAME, -128);

	if (avel < 0) avel = std::min(avel + 12 * TICSPERFRAME, 0);
	if (avel > 0) avel = std::max(avel - 12 * TICSPERFRAME, 0);
	if (svel < 0) svel = std::min(svel + 2 * TICSPERFRAME, 0);
	if (svel > 0) svel = std::max(svel - 2 * TICSPERFRAME, 0);
	if (fvel < 0) fvel = std::min(fvel + 2 * TICSPERFRAME, 0);
	if (fvel > 0) fvel = std::max(fvel - 2 * TICSPERFRAME, 0);

	if ((option[4] == 0) && (numplayers >= 2))
	{
		if (!keystatus[0x4f])
		{
			if (keystatus[0x4b]) avel2 = std::max(avel2 - 16 * TICSPERFRAME, -128);
			if (keystatus[0x4d]) avel2 = std::min(avel2 + 16 * TICSPERFRAME,  127);
		}
		else
		{
			if (keystatus[0x4b]) svel2 = std::min(svel2 + 8 * TICSPERFRAME,  127);
			if (keystatus[0x4d]) svel2 = std::max(svel2 - 8 * TICSPERFRAME, -128);
		}
		if (keystatus[0x48]) fvel2 = std::min(fvel2 + 8 * TICSPERFRAME,  127);
		if (keystatus[0x4c]) fvel2 = std::max(fvel2 - 8 * TICSPERFRAME, -128);

		if (avel2 < 0) avel2 = std::min(avel2 + 12 * TICSPERFRAME, 0);
		if (avel2 > 0) avel2 = std::max(avel2 - 12 * TICSPERFRAME, 0);
		if (svel2 < 0) svel2 = std::min(svel2 + 2 * TICSPERFRAME, 0);
		if (svel2 > 0) svel2 = std::max(svel2 - 2 * TICSPERFRAME, 0);
		if (fvel2 < 0) fvel2 = std::min(fvel2 + 2 * TICSPERFRAME, 0);
		if (fvel2 > 0) fvel2 = std::max(fvel2 - 2 * TICSPERFRAME, 0);
	}

	oscreentilt = screentilt;
	if (keystatus[0x1a]) screentilt += ((4*TICSPERFRAME)<<(keystatus[0x2a]|keystatus[0x36]));
	if (keystatus[0x1b]) screentilt -= ((4*TICSPERFRAME)<<(keystatus[0x2a]|keystatus[0x36]));

	i = (TICSPERFRAME<<1);
	while ((screentilt != 0) && (i > 0))
		{ screentilt = ((screentilt+ksgn(screentilt-1024))&2047); i--; }
	if (keystatus[0x28]) screentilt = 1536;


	loc.fvel = std::min(std::max(fvel, -128 + 8), 127 - 8);
	loc.svel = std::min(std::max(svel, -128 + 8), 127 - 8);
	loc.avel = std::min(std::max(avel, -128 + 16), 127 - 16);

	getmousevalues(&mousx,&mousy,&bstatus);
	loc.avel = std::min(std::max(static_cast<int>(loc.avel) + (mousx << 3), -128), 127);
	loc.fvel = std::min(std::max(static_cast<int>(loc.fvel) - (mousy << 3), -128), 127);

	loc.bits = (locselectedgun<<13);
	if (typemode == 0)           //if normal game keys active
	{
		loc.bits |= (keystatus[0x32]<<9);                 //M (be master)
		loc.bits |= ((keystatus[keys[14]]==1)<<12);       //Map mode
	}
	loc.bits |= keystatus[keys[8]];                   //Stand high
	loc.bits |= (keystatus[keys[9]]<<1);              //Stand low
	loc.bits |= (keystatus[keys[16]]<<4);             //Zoom in
	loc.bits |= (keystatus[keys[17]]<<5);             //Zoom out
	loc.bits |= (keystatus[keys[4]]<<8);                 //Run
	loc.bits |= (keystatus[keys[10]]<<2);                //Look up
	loc.bits |= (keystatus[keys[11]]<<3);                //Look down
	loc.bits |= ((keystatus[keys[7]]==1)<<10);           //Space
	loc.bits |= ((keystatus[keys[6]]==1)<<11);           //Shoot
	loc.bits |= (((bstatus&6)>(oldmousebstatus&6))<<10); //Space
	loc.bits |= (((bstatus&1)>(oldmousebstatus&1))<<11); //Shoot

	oldmousebstatus = bstatus;
	if (((loc.bits&2048) > 0) && (locselectedgun == 0))
		oldmousebstatus &= ~1;     //Allow continous fire with mouse for chain gun

	if (option[3] & 2) {
		if (joynumaxes == 2) {
			loc.avel = std::min(std::max(static_cast<int>(loc.avel) + (joyaxis[0] >> 8), -128), 127);
			loc.fvel = std::min(std::max(static_cast<int>(loc.fvel) - (joyaxis[1] >> 8), -128), 127);
		} else if (joynumaxes >= 4) {
			loc.avel = std::min(std::max(static_cast<int>(loc.avel) + (joyaxis[2] >> 8), -128), 127);
			loc.fvel = std::min(std::max(static_cast<int>(loc.fvel) - (joyaxis[1] >> 8), -128), 127);
			loc.svel = std::min(std::max(static_cast<int>(loc.svel) - (joyaxis[0] >> 8), -128), 127);
		}
		if (joynumaxes >= 6) {
			loc.bits |= (joyaxis[5] > 0) << 11;	// Rtrigger shoot
		}
		if (joynumbuttons >= 2) {
			loc.bits |= (!!(joyb & 1)) << 11;	// A button shoot
			loc.bits |= (!!(joyb & 2)) << 10;	// B button space
		}
	}

		//PRIVATE KEYS:
/*   if (keystatus[0xb7])  //Printscreen
	{
		keystatus[0xb7] = 0;
		printscreeninterrupt();
	}
*/
	if (keystatus[0x2f])       //V
	{
		keystatus[0x2f] = 0;
		if (cameradist < 0) cameradist = 0; else cameradist = -1;
		cameraang = 0;
	}

	if (typemode == 0)           //if normal game keys active
	{
		if (keystatus[0x19])  //P
		{
			keystatus[0x19] = 0;
			parallaxtype++;
			if (parallaxtype > 2) parallaxtype = 0;
		}
		if (keystatus[0x38]|keystatus[0xb8])  //ALT
		{
			if (keystatus[0x4a])  // Keypad -
				visibility = std::min(visibility + (visibility >> 3), 16384);
			if (keystatus[0x4e])  // Keypad +
				visibility = std::max(visibility - (visibility >> 3), 128);
		}

		if (keystatus[keys[18]])   //Typing mode
		{
			keystatus[keys[18]] = 0;
			typemode = 1;
			bflushchars();
			keyfifoplc = keyfifoend;      //Reset keyboard fifo
		}
	}
	else
	{
		while ((ch = bgetchar()))
		{
			if (ch == 8)   //Backspace
			{
				if (typemessageleng == 0) { typemode = 0; break; }
				typemessageleng--;
			}
			else if (ch == 9)   // tab
			{
				keystatus[0xf] = 0;
				typemode = 0;
				break;
			}
			else if (ch == 13)  //Either ENTER
			{
				keystatus[0x1c] = 0; keystatus[0x9c] = 0;
				if (typemessageleng > 0)
				{
					packbuf[0] = 2;          //Sending text is message type 4
					for(j=typemessageleng-1;j>=0;j--)
						packbuf[j+1] = typemessage[j];

					for(i=connecthead;i>=0;i=connectpoint2[i])
						if (i != myconnectindex)
							sendpacket(i,packbuf,typemessageleng+1);

					typemessageleng = 0;
				}
				typemode = 0;
				break;
			}
			else if ((typemessageleng < 159) && (ch >= 32) && (ch < 128))
			{
				typemessage[typemessageleng++] = ch;
			}
		}
	}
}

void initplayersprite(short snum)
{
	int i;
	unsigned char remapbuf[256];

	if (playersprite[snum] >= 0) return;

	spawnsprite(playersprite[snum],posx[snum],posy[snum],posz[snum]+EYEHEIGHT,
		1+256,0,snum,32,64,64,0,0,PLAYER,ang[snum],0,0,0,snum+4096,
		cursectnum[snum],8,0,0,0);

	switch(snum)
	{
		case 1: for(i=0;i<32;i++) remapbuf[i+192] = i+128; break; //green->red
		case 2: for(i=0;i<32;i++) remapbuf[i+192] = i+32; break;  //green->blue
		case 3: for(i=0;i<32;i++) remapbuf[i+192] = i+224; break; //green->pink
		case 4: for(i=0;i<32;i++) remapbuf[i+192] = i+64; break;  //green->brown
		case 5: for(i=0;i<32;i++) remapbuf[i+192] = i+96; break;
		case 6: for(i=0;i<32;i++) remapbuf[i+192] = i+160; break;
		case 7: for(i=0;i<32;i++) remapbuf[i+192] = i+192; break;
		default: for(i=0;i<256;i++) remapbuf[i] = i; break;
	}
	makepalookup(snum,remapbuf,0,0,0,1);
}

void playback()
{
	int i;
	int j;
	int k;

	ready2send = 0;
	recstat = 0; i = reccnt;
	while (!keystatus[1])
	{
		if (handleevents()) {
			if (quitevent) {
				keystatus[1] = 1;
				quitevent = false;
			}
		}

		refreshaudio();

		while (totalclock >= lockclock+TICSPERFRAME)
		{
			sampletimer();
			if (i >= reccnt)
			{
				prepareboard(boardfilename);
				for(i=connecthead;i>=0;i=connectpoint2[i])
					initplayersprite((short)i);
				totalclock = 0;
				i = 0;
			}

			k = 0;
			for(j=connecthead;j>=0;j=connectpoint2[j])
			{
				copybufbyte(&recsync[i][k],&ffsync[j],sizeof(input));
				k++;
			}
			movethings(); domovethings();
			i++;
		}
		drawscreen(screenpeek,(totalclock-gotlastpacketclock)*(65536/(TIMERINTSPERSECOND/MOVESPERSECOND)));

		if (keystatus[keys[15]])
		{
			keystatus[keys[15]] = 0;
			screenpeek = connectpoint2[screenpeek];
			if (screenpeek < 0) screenpeek = connecthead;
			drawstatusbar(screenpeek);   // Andy did this
		}
		if (keystatus[keys[14]])
		{
			keystatus[keys[14]] = 0;
			dimensionmode[screenpeek]++;
			if (dimensionmode[screenpeek] > 3) dimensionmode[screenpeek] = 1;
		}
	}

	musicoff();
	uninitmultiplayers();
	uninitengine();
	uninitsb();
	uninitgroupfile();
	std::exit(0);
}

void setup3dscreen()
{
	int i;
	int dax;
	int day;
	int dax2;
	int day2;

	i = setgamemode(fullscreen,xdimgame,ydimgame,bppgame);
	if (i < 0)
	{
		std::printf("Error setting video mode.\n");
		musicoff();
		uninitmultiplayers();
		uninitengine();
		uninitsb();
		uninitgroupfile();
		std::exit(0);
	}

	  //Make that ugly pink into black in case it ever shows up!
	i = 0L;
	setpalette(255,1,(unsigned char *)&i);
	//outp(0x3c8,255); outp(0x3c9,0); outp(0x3c9,0); outp(0x3c9,0);

	screensize = xdim;
	if (screensize > xdim)
	{
		dax = 0; day = 0;
		dax2 = xdim-1; day2 = ydim-1;
	}
	else
	{
		dax = ((xdim-screensize)>>1);
		dax2 = dax+screensize-1;
		day = (((ydim-32)-scale(screensize,ydim-32,xdim))>>1);
		day2 = day + scale(screensize,ydim-32,xdim)-1;
		setview(dax,day,dax2,day2);
	}

	flushperms();

	if (screensize < xdim)
		drawtilebackground(0L,0L,BACKGROUND,8,0L,0L,xdim-1L,ydim-1L,0);      //Draw background

	if (screensize <= xdim)
	{
		rotatesprite((xdim-320)<<15,(ydim-32)<<16,65536L,0,STATUSBAR,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L);
		i = ((xdim-320)>>1);
		while (i >= 8) i -= 8, rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL8,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L);
		if (i >= 4) i -= 4, rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL4,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L);
		i = ((xdim-320)>>1)+320;
		while (i <= xdim-8) rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL8,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L), i += 8;
		if (i <= xdim-4) rotatesprite(i<<16,(ydim-32)<<16,65536L,0,STATUSBARFILL4,0,0,8+16+64+128,0L,0L,xdim-1L,ydim-1L), i += 4;

		drawstatusbar(screenpeek);   // Andy did this
	}
}

void findrandomspot(int *x, int *y, short *sectnum)
{
	short startwall;
	short endwall;
	short s;
	short dasector;
	int dax;
	int day;
	int daz;
	int minx;
	int maxx;
	int miny;
	int maxy;
	int cnt;

	for(cnt=256;cnt>=0;cnt--)
	{
		do
		{
			dasector = mulscalen<16>(krand(),numsectors);
		} while ((g_sector[dasector].ceilingz+(8<<8) >= g_sector[dasector].floorz) || ((g_sector[dasector].lotag|g_sector[dasector].hitag) != 0) || ((g_sector[dasector].floorstat&1) != 0));

		startwall = g_sector[dasector].wallptr;
		endwall = startwall+g_sector[dasector].wallnum;
		if (endwall <= startwall) continue;

		dax = 0L;
		day = 0L;
		minx = 0x7fffffff; maxx = 0x80000000;
		miny = 0x7fffffff; maxy = 0x80000000;

		for(s=startwall;s<endwall;s++)
		{
			dax += wall[s].pt.x;
			day += wall[s].pt.y;
			if (wall[s].pt.x < minx) minx = wall[s].pt.x;
			if (wall[s].pt.x > maxx) maxx = wall[s].pt.x;
			if (wall[s].pt.y < miny) miny = wall[s].pt.y;
			if (wall[s].pt.y > maxy) maxy = wall[s].pt.y;
		}

		if ((maxx-minx <= 256) || (maxy-miny <= 256)) continue;

		dax /= (endwall-startwall);
		day /= (endwall-startwall);

		if (inside(dax,day,dasector) == 0) continue;

		daz = g_sector[dasector].floorz-(32<<8);
		if (pushmove(&dax,&day,&daz,&dasector,128L,4<<8,4<<8,CLIPMASK0) < 0) continue;

		*x = dax; *y = day; *sectnum = dasector;
		return;
	}
}

void warp(int *x, int *y, int *z, short *daang, short *dasector)
{
	short startwall;
	short endwall;
	short s;
	int i;
	int j;
	int dax;
	int day;
	int ox;
	int oy;

	ox = *x; oy = *y;

	for(i=0;i<warpsectorcnt;i++)
		if (warpsectorlist[i] == *dasector)
		{
			j = g_sector[*dasector].hitag;
			do
			{
				i++;
				if (i >= warpsectorcnt) i = 0;
			} while (g_sector[warpsectorlist[i]].hitag != j);
			*dasector = warpsectorlist[i];
			break;
		}

		//Find center of sector
	startwall = g_sector[*dasector].wallptr;
	endwall = startwall+g_sector[*dasector].wallnum;
	dax = 0L, day = 0L;
	for(s=startwall;s<endwall;s++)
	{
		dax += wall[s].pt.x, day += wall[s].pt.y;
		if (wall[s].nextsector >= 0)
			i = s;
	}
	*x = dax / (endwall-startwall);
	*y = day / (endwall-startwall);
	*z = g_sector[*dasector].floorz-(32<<8);
	updatesector(*x,*y,dasector);
	dax = ((wall[i].pt.x+wall[wall[i].point2].pt.x)>>1);
	day = ((wall[i].pt.y+wall[wall[i].point2].pt.y)>>1);
	*daang = getangle(dax-*x,day-*y);

	wsayfollow("warp.wav",3072L+(krand()&127)-64,192L,&ox,&oy,0);
	wsayfollow("warp.wav",4096L+(krand()&127)-64,256L,x,y,0);
}

void warpsprite(short spritenum)
{
	short dasectnum;

	dasectnum = sprite[spritenum].sectnum;
	warp(&sprite[spritenum].x,&sprite[spritenum].y,&sprite[spritenum].z,
		  &sprite[spritenum].ang,&dasectnum);

	copybuf(&sprite[spritenum].x,&osprite[spritenum].x,3);
	changespritesect(spritenum,dasectnum);

	show2dsprite[spritenum>>3] &= ~(1<<(spritenum&7));
	if (show2dsector[dasectnum>>3]&(1<<(dasectnum&7)))
		show2dsprite[spritenum>>3] |= (1<<(spritenum&7));
}

void initlava()
{
	int x;
	int y;
	int z;
	int r;

	for(z=0;z<32;z++) lavaradcnt[z] = 0;
	for(x=-16;x<=16;x++)
		for(y=-16;y<=16;y++)
		{
			r = static_cast<int>(std::hypot(x, y));
			lavaradx[r][lavaradcnt[r]] = x;
			lavarady[r][lavaradcnt[r]] = y;
			lavaradcnt[r]++;
		}

	for(z=0;z<16;z++)
		lavadropsizlookup[z] = 8 / (std::sqrt(z)+1);

	for(z=0;z<LAVASIZ;z++)
		lavainc[z] = std::abs((((z^17)>>4)&7)-4)+12;

	lavanumdrops = 0;
	lavanumframes = 0;
}

#if defined(__WATCOMC__) && USE_ASM
#pragma aux addlava =\
	"mov al, byte ptr [ebx-133]",\
	"mov dl, byte ptr [ebx-1]",\
	"add al, byte ptr [ebx-132]",\
	"add dl, byte ptr [ebx+131]",\
	"add al, byte ptr [ebx-131]",\
	"add dl, byte ptr [ebx+132]",\
	"add al, byte ptr [ebx+1]",\
	"add al, dl",\
	parm [ebx]\
	modify exact [eax edx]
int addlava(int);
#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM
inline int addlava(void *b)
{
	_asm {
	mov ebx, b
	mov al, byte ptr [ebx-133]
	mov dl, byte ptr [ebx-1]
	add al, byte ptr [ebx-132]
	add dl, byte ptr [ebx+131]
	add al, byte ptr [ebx-131]
	add dl, byte ptr [ebx+132]
	add al, byte ptr [ebx+1]
	add al, dl
	}
}
#elif defined(__GNUC__) && defined(__i386__) && USE_ASM
int addlava(void *b)
{
	int r;
	__asm__ __volatile__ (
		"movb -133(%%ebx), %%al\n\t"
		"movb -1(%%ebx), %%dl\n\t"
		"addb -132(%%ebx), %%al\n\t"
		"addb 131(%%ebx), %%dl\n\t"
		"addb -131(%%ebx), %%al\n\t"
		"addb 132(%%ebx), %%dl\n\t"
		"addb 1(%%ebx), %%al\n\t"
		"addb %%dl, %%al"
		: "=a" (r) : "b" (b)
		: "dx"
	);
	return r;
}
#else
int addlava(void *bx)
{
	const unsigned char *b = (unsigned char *)bx;
	return b[-133] + b[-132] + b[-131] + b[1] + b[-1] + b[131] + b[132];
}
#endif

void movelava(unsigned char *dapic)
{
	int i;
	int x;
	int y;
	int z;
	int zz;
	int dalavadropsiz;
	int dadropsizlookup;
	int dalavax;
	int dalavay;
	int *ptr;
	int *ptr2;
	unsigned char *pi;
	unsigned char *pj;
	unsigned char *py;

	for(z = std::min(LAVAMAXDROPS - lavanumdrops - 1, 3); z >= 0; z--)
	{
		lavadropx[lavanumdrops] = (std::rand()&(LAVASIZ-1));
		lavadropy[lavanumdrops] = (std::rand()&(LAVASIZ-1));
		lavadropsiz[lavanumdrops] = 1;
		lavanumdrops++;
	}

	for(z=lavanumdrops-1;z>=0;z--)
	{
		dadropsizlookup = lavadropsizlookup[lavadropsiz[z]]*(((z&1)<<1)-1);
		dalavadropsiz = lavadropsiz[z];
		dalavax = lavadropx[z]; dalavay = lavadropy[z];
		for(zz=lavaradcnt[lavadropsiz[z]]-1;zz>=0;zz--)
		{
			i = (((lavaradx[dalavadropsiz][zz]+dalavax)&(LAVASIZ-1))<<LAVALOGSIZ);
			i += ((lavarady[dalavadropsiz][zz]+dalavay)&(LAVASIZ-1));
			dapic[i] += dadropsizlookup;
			if (dapic[i] < 192) dapic[i] = 192;
		}

		lavadropsiz[z]++;
		if (lavadropsiz[z] > 10)
		{
			lavanumdrops--;
			lavadropx[z] = lavadropx[lavanumdrops];
			lavadropy[z] = lavadropy[lavanumdrops];
			lavadropsiz[z] = lavadropsiz[lavanumdrops];
		}
	}

		//Back up dapic with 1 pixel extra on each boundary
		//(to prevent anding for wrap-around)
	ptr = (int *)dapic;
	ptr2 = (int *)((LAVASIZ+4)+1+((intptr_t)lavabakpic));
	for(x=0;x<LAVASIZ;x++)
	{
		for(y=(LAVASIZ>>2);y>0;y--) *ptr2++ = ((*ptr++)&0x1f1f1f1f);
		ptr2++;
	}
	for(y=0;y<LAVASIZ;y++)
	{
		lavabakpic[y+1] = (dapic[y+((LAVASIZ-1)<<LAVALOGSIZ)]&31);
		lavabakpic[y+1+(LAVASIZ+1)*(LAVASIZ+4)] = (dapic[y]&31);
	}
	for(x=0;x<LAVASIZ;x++)
	{
		lavabakpic[(x+1)*(LAVASIZ+4)] = (dapic[(x<<LAVALOGSIZ)+(LAVASIZ-1)]&31);
		lavabakpic[(x+1)*(LAVASIZ+4)+(LAVASIZ+1)] = (dapic[x<<LAVALOGSIZ]&31);
	}
	lavabakpic[0] = (dapic[LAVASIZ*LAVASIZ-1]&31);
	lavabakpic[LAVASIZ+1] = (dapic[LAVASIZ*(LAVASIZ-1)]&31);
	lavabakpic[(LAVASIZ+4)*(LAVASIZ+1)] = (dapic[LAVASIZ-1]&31);
	lavabakpic[(LAVASIZ+4)*(LAVASIZ+2)-1] = (dapic[0]&31);

	ptr = (int *)dapic;
	for(x=0;x<LAVASIZ;x++)
	{
		pi = &lavabakpic[(x+1)*(LAVASIZ+4)+1];
		pj = pi+LAVASIZ;
		for(py=pi;py<pj;py+=4)
		{
			*ptr++ = ((addlava(&py[0])&0xf8)>>3)+
				((addlava(&py[1])&0xf8)<<5)+
				((addlava(&py[2])&0xf8)<<13)+
				((addlava(&py[3])&0xf8)<<21)+
				0xc2c2c2c2;
		}
	}

	lavanumframes++;
}

void doanimations()
{
	int i;
	int j;

	for(i=animatecnt-1;i>=0;i--)
	{
		j = *animateptr[i];

		if (j < animategoal[i])
			j = std::min(j + animatevel[i] * TICSPERFRAME, animategoal[i]);
		else
			j = std::max(j - animatevel[i] * TICSPERFRAME, animategoal[i]);
		animatevel[i] += animateacc[i];

		*animateptr[i] = j;

		if (j == animategoal[i])
		{
			animatecnt--;
			if (i != animatecnt)
			{
				stopinterpolation(animateptr[i]);
				animateptr[i] = animateptr[animatecnt];
				animategoal[i] = animategoal[animatecnt];
				animatevel[i] = animatevel[animatecnt];
				animateacc[i] = animateacc[animatecnt];
			}
		}
	}
}

int getanimationgoal(int *animptr)
{
	int i;

	for(i=animatecnt-1;i>=0;i--)
		if (animptr == animateptr[i]) return(i);
	return(-1);
}

int setanimation(int *animptr, int thegoal, int thevel, int theacc)
{
	int i;
	int j;

	if (animatecnt >= MAXANIMATES) return(-1);

	j = animatecnt;
	for(i=animatecnt-1;i>=0;i--)
		if (animptr == animateptr[i])
			{ j = i; break; }

	setinterpolation(animptr);

	animateptr[j] = animptr;
	animategoal[j] = thegoal;
	animatevel[j] = thevel;
	animateacc[j] = theacc;
	if (j == animatecnt) animatecnt++;
	return(j);
}

void checkmasterslaveswitch()
{
	int i;
	int j;

	if (option[4] == 0) return;

	j = 0;
	for(i=connecthead;i>=0;i=connectpoint2[i])
		if (ssync[i].bits&512) j++;
	if (j != 1) return;

	i = connecthead;
	for(j=connectpoint2[i];j>=0;j=connectpoint2[j])
	{
		if (ssync[j].bits&512)
		{
			connectpoint2[i] = connectpoint2[j];
			connectpoint2[j] = connecthead;
			connecthead = (short)j;

			oloc.fvel = loc.fvel+1;
			oloc.svel = loc.svel+1;
			oloc.avel = loc.avel+1;
			oloc.bits = loc.bits+1;
			for(i=0;i<MAXPLAYERS;i++)
			{
				osync[i].fvel = ffsync[i].fvel+1;
				osync[i].svel = ffsync[i].svel+1;
				osync[i].avel = ffsync[i].avel+1;
				osync[i].bits = ffsync[i].bits+1;
			}

			syncvalhead = othersyncvalhead = syncvaltail = 0L;
			totalclock = ototalclock = gotlastpacketclock = lockclock;

			j = 1;
			for(i=connecthead;i>=0;i=connectpoint2[i])
			{
				if (myconnectindex == i) break;
				j++;
			}
			if (j == 1)
				std::strcpy(getmessage,"Player 1 (Master)");
			else
				std::sprintf(getmessage,"Player %d (Slave)",j);
			getmessageleng = (int)std::strlen(getmessage);
			getmessagetimeoff = totalclock+120;

			return;
		}
		i = j;
	}
}


int testneighborsectors(short sect1, short sect2)
{
	short i;
	short startwall;
	short num1;
	short num2;

	num1 = g_sector[sect1].wallnum;
	num2 = g_sector[sect2].wallnum;
	if (num1 < num2) //Traverse walls of sector with fewest walls (for speed)
	{
		startwall = g_sector[sect1].wallptr;
		for(i=num1-1;i>=0;i--)
			if (wall[i+startwall].nextsector == sect2)
				return(1);
	}
	else
	{
		startwall = g_sector[sect2].wallptr;
		for(i=num2-1;i>=0;i--)
			if (wall[i+startwall].nextsector == sect1)
				return(1);
	}
	return(0);
}

int loadgame()
{
	int i;
	int fil;
	std::array<int, MAXANIMATES> tmpanimateptr;

	if ((fil = kopen4load("save0000.gam",0)) == -1) return(-1);

	kdfread(&numplayers,4,1,fil);
	kdfread(&myconnectindex,4,1,fil);
	kdfread(&connecthead,4,1,fil);
	kdfread(&connectpoint2[0], 4, MAXPLAYERS, fil);

		//Make sure palookups get set, sprites will get overwritten later
	for(i=connecthead;i>=0;i=connectpoint2[i]) initplayersprite((short)i);

	kdfread(posx,4,MAXPLAYERS,fil);
	kdfread(posy,4,MAXPLAYERS,fil);
	kdfread(posz,4,MAXPLAYERS,fil);
	kdfread(horiz,4,MAXPLAYERS,fil);
	kdfread(zoom,4,MAXPLAYERS,fil);
	kdfread(hvel,4,MAXPLAYERS,fil);
	kdfread(ang,2,MAXPLAYERS,fil);
	kdfread(cursectnum,2,MAXPLAYERS,fil);
	kdfread(ocursectnum,2,MAXPLAYERS,fil);
	kdfread(playersprite,2,MAXPLAYERS,fil);
	kdfread(deaths,2,MAXPLAYERS,fil);
	kdfread(lastchaingun,4,MAXPLAYERS,fil);
	kdfread(health,4,MAXPLAYERS,fil);
	kdfread(numgrabbers,2,MAXPLAYERS,fil);
	kdfread(nummissiles,2,MAXPLAYERS,fil);
	kdfread(numbombs,2,MAXPLAYERS,fil);
	kdfread(flytime,4,MAXPLAYERS,fil);
	kdfread(oflags,2,MAXPLAYERS,fil);
	kdfread(dimensionmode,1,MAXPLAYERS,fil);
	kdfread(revolvedoorstat,1,MAXPLAYERS,fil);
	kdfread(revolvedoorang,2,MAXPLAYERS,fil);
	kdfread(revolvedoorrotang,2,MAXPLAYERS,fil);
	kdfread(revolvedoorx,4,MAXPLAYERS,fil);
	kdfread(revolvedoory,4,MAXPLAYERS,fil);

	kdfread(&numsectors,2,1,fil);
	kdfread(&g_sector[0],sizeof(sectortype),numsectors,fil);
	kdfread(&numwalls,2,1,fil);
	kdfread(&wall[0],sizeof(walltype),numwalls,fil);
		//Store all sprites (even holes) to preserve indeces
	kdfread(&sprite[0],sizeof(spritetype),MAXSPRITES,fil);
	kdfread(&headspritesect[0],2,MAXSECTORS+1,fil);
	kdfread(&prevspritesect[0],2,MAXSPRITES,fil);
	kdfread(&nextspritesect[0],2,MAXSPRITES,fil);
	kdfread(&headspritestat[0],2,MAXSTATUS+1,fil);
	kdfread(&prevspritestat[0],2,MAXSPRITES,fil);
	kdfread(&nextspritestat[0],2,MAXSPRITES,fil);

	kdfread(&fvel,4,1,fil);
	kdfread(&svel,4,1,fil);
	kdfread(&avel,4,1,fil);

	kdfread(&locselectedgun,4,1,fil);
	kdfread(&loc.fvel,1,1,fil);
	kdfread(&oloc.fvel,1,1,fil);
	kdfread(&loc.svel,1,1,fil);
	kdfread(&oloc.svel,1,1,fil);
	kdfread(&loc.avel,1,1,fil);
	kdfread(&oloc.avel,1,1,fil);
	kdfread(&loc.bits,2,1,fil);
	kdfread(&oloc.bits,2,1,fil);

	kdfread(&locselectedgun2,4,1,fil);
	kdfread(&loc2.fvel,sizeof(input),1,fil);

	kdfread(&ssync[0], sizeof(input), MAXPLAYERS, fil);
	kdfread(&osync[0], sizeof(input), MAXPLAYERS, fil);

	kdfread(boardfilename,1,80,fil);
	kdfread(&screenpeek,2,1,fil);
	kdfread(&oldmousebstatus,2,1,fil);
	kdfread(&brightness,2,1,fil);
	kdfread(&neartagsector,2,1,fil);
	kdfread(&neartagwall,2,1,fil);
	kdfread(&neartagsprite,2,1,fil);
	kdfread(&lockclock,4,1,fil);
	kdfread(&neartagdist,4,1,fil);
	kdfread(&neartaghitdist,4,1,fil);

	kdfread(turnspritelist,2,16,fil);
	kdfread(&turnspritecnt,2,1,fil);
	kdfread(warpsectorlist,2,16,fil);
	kdfread(&warpsectorcnt,2,1,fil);
	kdfread(xpanningsectorlist,2,16,fil);
	kdfread(&xpanningsectorcnt,2,1,fil);
	kdfread(ypanningwalllist,2,64,fil);
	kdfread(&ypanningwallcnt,2,1,fil);
	kdfread(floorpanninglist,2,64,fil);
	kdfread(&floorpanningcnt,2,1,fil);
	kdfread(dragsectorlist,2,16,fil);
	kdfread(dragxdir,2,16,fil);
	kdfread(dragydir,2,16,fil);
	kdfread(&dragsectorcnt,2,1,fil);
	kdfread(dragx1,4,16,fil);
	kdfread(dragy1,4,16,fil);
	kdfread(dragx2,4,16,fil);
	kdfread(dragy2,4,16,fil);
	kdfread(dragfloorz,4,16,fil);
	kdfread(&swingcnt,2,1,fil);
	kdfread(swingwall,2,32*5,fil);
	kdfread(swingsector,2,32,fil);
	kdfread(swingangopen,2,32,fil);
	kdfread(swingangclosed,2,32,fil);
	kdfread(swingangopendir,2,32,fil);
	kdfread(swingang,2,32,fil);
	kdfread(swinganginc,2,32,fil);
	kdfread(swingx,4,32*8,fil);
	kdfread(swingy,4,32*8,fil);
	kdfread(revolvesector,2,4,fil);
	kdfread(revolveang,2,4,fil);
	kdfread(&revolvecnt,2,1,fil);
	kdfread(revolvex,4,4*16,fil);
	kdfread(revolvey,4,4*16,fil);
	kdfread(revolvepivotx,4,4,fil);
	kdfread(revolvepivoty,4,4,fil);
	kdfread(subwaytracksector,2,4*128,fil);
	kdfread(subwaynumsectors,2,4,fil);
	kdfread(&subwaytrackcnt,2,1,fil);
	kdfread(subwaystop,4,4*8,fil);
	kdfread(subwaystopcnt,4,4,fil);
	kdfread(subwaytrackx1,4,4,fil);
	kdfread(subwaytracky1,4,4,fil);
	kdfread(subwaytrackx2,4,4,fil);
	kdfread(subwaytracky2,4,4,fil);
	kdfread(subwayx,4,4,fil);
	kdfread(subwaygoalstop,4,4,fil);
	kdfread(subwayvel,4,4,fil);
	kdfread(subwaypausetime,4,4,fil);
	kdfread(waterfountainwall,2,MAXPLAYERS,fil);
	kdfread(waterfountaincnt,2,MAXPLAYERS,fil);
	kdfread(slimesoundcnt,2,MAXPLAYERS,fil);

		//Warning: only works if all pointers are in sector structures!
	kdfread(&tmpanimateptr[0], 4, MAXANIMATES, fil);
	for(i=MAXANIMATES-1;i>=0;i--)
		animateptr[i] = (int *)(tmpanimateptr[i]+(intptr_t)&g_sector[0]);

	kdfread(animategoal,4,MAXANIMATES,fil);
	kdfread(animatevel,4,MAXANIMATES,fil);
	kdfread(animateacc,4,MAXANIMATES,fil);
	kdfread(&animatecnt,4,1,fil);

	kdfread(&totalclock,4,1,fil);
	kdfread(&numframes,4,1,fil);
	kdfread(&randomseed,4,1,fil);
	kdfread(&numpalookups,2,1,fil);

	kdfread(&visibility,4,1,fil);
	kdfread(&parallaxvisibility,4,1,fil);
	kdfread(&parallaxtype,1,1,fil);
	kdfread(&parallaxyoffs,4,1,fil);
	kdfread(&pskyoff[0],2,MAXPSKYTILES,fil);
	kdfread(&pskybits,2,1,fil);

	kdfread(&mirrorcnt,2,1,fil);
	kdfread(mirrorwall,2,mirrorcnt,fil);
	kdfread(mirrorsector,2,mirrorcnt,fil);

		//I should save off interpolation list, but they're pointers :(
	numinterpolations = 0;
	startofdynamicinterpolations = 0;

	kclose(fil);

	for(i=connecthead;i>=0;i=connectpoint2[i]) initplayersprite((short)i);

	totalclock = lockclock;
	ototalclock = lockclock;

	std::strcpy(getmessage,"Game loaded.");
	getmessageleng = (int)std::strlen(getmessage);
	getmessagetimeoff = totalclock+360+(getmessageleng<<4);
	return(0);
}

int savegame()
{
	int i;
	std::FILE *fil;
	int tmpanimateptr[MAXANIMATES];

	if ((fil = std::fopen("save0000.gam","wb")) == nullptr) return(-1);

	dfwrite(&numplayers,4,1,fil);
	dfwrite(&myconnectindex,4,1,fil);
	dfwrite(&connecthead,4,1,fil);
	dfwrite(&connectpoint2[0], 4, MAXPLAYERS, fil);

	dfwrite(posx,4,MAXPLAYERS,fil);
	dfwrite(posy,4,MAXPLAYERS,fil);
	dfwrite(posz,4,MAXPLAYERS,fil);
	dfwrite(horiz,4,MAXPLAYERS,fil);
	dfwrite(zoom,4,MAXPLAYERS,fil);
	dfwrite(hvel,4,MAXPLAYERS,fil);
	dfwrite(ang,2,MAXPLAYERS,fil);
	dfwrite(cursectnum,2,MAXPLAYERS,fil);
	dfwrite(ocursectnum,2,MAXPLAYERS,fil);
	dfwrite(playersprite,2,MAXPLAYERS,fil);
	dfwrite(deaths,2,MAXPLAYERS,fil);
	dfwrite(lastchaingun,4,MAXPLAYERS,fil);
	dfwrite(health,4,MAXPLAYERS,fil);
	dfwrite(numgrabbers,2,MAXPLAYERS,fil);
	dfwrite(nummissiles,2,MAXPLAYERS,fil);
	dfwrite(numbombs,2,MAXPLAYERS,fil);
	dfwrite(flytime,4,MAXPLAYERS,fil);
	dfwrite(oflags,2,MAXPLAYERS,fil);
	dfwrite(dimensionmode,1,MAXPLAYERS,fil);
	dfwrite(revolvedoorstat,1,MAXPLAYERS,fil);
	dfwrite(revolvedoorang,2,MAXPLAYERS,fil);
	dfwrite(revolvedoorrotang,2,MAXPLAYERS,fil);
	dfwrite(revolvedoorx,4,MAXPLAYERS,fil);
	dfwrite(revolvedoory,4,MAXPLAYERS,fil);

	dfwrite(&numsectors,2,1,fil);
	dfwrite(&g_sector[0],sizeof(sectortype),numsectors,fil);
	dfwrite(&numwalls,2,1,fil);
	dfwrite(&wall[0],sizeof(walltype),numwalls,fil);
		//Store all sprites (even holes) to preserve indeces
	dfwrite(&sprite[0],sizeof(spritetype),MAXSPRITES,fil);
	dfwrite(&headspritesect[0],2,MAXSECTORS+1,fil);
	dfwrite(&prevspritesect[0],2,MAXSPRITES,fil);
	dfwrite(&nextspritesect[0],2,MAXSPRITES,fil);
	dfwrite(&headspritestat[0],2,MAXSTATUS+1,fil);
	dfwrite(&prevspritestat[0],2,MAXSPRITES,fil);
	dfwrite(&nextspritestat[0],2,MAXSPRITES,fil);

	dfwrite(&fvel,4,1,fil);
	dfwrite(&svel,4,1,fil);
	dfwrite(&avel,4,1,fil);

	dfwrite(&locselectedgun,4,1,fil);
	dfwrite(&loc.fvel,1,1,fil);
	dfwrite(&oloc.fvel,1,1,fil);
	dfwrite(&loc.svel,1,1,fil);
	dfwrite(&oloc.svel,1,1,fil);
	dfwrite(&loc.avel,1,1,fil);
	dfwrite(&oloc.avel,1,1,fil);
	dfwrite(&loc.bits,2,1,fil);
	dfwrite(&oloc.bits,2,1,fil);

	dfwrite(&locselectedgun2,4,1,fil);
	dfwrite(&loc2.fvel,sizeof(input),1,fil);

	dfwrite(&ssync[0], sizeof(input), MAXPLAYERS, fil);
	dfwrite(&osync[0], sizeof(input), MAXPLAYERS, fil);

	dfwrite(boardfilename,1,80,fil);
	dfwrite(&screenpeek,2,1,fil);
	dfwrite(&oldmousebstatus,2,1,fil);
	dfwrite(&brightness,2,1,fil);
	dfwrite(&neartagsector,2,1,fil);
	dfwrite(&neartagwall,2,1,fil);
	dfwrite(&neartagsprite,2,1,fil);
	dfwrite(&lockclock,4,1,fil);
	dfwrite(&neartagdist,4,1,fil);
	dfwrite(&neartaghitdist,4,1,fil);

	dfwrite(turnspritelist,2,16,fil);
	dfwrite(&turnspritecnt,2,1,fil);
	dfwrite(warpsectorlist,2,16,fil);
	dfwrite(&warpsectorcnt,2,1,fil);
	dfwrite(xpanningsectorlist,2,16,fil);
	dfwrite(&xpanningsectorcnt,2,1,fil);
	dfwrite(ypanningwalllist,2,64,fil);
	dfwrite(&ypanningwallcnt,2,1,fil);
	dfwrite(floorpanninglist,2,64,fil);
	dfwrite(&floorpanningcnt,2,1,fil);
	dfwrite(dragsectorlist,2,16,fil);
	dfwrite(dragxdir,2,16,fil);
	dfwrite(dragydir,2,16,fil);
	dfwrite(&dragsectorcnt,2,1,fil);
	dfwrite(dragx1,4,16,fil);
	dfwrite(dragy1,4,16,fil);
	dfwrite(dragx2,4,16,fil);
	dfwrite(dragy2,4,16,fil);
	dfwrite(dragfloorz,4,16,fil);
	dfwrite(&swingcnt,2,1,fil);
	dfwrite(swingwall,2,32*5,fil);
	dfwrite(swingsector,2,32,fil);
	dfwrite(swingangopen,2,32,fil);
	dfwrite(swingangclosed,2,32,fil);
	dfwrite(swingangopendir,2,32,fil);
	dfwrite(swingang,2,32,fil);
	dfwrite(swinganginc,2,32,fil);
	dfwrite(swingx,4,32*8,fil);
	dfwrite(swingy,4,32*8,fil);
	dfwrite(revolvesector,2,4,fil);
	dfwrite(revolveang,2,4,fil);
	dfwrite(&revolvecnt,2,1,fil);
	dfwrite(revolvex,4,4*16,fil);
	dfwrite(revolvey,4,4*16,fil);
	dfwrite(revolvepivotx,4,4,fil);
	dfwrite(revolvepivoty,4,4,fil);
	dfwrite(subwaytracksector,2,4*128,fil);
	dfwrite(subwaynumsectors,2,4,fil);
	dfwrite(&subwaytrackcnt,2,1,fil);
	dfwrite(subwaystop,4,4*8,fil);
	dfwrite(subwaystopcnt,4,4,fil);
	dfwrite(subwaytrackx1,4,4,fil);
	dfwrite(subwaytracky1,4,4,fil);
	dfwrite(subwaytrackx2,4,4,fil);
	dfwrite(subwaytracky2,4,4,fil);
	dfwrite(subwayx,4,4,fil);
	dfwrite(subwaygoalstop,4,4,fil);
	dfwrite(subwayvel,4,4,fil);
	dfwrite(subwaypausetime,4,4,fil);
	dfwrite(waterfountainwall,2,MAXPLAYERS,fil);
	dfwrite(waterfountaincnt,2,MAXPLAYERS,fil);
	dfwrite(slimesoundcnt,2,MAXPLAYERS,fil);

		//Warning: only works if all pointers are in sector structures!
	for(i=MAXANIMATES-1;i>=0;i--)
		tmpanimateptr[i] = (int)((intptr_t)animateptr[i]-(intptr_t)&g_sector[0]);
	dfwrite(tmpanimateptr,4,MAXANIMATES,fil);

	dfwrite(animategoal,4,MAXANIMATES,fil);
	dfwrite(animatevel,4,MAXANIMATES,fil);
	dfwrite(animateacc,4,MAXANIMATES,fil);
	dfwrite(&animatecnt,4,1,fil);

	dfwrite(&totalclock,4,1,fil);
	dfwrite(&numframes,4,1,fil);
	dfwrite(&randomseed,4,1,fil);
	dfwrite(&numpalookups,2,1,fil);

	dfwrite(&visibility,4,1,fil);
	dfwrite(&parallaxvisibility,4,1,fil);
	dfwrite(&parallaxtype,1,1,fil);
	dfwrite(&parallaxyoffs,4,1,fil);
	dfwrite(&pskyoff[0],2,MAXPSKYTILES,fil);
	dfwrite(&pskybits,2,1,fil);

	dfwrite(&mirrorcnt,2,1,fil);
	dfwrite(mirrorwall,2,mirrorcnt,fil);
	dfwrite(mirrorsector,2,mirrorcnt,fil);

	std::fclose(fil);

	std::strcpy(getmessage,"Game saved.");
	getmessageleng = (int)std::strlen(getmessage);
	getmessagetimeoff = totalclock+360+(getmessageleng<<4);
	return(0);
}

void faketimerhandler()
{
	short other;
	int i;
	int j;
	int k;
	int l;

	sampletimer();
	if ((totalclock < ototalclock+(TIMERINTSPERSECOND/MOVESPERSECOND)) || (ready2send == 0)) return;
	ototalclock += (TIMERINTSPERSECOND/MOVESPERSECOND);

	getpackets();
	getinput();

	/*
	for(i=connecthead;i>=0;i=connectpoint2[i])
		if (i != myconnectindex)
		{
			k = (movefifoend[myconnectindex]-1)-movefifoend[i];
			myminlag[i] = std::min(myminlag[i], k);
			mymaxlag = std::max(mymaxlag, k);
		}

	if (((movefifoend[myconnectindex]-1)&(TIMERUPDATESIZ-1)) == 0)
	{
		i = mymaxlag-bufferjitter; mymaxlag = 0;
		if (i > 0) bufferjitter += ((2+i)>>2);
		else if (i < 0) bufferjitter -= ((2-i)>>2);
	}
	*/

	if (networkmode == 1)
	{
		packbuf[2] = 0; j = 3;
		if (loc.fvel != oloc.fvel) packbuf[j++] = loc.fvel, packbuf[2] |= 1;
		if (loc.svel != oloc.svel) packbuf[j++] = loc.svel, packbuf[2] |= 2;
		if (loc.avel != oloc.avel) packbuf[j++] = loc.avel, packbuf[2] |= 4;
		if ((loc.bits^oloc.bits)&0x00ff) packbuf[j++] = (loc.bits&255), packbuf[2] |= 8;
		if ((loc.bits^oloc.bits)&0xff00) packbuf[j++] = ((loc.bits>>8)&255), packbuf[2] |= 16;
		copybufbyte(&loc,&oloc,sizeof(input));

		copybufbyte(&loc,&baksync[movefifoend[myconnectindex]][myconnectindex],sizeof(input));
		movefifoend[myconnectindex] = ((movefifoend[myconnectindex]+1)&(MOVEFIFOSIZ-1));

		for(i=connecthead;i>=0;i=connectpoint2[i])
			if (i != myconnectindex)
			{
				packbuf[0] = 17;
				packbuf[1] = (unsigned char)((movefifoend[myconnectindex]-movefifoend[i])&(MOVEFIFOSIZ-1));

				k = j;
				if ((myconnectindex == connecthead) || ((i == connecthead) && (myconnectindex == connectpoint2[connecthead])))
				{
					while (syncvalhead != syncvaltail)
					{
						packbuf[j++] = syncval[syncvaltail];
						syncvaltail = ((syncvaltail+1)&(MOVEFIFOSIZ-1));
					}
				}
				sendpacket(i,packbuf,j);
				j = k;
			}

		gotlastpacketclock = totalclock;
		return;
	}

		//MASTER (or 1 player game)
	if ((myconnectindex == connecthead) || (option[4] == 0))
	{
		copybufbyte(&loc,&ffsync[myconnectindex],sizeof(input));

		if (option[4] != 0)
		{
			packbuf[0] = 0;
			j = ((numplayers+1)>>1)+1;
			for(k=1;k<j;k++) packbuf[k] = 0;
			k = (1<<3);
			for(i=connecthead;i>=0;i=connectpoint2[i])
			{
				l = 0;
				if (ffsync[i].fvel != osync[i].fvel) packbuf[j++] = ffsync[i].fvel, l |= 1;
				if (ffsync[i].svel != osync[i].svel) packbuf[j++] = ffsync[i].svel, l |= 2;
				if (ffsync[i].avel != osync[i].avel) packbuf[j++] = ffsync[i].avel, l |= 4;
				if (ffsync[i].bits != osync[i].bits)
				{
					packbuf[j++] = (ffsync[i].bits&255);
					packbuf[j++] = ((ffsync[i].bits>>8)&255);
					l |= 8;
				}
				packbuf[k>>3] |= (l<<(k&7));
				k += 4;

				copybufbyte(&ffsync[i],&osync[i],sizeof(input));
			}

			while (syncvalhead != syncvaltail)
			{
				packbuf[j++] = syncval[syncvaltail];
				syncvaltail = ((syncvaltail+1)&(MOVEFIFOSIZ-1));
			}

			for(i=connectpoint2[connecthead];i>=0;i=connectpoint2[i])
				sendpacket(i,packbuf,j);
		}
		else if (numplayers >= 2)
		{
			if (keystatus[0xb5])
			{
				keystatus[0xb5] = 0;
				locselectedgun2++; if (locselectedgun2 >= 3) locselectedgun2 = 0;
			}

				//Second player on 1 computer mode
			loc2.fvel = std::min(std::max(fvel2, -128 + 8), 127 - 8);
			loc2.svel = std::min(std::max(svel2, -128 + 8), 127 - 8);
			loc2.avel = std::min(std::max(avel2, -128 + 16), 127 - 16);
			loc2.bits = (locselectedgun2<<13);
			loc2.bits |= keystatus[0x45];                  //Stand high
			loc2.bits |= (keystatus[0x47]<<1);             //Stand low
			loc2.bits |= (1<<8);                           //Run
			loc2.bits |= (keystatus[0x49]<<2);             //Look up
			loc2.bits |= (keystatus[0x37]<<3);             //Look down
			loc2.bits |= (keystatus[0x50]<<10);            //Space
			loc2.bits |= (keystatus[0x52]<<11);            //Shoot

			other = connectpoint2[myconnectindex];
			if (other < 0) other = connecthead;

			copybufbyte(&loc2,&ffsync[other],sizeof(input));
		}
		movethings();  //Move EVERYTHING (you too!)
	}
	else                        //I am a SLAVE
	{
		packbuf[0] = 1; packbuf[1] = 0; j = 2;
		if (loc.fvel != oloc.fvel) packbuf[j++] = loc.fvel, packbuf[1] |= 1;
		if (loc.svel != oloc.svel) packbuf[j++] = loc.svel, packbuf[1] |= 2;
		if (loc.avel != oloc.avel) packbuf[j++] = loc.avel, packbuf[1] |= 4;
		if ((loc.bits^oloc.bits)&0x00ff) packbuf[j++] = (loc.bits&255), packbuf[1] |= 8;
		if ((loc.bits^oloc.bits)&0xff00) packbuf[j++] = ((loc.bits>>8)&255), packbuf[1] |= 16;
		copybufbyte(&loc,&oloc,sizeof(input));
		sendpacket(connecthead,packbuf,j);
	}
}

void getpackets()
{
	int i;
	int j;
	int k;
	int l;
	int other;
	int packbufleng;
	int movecnt;

	if (option[4] == 0) return;

	movecnt = 0;
	while ((packbufleng = getpacket(&other,packbuf)) > 0)
	{
		switch(packbuf[0])
		{
			case 0:  //[0] (receive master sync buffer)
				j = ((numplayers+1)>>1)+1; k = (1<<3);
				for(i=connecthead;i>=0;i=connectpoint2[i])
				{
					l = (packbuf[k>>3]>>(k&7));
					if (l&1) ffsync[i].fvel = packbuf[j++];
					if (l&2) ffsync[i].svel = packbuf[j++];
					if (l&4) ffsync[i].avel = packbuf[j++];
					if (l&8)
					{
						ffsync[i].bits = ((short)packbuf[j])+(((short)packbuf[j+1])<<8);
						j += 2;
					}
					k += 4;
				}

				while (j != packbufleng)
				{
					 othersyncval[othersyncvalhead] = packbuf[j++];
					 othersyncvalhead = ((othersyncvalhead+1)&(MOVEFIFOSIZ-1));
				}
				if ((syncvalhead != syncvaltottail) && (othersyncvalhead != syncvaltottail))
				{
					syncstat = 0;
					do
					{
						syncstat |= (syncval[syncvaltottail]^othersyncval[syncvaltottail]);
						syncvaltottail = ((syncvaltottail+1)&(MOVEFIFOSIZ-1));
					} while ((syncvalhead != syncvaltottail) && (othersyncvalhead != syncvaltottail));
				}

				movethings();        //Move all players and sprites
				movecnt++;
				break;
			case 1:  //[1] (receive slave sync buffer)
				j = 2; k = packbuf[1];
				if (k&1) ffsync[other].fvel = packbuf[j++];
				if (k&2) ffsync[other].svel = packbuf[j++];
				if (k&4) ffsync[other].avel = packbuf[j++];
				if (k&8) ffsync[other].bits = ((ffsync[other].bits&0xff00)|((short)packbuf[j++]));
				if (k&16) ffsync[other].bits = ((ffsync[other].bits&0x00ff)|(((short)packbuf[j++])<<8));
				break;
			case 2:
				getmessageleng = packbufleng-1;
				for(j=getmessageleng-1;j>=0;j--) getmessage[j] = packbuf[j+1];
				getmessagetimeoff = totalclock+360+(getmessageleng<<4);
				wsay("getstuff.wav", 8192L, 63L, 63L); //Added 12/2004
				break;
			case 3:
				wsay("getstuff.wav", 4096L, 63L, 63L);
				break;
				/*
			case 5:
				playerreadyflag[other] = packbuf[1];
				if ((other == connecthead) && (packbuf[1] == 2))
					sendpacket(connecthead,packbuf,2);
				break;
				*/
			case 250:
				playerreadyflag[other]++;
				break;
			case 17:
				j = 3; k = packbuf[2];
				if (k&1) ffsync[other].fvel = packbuf[j++];
				if (k&2) ffsync[other].svel = packbuf[j++];
				if (k&4) ffsync[other].avel = packbuf[j++];
				if (k&8) ffsync[other].bits = ((ffsync[other].bits&0xff00)|((short)packbuf[j++]));
				if (k&16) ffsync[other].bits = ((ffsync[other].bits&0x00ff)|(((short)packbuf[j++])<<8));
				otherlag[other] = packbuf[1];

				copybufbyte(&ffsync[other],&baksync[movefifoend[other]][other],sizeof(input));
				movefifoend[other] = ((movefifoend[other]+1)&(MOVEFIFOSIZ-1));

				while (j != packbufleng)
				{
					 othersyncval[othersyncvalhead] = packbuf[j++];
					 othersyncvalhead = ((othersyncvalhead+1)&(MOVEFIFOSIZ-1));
				}
				if ((syncvalhead != syncvaltottail) && (othersyncvalhead != syncvaltottail))
				{
					syncstat = 0;
					do
					{
						syncstat |= (syncval[syncvaltottail]^othersyncval[syncvaltottail]);
						syncvaltottail = ((syncvaltottail+1)&(MOVEFIFOSIZ-1));
					} while ((syncvalhead != syncvaltottail) && (othersyncvalhead != syncvaltottail));
				}

				break;
			case 255:  //[255] (logout)
				keystatus[1] = 1;
				break;
		}
	}
	if ((networkmode == 0) && (myconnectindex != connecthead) && ((movecnt&1) == 0))
	{
		if (rand()&1) ototalclock += (TICSPERFRAME>>1);
		else ototalclock -= (TICSPERFRAME>>1);
	}
}

void drawoverheadmap(int cposx, int cposy, int czoom, short cang)
{
	int i;
	int j;
	int k;
	int l=0;
	int x1;
	int y1;
	int x2=0;
	int y2=0;
	int x3;
	int y3;
	int x4;
	int y4;
	int ox;
	int oy;
	int xoff;
	int yoff;
	int dax;
	int day;
	int cosang;
	int sinang;
	int xspan;
	int yspan;
	int sprx;
	int spry;
	int xrepeat;
	int yrepeat;
	int z1;
	int z2;
	int startwall;
	int endwall;
	int tilenum;
	int daang;
	int xvect;
	int yvect;
	int xvect2;
	int yvect2;
	unsigned char col;
	walltype *wal;
	walltype *wal2;
	spritetype *spr;

	xvect = sintable[(-cang)&2047] * czoom;
	yvect = sintable[(1536-cang)&2047] * czoom;
	xvect2 = mulscalen<16>(xvect,yxaspect);
	yvect2 = mulscalen<16>(yvect,yxaspect);

		//Draw red lines
	for(i=0;i<numsectors;i++)
	{
		startwall = g_sector[i].wallptr;
		endwall = g_sector[i].wallptr + g_sector[i].wallnum;

		z1 = g_sector[i].ceilingz; z2 = g_sector[i].floorz;

		for(j=startwall,wal=&wall[startwall];j<endwall;j++,wal++)
		{
			k = wal->nextwall; if (k < 0) continue;

			if ((show2dwall[j>>3]&(1<<(j&7))) == 0) continue;
			if ((k > j) && ((show2dwall[k>>3]&(1<<(k&7))) > 0)) continue;

			if (g_sector[wal->nextsector].ceilingz == z1)
				if (g_sector[wal->nextsector].floorz == z2)
					if (((wal->cstat|wall[wal->nextwall].cstat)&(16+32)) == 0) continue;

			col = 152;

			if (dimensionmode[screenpeek] == 2)
			{
				if (g_sector[i].floorz != g_sector[i].ceilingz)
					if (g_sector[wal->nextsector].floorz != g_sector[wal->nextsector].ceilingz)
						if (((wal->cstat|wall[wal->nextwall].cstat)&(16+32)) == 0)
							if (g_sector[i].floorz == g_sector[wal->nextsector].floorz) continue;
				if (g_sector[i].floorpicnum != g_sector[wal->nextsector].floorpicnum) continue;
				if (g_sector[i].floorshade != g_sector[wal->nextsector].floorshade) continue;
				col = 12;
			}

			ox = wal->pt.x-cposx; oy = wal->pt.y-cposy;
			x1 = dmulscalen<16>(ox,xvect,-oy,yvect)+(xdim<<11);
			y1 = dmulscalen<16>(oy,xvect2,ox,yvect2)+(ydim<<11);

			wal2 = &wall[wal->point2];
			ox = wal2->pt.x-cposx; oy = wal2->pt.y-cposy;
			x2 = dmulscalen<16>(ox,xvect,-oy,yvect)+(xdim<<11);
			y2 = dmulscalen<16>(oy,xvect2,ox,yvect2)+(ydim<<11);

			drawline256(x1,y1,x2,y2,col);
		}
	}

		//Draw sprites
	k = playersprite[screenpeek];
	for(i=0;i<numsectors;i++)
		for(j=headspritesect[i];j>=0;j=nextspritesect[j])
			if ((show2dsprite[j>>3]&(1<<(j&7))) > 0)
			{
				spr = &sprite[j]; if (spr->cstat&0x8000) continue;
				col = 56;
				if (spr->cstat&1) col = 248;
				if (j == k) col = 31;

				k = statrate[spr->statnum];
				sprx = spr->x;
				spry = spr->y;
				if (k >= 0)
				{
					switch(k)
					{
						case 0: l = smoothratio; break;
						case 1: l = (smoothratio>>1)+(((nummoves-j)&1)<<15); break;
						case 3: l = (smoothratio>>2)+(((nummoves-j)&3)<<14); break;
						case 7: l = (smoothratio>>3)+(((nummoves-j)&7)<<13); break;
						case 15: l = (smoothratio>>4)+(((nummoves-j)&15)<<12); break;
					}
					sprx = osprite[j].x+mulscalen<16>(sprx-osprite[j].x,l);
					spry = osprite[j].y+mulscalen<16>(spry-osprite[j].y,l);
				}

				switch (spr->cstat&48)
				{
					case 0:
						ox = sprx-cposx; oy = spry-cposy;
						x1 = dmulscalen<16>(ox,xvect,-oy,yvect);
						y1 = dmulscalen<16>(oy,xvect2,ox,yvect2);

						if (dimensionmode[screenpeek] == 1)
						{
							ox = (sintable[(spr->ang+512)&2047]>>7);
							oy = (sintable[(spr->ang)&2047]>>7);
							x2 = dmulscalen<16>(ox,xvect,-oy,yvect);
							y2 = dmulscalen<16>(oy,xvect,ox,yvect);

							if (j == playersprite[screenpeek])
							{
								x2 = 0L;
								y2 = -(czoom<<5);
							}

							x3 = mulscalen<16>(x2,yxaspect);
							y3 = mulscalen<16>(y2,yxaspect);

							drawline256(x1-x2+(xdim<<11),y1-y3+(ydim<<11),
											x1+x2+(xdim<<11),y1+y3+(ydim<<11),col);
							drawline256(x1-y2+(xdim<<11),y1+x3+(ydim<<11),
											x1+x2+(xdim<<11),y1+y3+(ydim<<11),col);
							drawline256(x1+y2+(xdim<<11),y1-x3+(ydim<<11),
											x1+x2+(xdim<<11),y1+y3+(ydim<<11),col);
						}
						else
						{
							if (((gotsector[i>>3]&(1<<(i&7))) > 0) && (czoom > 96))
							{
								daang = (spr->ang-cang)&2047;
								if (j == playersprite[screenpeek]) { x1 = 0; y1 = 0; daang = 0; }
								rotatesprite((x1<<4)+(xdim<<15),(y1<<4)+(ydim<<15),mulscalen<16>(czoom*spr->yrepeat,yxaspect),daang,spr->picnum,spr->shade,spr->pal,(spr->cstat&2)>>1,windowx1,windowy1,windowx2,windowy2);
							}
						}
						break;
					case 16:
						x1 = sprx; y1 = spry;
						tilenum = spr->picnum;
						xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						if ((spr->cstat&4) > 0) xoff = -xoff;
						k = spr->ang; l = spr->xrepeat;
						dax = sintable[k&2047]*l; day = sintable[(k+1536)&2047]*l;
						l = tilesizx[tilenum]; k = (l>>1)+xoff;
						x1 -= mulscalen<16>(dax,k); x2 = x1+mulscalen<16>(dax,l);
						y1 -= mulscalen<16>(day,k); y2 = y1+mulscalen<16>(day,l);

						ox = x1-cposx; oy = y1-cposy;
						x1 = dmulscalen<16>(ox,xvect,-oy,yvect);
						y1 = dmulscalen<16>(oy,xvect2,ox,yvect2);

						ox = x2-cposx; oy = y2-cposy;
						x2 = dmulscalen<16>(ox,xvect,-oy,yvect);
						y2 = dmulscalen<16>(oy,xvect2,ox,yvect2);

						drawline256(x1+(xdim<<11),y1+(ydim<<11),
										x2+(xdim<<11),y2+(ydim<<11),col);

						break;
					case 32:
						if (dimensionmode[screenpeek] == 1)
						{
							tilenum = spr->picnum;
							xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
							yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
							if ((spr->cstat&4) > 0) xoff = -xoff;
							if ((spr->cstat&8) > 0) yoff = -yoff;

							k = spr->ang;
							cosang = sintable[(k+512)&2047]; sinang = sintable[k];
							xspan = tilesizx[tilenum]; xrepeat = spr->xrepeat;
							yspan = tilesizy[tilenum]; yrepeat = spr->yrepeat;

							dax = ((xspan>>1)+xoff)*xrepeat; day = ((yspan>>1)+yoff)*yrepeat;
							x1 = sprx + dmulscalen<16>(sinang,dax,cosang,day);
							y1 = spry + dmulscalen<16>(sinang,day,-cosang,dax);
							l = xspan*xrepeat;
							x2 = x1 - mulscalen<16>(sinang,l);
							y2 = y1 + mulscalen<16>(cosang,l);
							l = yspan*yrepeat;
							k = -mulscalen<16>(cosang,l); x3 = x2+k; x4 = x1+k;
							k = -mulscalen<16>(sinang,l); y3 = y2+k; y4 = y1+k;

							ox = x1-cposx; oy = y1-cposy;
							x1 = dmulscalen<16>(ox,xvect,-oy,yvect);
							y1 = dmulscalen<16>(oy,xvect2,ox,yvect2);

							ox = x2-cposx; oy = y2-cposy;
							x2 = dmulscalen<16>(ox,xvect,-oy,yvect);
							y2 = dmulscalen<16>(oy,xvect2,ox,yvect2);

							ox = x3-cposx; oy = y3-cposy;
							x3 = dmulscalen<16>(ox,xvect,-oy,yvect);
							y3 = dmulscalen<16>(oy,xvect2,ox,yvect2);

							ox = x4-cposx; oy = y4-cposy;
							x4 = dmulscalen<16>(ox,xvect,-oy,yvect);
							y4 = dmulscalen<16>(oy,xvect2,ox,yvect2);

							drawline256(x1+(xdim<<11),y1+(ydim<<11),
											x2+(xdim<<11),y2+(ydim<<11),col);

							drawline256(x2+(xdim<<11),y2+(ydim<<11),
											x3+(xdim<<11),y3+(ydim<<11),col);

							drawline256(x3+(xdim<<11),y3+(ydim<<11),
											x4+(xdim<<11),y4+(ydim<<11),col);

							drawline256(x4+(xdim<<11),y4+(ydim<<11),
											x1+(xdim<<11),y1+(ydim<<11),col);

						}
						break;
				}
			}

		//Draw white lines
	for(i=0;i<numsectors;i++)
	{
		startwall = g_sector[i].wallptr;
		endwall = g_sector[i].wallptr + g_sector[i].wallnum;

		k = -1;
		for(j=startwall,wal=&wall[startwall];j<endwall;j++,wal++)
		{
			if (wal->nextwall >= 0) continue;

			if ((show2dwall[j>>3]&(1<<(j&7))) == 0) continue;

			if (tilesizx[wal->picnum] == 0) continue;
			if (tilesizy[wal->picnum] == 0) continue;

			if (j == k)
				{ x1 = x2; y1 = y2; }
			else
			{
				ox = wal->pt.x-cposx; oy = wal->pt.y-cposy;
				x1 = dmulscalen<16>(ox,xvect,-oy,yvect)+(xdim<<11);
				y1 = dmulscalen<16>(oy,xvect2,ox,yvect2)+(ydim<<11);
			}

			k = wal->point2; wal2 = &wall[k];
			ox = wal2->pt.x-cposx; oy = wal2->pt.y-cposy;
			x2 = dmulscalen<16>(ox,xvect,-oy,yvect)+(xdim<<11);
			y2 = dmulscalen<16>(oy,xvect2,ox,yvect2)+(ydim<<11);

			drawline256(x1,y1,x2,y2,24);
		}
	}
}

	//New movesprite using getzrange.  Note that I made the getzrange
	//parameters global (&globhiz,&globhihit,&globloz,&globlohit) so they
	//don't need to be passed everywhere.  Also this should make this
	//movesprite function compatible with the older movesprite functions.
int movesprite(short spritenum, int dx, int dy, int dz, int ceildist, int flordist, int clipmask)
{
	int daz;
	int zoffs;
	short retval;
	short dasectnum;
	short datempshort;
	spritetype *spr;

	spr = &sprite[spritenum];

	if ((spr->cstat&128) == 0)
		zoffs = -((tilesizy[spr->picnum]*spr->yrepeat)<<1);
	else
		zoffs = 0;

	dasectnum = spr->sectnum;  //Can't modify sprite sectors directly becuase of linked lists
	daz = spr->z+zoffs;  //Must do this if not using the new centered centering (of course)
	retval = clipmove(&spr->x,&spr->y,&daz,&dasectnum,dx,dy,
				((int)spr->clipdist)<<2,ceildist,flordist,clipmask);

	if (dasectnum < 0) retval = -1;

	if ((dasectnum != spr->sectnum) && (dasectnum >= 0))
		changespritesect(spritenum,dasectnum);

		//Set the blocking bit to 0 temporarly so getzrange doesn't pick up
		//its own sprite
	datempshort = spr->cstat; spr->cstat &= ~1;
	getzrange(spr->x,spr->y,spr->z-1,spr->sectnum,
				 &globhiz,&globhihit,&globloz,&globlohit,
				 ((int)spr->clipdist)<<2,clipmask);
	spr->cstat = datempshort;

	daz = spr->z+zoffs + dz;
	if ((daz <= globhiz) || (daz > globloz))
	{
		if (retval != 0) return(retval);
		return(16384+dasectnum);
	}
	spr->z = daz-zoffs;
	return(retval);
}


void waitforeverybody ()
{
	int i;
	if (numplayers < 2) return;
	packbuf[0] = 250;
	for(i=connecthead;i>=0;i=connectpoint2[i])
	{
		if (i != myconnectindex) sendpacket(i,packbuf,1);
		if ((!networkmode) && (myconnectindex != connecthead)) break; //slaves in M/S mode only send to master
	}
	playerreadyflag[myconnectindex]++;
	while (1)
	{
		handleevents();
		refreshaudio();

		drawrooms(posx[myconnectindex],posy[myconnectindex],posz[myconnectindex],ang[myconnectindex],horiz[myconnectindex],cursectnum[myconnectindex]);
		if (!networkmode) std::sprintf(tempbuf,"Master/slave mode");
						 else std::sprintf(tempbuf,"Peer-peer mode");
		printext256((xdim>>1)-(int)(std::strlen(tempbuf)<<2),(ydim>>1)-24,31,0,tempbuf,0);
		std::sprintf(tempbuf,"Waiting for players");
		printext256((xdim>>1)-(int)(std::strlen(tempbuf)<<2),(ydim>>1)-16,31,0,tempbuf,0);
		for(i=connecthead;i>=0;i=connectpoint2[i])
		{
			if (playerreadyflag[i] < playerreadyflag[myconnectindex])
			{
					//slaves in M/S mode only wait for master
				if ((!networkmode) && (myconnectindex != connecthead) && (i != connecthead))
				{
					std::sprintf(tempbuf,"Player %d",i);
					printext256((xdim>>1)-(16<<2),(ydim>>1)+i*8,15,0,tempbuf,0);
				}
				else
				{
					std::sprintf(tempbuf,"Player %d NOT ready",i);
					printext256((xdim>>1)-(16<<2),(ydim>>1)+i*8,127,0,tempbuf,0);
				}
			}
			else
			{
				std::sprintf(tempbuf,"Player %d ready",i);
				printext256((xdim>>1)-(16<<2),(ydim>>1)+i*8,31,0,tempbuf,0);
			}
			if (i == myconnectindex)
			{
				std::sprintf(tempbuf,"You->");
				printext256((xdim>>1)-(26<<2),(ydim>>1)+i*8,95,0,tempbuf,0);
			}
		}
		nextpage();


		if (quitevent || keystatus[1]) {
			musicoff();
			uninitmultiplayers();
			uninitengine();
			uninitsb();
			uninitgroupfile();
			std::exit(0);
		}

		getpackets();

		for(i=connecthead;i>=0;i=connectpoint2[i])
		{
			if (playerreadyflag[i] < playerreadyflag[myconnectindex]) break;
			if ((!networkmode) && (myconnectindex != connecthead)) { i = -1; break; } //slaves in M/S mode only wait for master
		}
		if (i < 0) return;
	}
}


void searchmap(short startsector)
{
	int i;
	int j;
	int dasect;
	int splc;
	int send;
	int startwall;
	int endwall;
	short dapic;
	walltype *wal;

	if ((startsector < 0) || (startsector >= numsectors)) return;
	for(i=0;i<(MAXSECTORS>>3);i++) show2dsector[i] = 0;
	for(i=0;i<(MAXWALLS>>3);i++) show2dwall[i] = 0;
	for(i=0;i<(MAXSPRITES>>3);i++) show2dsprite[i] = 0;

	automapping = false;

		//Search your area recursively & set all show2dsector/show2dwalls
	tempshort[0] = startsector;
	show2dsector[startsector>>3] |= (1<<(startsector&7));
	dapic = g_sector[startsector].ceilingpicnum;
	if (waloff[dapic] == 0) loadtile(dapic);
	dapic = g_sector[startsector].floorpicnum;
	if (waloff[dapic] == 0) loadtile(dapic);
	for(splc=0,send=1;splc<send;splc++)
	{
		dasect = tempshort[splc];
		startwall = g_sector[dasect].wallptr;
		endwall = startwall + g_sector[dasect].wallnum;
		for(i=startwall,wal=&wall[startwall];i<endwall;i++,wal++)
		{
			show2dwall[i>>3] |= (1<<(i&7));
			dapic = wall[i].picnum;
			if (waloff[dapic] == 0) loadtile(dapic);
			dapic = wall[i].overpicnum;
			if (((dapic&0xfffff000) == 0) && (waloff[dapic] == 0)) loadtile(dapic);

			j = wal->nextsector;
			if ((j >= 0) && ((show2dsector[j>>3]&(1<<(j&7))) == 0))
			{
				show2dsector[j>>3] |= (1<<(j&7));

				dapic = g_sector[j].ceilingpicnum;
				if (waloff[dapic] == 0) loadtile(dapic);
				dapic = g_sector[j].floorpicnum;
				if (waloff[dapic] == 0) loadtile(dapic);

				tempshort[send++] = (short)j;
			}
		}

		for(i=headspritesect[dasect];i>=0;i=nextspritesect[i])
		{
			show2dsprite[i>>3] |= (1<<(i&7));
			dapic = sprite[i].picnum;
			if (waloff[dapic] == 0) loadtile(dapic);
		}
	}
}

void setinterpolation(int *posptr)
{
	int i;

	if (numinterpolations >= MAXINTERPOLATIONS) return;
	for(i=numinterpolations-1;i>=0;i--)
		if (curipos[i] == posptr) return;
	curipos[numinterpolations] = posptr;
	oldipos[numinterpolations] = *posptr;
	numinterpolations++;
}

void stopinterpolation(int *posptr)
{
	int i;

	for(i=numinterpolations-1;i>=startofdynamicinterpolations;i--)
		if (curipos[i] == posptr)
		{
			numinterpolations--;
			oldipos[i] = oldipos[numinterpolations];
			bakipos[i] = bakipos[numinterpolations];
			curipos[i] = curipos[numinterpolations];
		}
}

void updateinterpolations()  //Stick at beginning of domovethings
{
	int i;

	for(i=numinterpolations-1;i>=0;i--) oldipos[i] = *curipos[i];
}

void dointerpolations()       //Stick at beginning of drawscreen
{
	int i;
	int j;
	int odelta;
	int ndelta;

	ndelta = 0; j = 0;
	for(i=numinterpolations-1;i>=0;i--)
	{
		bakipos[i] = *curipos[i];
		odelta = ndelta; ndelta = (*curipos[i])-oldipos[i];
		if (odelta != ndelta) j = mulscalen<16>(ndelta,smoothratio);
		*curipos[i] = oldipos[i]+j;
	}
}

void restoreinterpolations()  //Stick at end of drawscreen
{
	int i;

	for(i=numinterpolations-1;i>=0;i--) *curipos[i] = bakipos[i];
}

void printext(int x, int y, char *buffer, short tilenum, unsigned char invisiblecol)
{
	int i;
	unsigned char ch;

	(void)invisiblecol;

	for(i=0;buffer[i]!=0;i++)
	{
		ch = (unsigned char)buffer[i];
		rotatesprite((x-((8&15)<<3))<<16,(y-((8>>4)<<3))<<16,65536L,0,tilenum,0,0,8+16+64+128,x,y,x+7,y+7);
		rotatesprite((x-((ch&15)<<3))<<16,(y-((ch>>4)<<3))<<16,65536L,0,tilenum,0,0,8+16+128,x,y,x+7,y+7);
		x += 8;
	}
}

void drawtilebackground (int thex, int they, short tilenum,
			  signed char shade, int cx1, int cy1,
			  int cx2, int cy2, unsigned char dapalnum)
{
	(void)thex; (void)they;

	const int xsiz = tilesizx[tilenum];
	const int tx1 = cx1/xsiz;
	const int tx2 = cx2/xsiz;
	const int ysiz = tilesizy[tilenum];
	const int ty1 = cy1/ysiz;
	const int ty2 = cy2/ysiz;

	for(int x{tx1}; x <= tx2; ++x)
		for(int y{ty1}; y <= ty2; ++y)
			rotatesprite(x*xsiz<<16,y*ysiz<<16,65536L,0,tilenum,shade,dapalnum,8+16+64+128,cx1,cy1,cx2,cy2);
}


/*
 * vim:ts=4:sw=4:
 */
