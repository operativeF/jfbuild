// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)


#ifndef __build_h__
#define __build_h__

#ifndef USE_POLYMOST
#  define USE_POLYMOST 0
#endif
#ifndef USE_OPENGL
#  define USE_OPENGL 0
#endif
#define USE_GL2 2
#define USE_GL3 3
#define USE_GLES2 12

#include "baselayer.hpp"
#include "compat.hpp"
#include "osd.hpp"

#include <fmt/core.h>

#include <array>
#include <span>
#include <string>
#include <vector>

inline constexpr auto MAXSECTORSV8 {4096};
inline constexpr auto MAXWALLSV8 {16384};
inline constexpr auto MAXSPRITESV8 {16384};

inline constexpr auto MAXSECTORSV7 {1024};
inline constexpr auto MAXWALLSV7 {8192};
inline constexpr auto MAXSPRITESV7 {4096};

inline constexpr auto MAXSECTORSV6 {1024};
inline constexpr auto MAXWALLSV6   {8192};
inline constexpr auto MAXSPRITESV6 {4096};
inline constexpr auto MAXTILESV6   {4096};

inline constexpr auto MAXSECTORSV5 {1024};
inline constexpr auto MAXWALLSV5   {4096};
inline constexpr auto MAXSPRITESV5 {4096};
inline constexpr auto MAXTILESV5   {4096};

inline constexpr auto MAXSECTORS{MAXSECTORSV8};
inline constexpr auto MAXWALLS{MAXWALLSV8};
inline constexpr auto MAXSPRITES{MAXSPRITESV8};

inline constexpr auto MAXTILES{9216};
inline constexpr auto MAXVOXELS{4096};
inline constexpr auto MAXSTATUS{1024};
inline constexpr auto MAXPLAYERS{16};
inline constexpr auto MAXXDIM{2880};
inline constexpr auto MAXYDIM{1800};
inline constexpr auto MAXPALOOKUPS{256};
inline constexpr auto MAXPSKYTILES{256};
inline constexpr auto MAXSPRITESONSCREEN{2048};
inline constexpr auto MAXUNIQHUDID{256}; //Extra slots so HUD models can store animation state without messing game sprites

inline constexpr auto CLIPMASK0 = (((1L)<<16)+1L);
inline constexpr auto CLIPMASK1 = (((256L)<<16)+64L);

inline constexpr int editorgridextent{131072};
inline int xdim2d{640};
inline int ydim2d{480};
inline int xdimgame{640};
inline int cachesize{0};
inline int artsize{0};
inline bool editstatus{false};
inline short searchit{0};
inline int searchx{-1};
inline int searchy{-1};
inline short searchsector{0};

inline std::FILE *logfile{nullptr};		// log filehandle


#ifdef __GNUC__
#  if __GNUC__ == 4 && __GNUC_MINOR__ >= 7
#    define BPACK __attribute__ ((packed, gcc_struct))
#  else
#    define BPACK __attribute__ ((packed))
#  endif
#else
#define BPACK
#endif

#ifdef _MSC_VER
#pragma pack(1)
#endif

#ifdef __WATCOMC__
#pragma pack(push,1);
#endif


//ceilingstat/floorstat:
//   bit 0: 1 = parallaxing, 0 = not                                 "P"
//   bit 1: 1 = groudraw, 0 = not
//   bit 2: 1 = swap x&y, 0 = not                                    "F"
//   bit 3: 1 = double smooshiness                                   "E"
//   bit 4: 1 = x-flip                                               "F"
//   bit 5: 1 = y-flip                                               "F"
//   bit 6: 1 = Align texture to first wall of sector                "R"
//   bits 7-8:                                                       "T"
//          00 = normal floors
//          01 = masked floors
//          10 = transluscent masked floors
//          11 = reverse transluscent masked floors
//   bits 9-15: reserved

	//40 bytes
struct sectortype
{
	short wallptr;
	short wallnum;
	int ceilingz;
	int floorz;
	short ceilingstat;
	short floorstat;
	short ceilingpicnum;
	short ceilingheinum;
	signed char ceilingshade;
	unsigned char ceilingpal;
	unsigned char ceilingxpanning;
	unsigned char ceilingypanning;
	short floorpicnum;
	short floorheinum;
	signed char floorshade;
	unsigned char floorpal;
	unsigned char floorxpanning;
	unsigned char floorypanning;
	unsigned char visibility;
	unsigned char filler;
	short lotag;
	short hitag;
	short extra{-1};
};

//cstat:
//   bit 0: 1 = Blocking wall (use with clipmove, getzrange)         "B"
//   bit 1: 1 = bottoms of invisible walls swapped, 0 = not          "2"
//   bit 2: 1 = align picture on bottom (for doors), 0 = top         "O"
//   bit 3: 1 = x-flipped, 0 = normal                                "F"
//   bit 4: 1 = masking wall, 0 = not                                "M"
//   bit 5: 1 = 1-way wall, 0 = not                                  "1"
//   bit 6: 1 = Blocking wall (use with hitscan / cliptype 1)        "H"
//   bit 7: 1 = Transluscence, 0 = not                               "T"
//   bit 8: 1 = y-flipped, 0 = normal                                "F"
//   bit 9: 1 = Transluscence reversing, 0 = normal                  "T"
//   bits 10-15: reserved

	//32 bytes
struct walltype
{
	int x;
	int y;
	short point2;
	short nextwall;
	short nextsector;
	short cstat;
	short picnum;
	short overpicnum;
	signed char shade;
	unsigned char pal;
	unsigned char xrepeat;
	unsigned char yrepeat;
	unsigned char xpanning;
	unsigned char ypanning;
	short lotag;
	short hitag;
	short extra;
};

//cstat:
//   bit 0: 1 = Blocking sprite (use with clipmove, getzrange)       "B"
//   bit 1: 1 = transluscence, 0 = normal                            "T"
//   bit 2: 1 = x-flipped, 0 = normal                                "F"
//   bit 3: 1 = y-flipped, 0 = normal                                "F"
//   bits 5-4: 00 = FACE sprite (default)                            "R"
//             01 = WALL sprite (like masked walls)
//             10 = FLOOR sprite (parallel to ceilings&floors)
//   bit 6: 1 = 1-sided sprite, 0 = normal                           "1"
//   bit 7: 1 = Real centered centering, 0 = foot center             "C"
//   bit 8: 1 = Blocking sprite (use with hitscan / cliptype 1)      "H"
//   bit 9: 1 = Transluscence reversing, 0 = normal                  "T"
//   bits 10-14: reserved
//   bit 15: 1 = Invisible sprite, 0 = not invisible

	//44 bytes
struct spritetype
{
	int x;
	int y;
	int z;
	short cstat;
	short picnum;
	signed char shade;
	unsigned char pal;
	unsigned char clipdist;
	unsigned char filler;
	unsigned char xrepeat;
	unsigned char yrepeat;
	signed char xoffset;
	signed char yoffset;
	short sectnum;
	short statnum;
	short ang;
	short owner;
	short xvel;
	short yvel;
	short zvel;
	short lotag;
	short hitag;
	short extra;
};

	// 12 bytes
struct spriteexttype {
	unsigned int mdanimtims;
	short mdanimcur;
	short angoff;
	unsigned char flags;
	std::array<char, 3> filler;
};
#define SPREXT_NOTMD 1
#define SPREXT_NOMDANIM 2

inline std::array<sectortype, MAXSECTORS> sector{};
inline std::array<walltype, MAXWALLS> wall{};
inline std::array<spritetype, MAXSPRITES> sprite{};
inline std::array<spriteexttype, MAXSPRITES + MAXUNIQHUDID> spriteext{};
inline constexpr auto NUMOPTIONS{9};

inline std::array<unsigned char, NUMOPTIONS> option = {1, 1, 1, 0, 0, 0, 1, (4 << 4) | 1 | 2 | 4};

inline int guniqhudid{0};

inline int spritesortcnt{};
inline std::array<spritetype, MAXSPRITESONSCREEN> tsprite{};

//numpages==127 means no persistence. Permanent rotatesprites will be retained until flushed.
//The initial frame contents will be invalid after each swap.
inline int xdim{0};
inline int ydim{0};
inline std::array<int, MAXYDIM + 1> ylookup{};
inline int numpages{0};
inline int yxaspect{0};
inline int xyaspect{-1};
inline int pixelaspect{0};
inline bool widescreen{false};
inline bool tallscreen{false};
inline int viewingrange{0};

inline constexpr auto MAXVALIDMODES{256};

struct validmode_t {
	int xdim;
	int ydim;
	unsigned char bpp;
	bool fs;	// bit 0 = fullscreen flag
	std::array<char, 2> filler;
	int extra;	// internal use
};

inline std::vector<validmode_t> validmode{};

inline short numsectors{0};
inline short numwalls{0};
inline /*volatile*/ int totalclock{0};
inline int numframes{0};
inline int randomseed{0};
inline std::array<short, 2048> sintable{};
// FIXME: Replace with constexpr after sintable is generated externally from 16384 * std::sin((double)n * pi / 1024).
//static constexpr std::array<short, 2048> comptable = []() {
// std::array<short, 2048> cc;
//     std::ranges::generate(cc, [n = 0]() mutable {
//         auto cc_val = (short)(16384*std::sin((double)(n++)*std::numbers::pi_v<double> / 1024));
//         // ++n;
//         return cc_val;
//         });
//     return cc;
// }();
inline std::array<unsigned char, 768> palette{};
inline short numpalookups{0};
inline std::array<unsigned char*, MAXPALOOKUPS> palookup{};
inline unsigned char parallaxtype{2};
inline unsigned char showinvisibility{0};
inline int parallaxyoffs{0};
inline int parallaxyscale{65536};
inline int visibility{0};
inline int parallaxvisibility{512};

inline int windowx1{0};
inline int windowy1{0};
inline int windowx2{0};
inline int windowy2{0};
inline int msens = 1 << 16;
inline short brightness{0};
inline std::array<short, MAXXDIM> startumost{};
inline std::array<short, MAXXDIM> startdmost{};

inline std::array<short, MAXPSKYTILES> pskyoff{};
inline short pskybits{0};

inline std::array<short, MAXSECTORS + 1> headspritesect{};
inline std::array<short, MAXSTATUS + 1> headspritestat{};
inline std::array<short, MAXSPRITES> prevspritesect{};
inline std::array<short, MAXSPRITES> prevspritestat{};
inline std::array<short, MAXSPRITES> nextspritesect{};
inline std::array<short, MAXSPRITES> nextspritestat{};

inline std::array<short, MAXTILES> tilesizx{};
inline std::array<short, MAXTILES> tilesizy{};
inline std::array<unsigned char, MAXTILES> walock{};
inline int numtiles{0};
inline std::array<int, MAXTILES> picanm{};
inline std::array<intptr_t, MAXTILES> waloff{};

	//These variables are for auto-mapping with the draw2dscreen function.
	//When you load a new board, these bits are all set to 0 - since
	//you haven't mapped out anything yet.  Note that these arrays are
	//bit-mapped.
	//If you want draw2dscreen() to show sprite #54 then you say:
	//   spritenum = 54;
	//   show2dsprite[spritenum>>3] |= (1<<(spritenum&7));
	//And if you want draw2dscreen() to not show sprite #54 then you say:
	//   spritenum = 54;
	//   show2dsprite[spritenum>>3] &= ~(1<<(spritenum&7));
	//Automapping defaults to false (do nothing).  If you set automapping to true,
	//   then in 3D mode, the walls and sprites that you see will show up the
	//   next time you flip to 2D mode.

inline constexpr auto SHOWN_SECTORS = (MAXSECTORS + 7) >> 3;
inline constexpr auto SHOWN_SPRITES = (MAXSPRITES + 7) >> 3;
inline constexpr auto SHOWN_WALLS   = (MAXWALLS + 7) >> 3;
inline constexpr auto SHOWN_TILES   = (MAXTILES + 7) >> 3;
inline std::array<unsigned char, SHOWN_SECTORS> show2dsector{};
inline std::array<unsigned char, SHOWN_WALLS> show2dwall{};
inline std::array<unsigned char, SHOWN_SPRITES> show2dsprite{};
inline bool automapping{false};

inline std::array<unsigned char, SHOWN_TILES> gotpic;
inline std::array<unsigned char, SHOWN_SECTORS> gotsector;

inline int captureformat{0};
inline unsigned int drawlinepat{0xFFFFFFFF};

extern void faketimerhandler();

struct palette_t {
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char f;
};

inline std::array<palette_t, 256> curpalette{}; // the current palette, unadjusted for brightness or tint
inline std::array<palette_t, 256> curpalettefaded{}; // the current palette, adjusted for brightness and tint (ie. what gets sent to the card)
inline palette_t palfadergb{0, 0, 0, 0};
inline unsigned char palfadedelta{0};

inline bool dommxoverlay{true};
inline bool novoxmips{false};

inline std::array<int, MAXTILES> tiletovox{};
inline bool usevoxels{true};
inline std::array<int, MAXVOXELS> voxscale{};

#if USE_POLYMOST && USE_OPENGL
inline bool usemodels{true};
inline bool usehightile{true};
#endif

inline std::string engineerrstr{};

inline constexpr auto MAXVOXMIPS{5};
inline intptr_t voxoff[MAXVOXELS][MAXVOXMIPS]{};

/*************************************************************************
POSITION VARIABLES:

		POSX is your x - position ranging from 0 to 65535
		POSY is your y - position ranging from 0 to 65535
			(the length of a side of the grid in EDITBORD would be 1024)
		POSZ is your z - position (height) ranging from 0 to 65535, 0 highest.
		ANG is your angle ranging from 0 to 2047.  Instead of 360 degrees, or
			 2 * PI radians, I use 2048 different angles, so 90 degrees would
			 be 512 in my system.

SPRITE VARIABLES:

	inline short headspritesect[MAXSECTORS+1], headspritestat[MAXSTATUS+1];
	inline short prevspritesect[MAXSPRITES], prevspritestat[MAXSPRITES];
	inline short nextspritesect[MAXSPRITES], nextspritestat[MAXSPRITES];

	Example: if the linked lists look like the following:
		 ⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
		 ≥      Sector lists:               Status lists:               ≥
		 √ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒ¥
		 ≥  Sector0:  4, 5, 8             Status0:  2, 0, 8             ≥
		 ≥  Sector1:  16, 2, 0, 7         Status1:  4, 5, 16, 7, 3, 9   ≥
		 ≥  Sector2:  3, 9                                              ≥
		 ¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
	Notice that each number listed above is shown exactly once on both the
		left and right side.  This is because any sprite that exists must
		be in some sector, and must have some kind of status that you define.


Coding example #1:
	To go through all the sprites in sector 1, the code can look like this:

		sectnum = 1;
		i = headspritesect[sectnum];
		while (i != -1)
		{
			nexti = nextspritesect[i];

			//your code goes here
			//ex: printf("Sprite %d is in sector %d\n",i,sectnum);

			i = nexti;
		}

Coding example #2:
	To go through all sprites with status = 1, the code can look like this:

		statnum = 1;        //status 1
		i = headspritestat[statnum];
		while (i != -1)
		{
			nexti = nextspritestat[i];

			//your code goes here
			//ex: printf("Sprite %d has a status of 1 (active)\n",i,statnum);

			i = nexti;
		}

			 insertsprite(short sectnum, short statnum);
			 deletesprite(short spritenum);
			 changespritesect(short spritenum, short newsectnum);
			 changespritestat(short spritenum, short newstatnum);

TILE VARIABLES:
		NUMTILES - the number of tiles found TILES.DAT.
		TILESIZX[MAXTILES] - simply the x-dimension of the tile number.
		TILESIZY[MAXTILES] - simply the y-dimension of the tile number.
		WALOFF[MAXTILES] - the actual 32-bit offset pointing to the top-left
								 corner of the tile.
		PICANM[MAXTILES] - flags for animating the tile.

TIMING VARIABLES:
		TOTALCLOCK - When the engine is initialized, TOTALCLOCK is set to zero.
			From then on, it is incremented 120 times a second by 1.  That
			means that the number of seconds elapsed is totalclock / 120.
		NUMFRAMES - The number of times the draw3dscreen function was called
			since the engine was initialized.  This helps to determine frame
			rate.  (Frame rate = numframes * 120 / totalclock.)

OTHER VARIABLES:

		STARTUMOST[320] is an array of the highest y-coordinates on each column
				that my engine is allowed to write to.  You need to set it only
				once.
		STARTDMOST[320] is an array of the lowest y-coordinates on each column
				that my engine is allowed to write to.  You need to set it only
				once.
		SINTABLE[2048] is a sin table with 2048 angles rather than the
			normal 360 angles for higher precision.  Also since SINTABLE is in
			all integers, the range is multiplied by 16383, so instead of the
			normal -1<sin(x)<1, the range of sintable is -16383<sintable[]<16383
			If you use this sintable, you can possibly speed up your code as
			well as save space in memory.  If you plan to use sintable, 2
			identities you may want to keep in mind are:
				sintable[ang&2047]       = sin(ang * (3.141592/1024)) * 16383
				sintable[(ang+512)&2047] = cos(ang * (3.141592/1024)) * 16383
		NUMSECTORS - the total number of existing sectors.  Modified every time
			you call the loadboard function.
***************************************************************************/

int    preinitengine();	// a partial setup of the engine used for launch windows
bool   initengine();
void   uninitengine();
void   initspritelists();
int   loadboard(const std::string& filename, char fromwhere, int *daposx, int *daposy, int *daposz, short *daang, short *dacursectnum);
int   loadmaphack(const std::string& filename);
int   saveboard(const std::string& filename, const int *daposx, const int *daposy, const int *daposz, const short *daang, const short *dacursectnum);
int   saveoldboard(const char *filename, const int *daposx, const int *daposy, const int *daposz, const short *daang, const short *dacursectnum);
int   loadpics(const std::string& filename, int askedsize);
void   loadtile(short tilenume);
int   qloadkvx(int voxindex, const std::string& filename);
intptr_t allocatepermanenttile(short tilenume, int xsiz, int ysiz);
void   copytilepiece(int tilenume1, int sx1, int sy1, int xsiz, int ysiz, int tilenume2, int sx2, int sy2);
int    makepalookup(int palnum, unsigned char *remapbuf, signed char r, signed char g, signed char b, unsigned char dastat);
void   setvgapalette();
void   setbrightness(int dabrightness, std::span<const unsigned char> dapal, char noapply);
void   setpalettefade(unsigned char r, unsigned char g, unsigned char b, unsigned char offset);
void   squarerotatetile(short tilenume);

int   setgamemode(bool davidoption, int daxdim, int daydim, int dabpp);
void   nextpage();
void   setview(int x1, int y1, int x2, int y2);
void   setaspect(int daxrange, int daaspect);
void   flushperms();

void   plotpixel(int x, int y, unsigned char col);
unsigned char   getpixel(int x, int y);
void   setviewtotile(short tilenume, int xsiz, int ysiz);
void   setviewback();
void   preparemirror(int dax, int day, int daz, short daang, int dahoriz, short dawall, short dasector, int *tposx, int *tposy, short *tang);
void   completemirror();

void   drawrooms(int daposx, int daposy, int daposz, short daang, int dahoriz, short dacursectnum);
void   drawmasks();
void   clearview(int dacol);
void   clearallviews(int dacol);
void   drawmapview(int dax, int day, int zoome, short ang);
void   rotatesprite(int sx, int sy, int z, short a, short picnum, signed char dashade, unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2);
void   drawline256(int x1, int y1, int x2, int y2, unsigned char col);
void   printext256(int xpos, int ypos, short col, short backcol, std::string_view name, char fontsize);

int   clipmove(int *x, int *y, const int *z, short *sectnum, int xvect, int yvect, int walldist, int ceildist, int flordist, unsigned int cliptype);
int   clipinsidebox(int x, int y, short wallnum, int walldist);
int   clipinsideboxline(int x, int y, int x1, int y1, int x2, int y2, int walldist);
int   pushmove(int *x, int *y, const int *z, short *sectnum, int walldist, int ceildist, int flordist, unsigned int cliptype);
void   getzrange(int x, int y, int z, short sectnum, int *ceilz, int *ceilhit, int *florz, int *florhit, int walldist, unsigned int cliptype);
int    hitscan(int xs, int ys, int zs, short sectnum, int vx, int vy, int vz, short *hitsect, short *hitwall, short *hitsprite, int *hitx, int *hity, int *hitz, unsigned int cliptype);
int   neartag(int xs, int ys, int zs, short sectnum, short ange, short *neartagsector, short *neartagwall, short *neartagsprite, int *neartaghitdist, int neartagrange, unsigned char tagsearch);
bool  cansee(int x1, int y1, int z1, short sect1, int x2, int y2, int z2, short sect2);
void   updatesector(int x, int y, short *sectnum);
void   updatesectorz(int x, int y, int z, short *sectnum);
int   inside(int x, int y, short sectnum);
void   dragpoint(short pt_highlight, int dax, int day);
void   setfirstwall(short sectnum, short newfirstwall);

void   getmousevalues(int *mousx, int *mousy, int *bstatus);
int    krand();
int   ksqrt(int num);
int   getangle(int xvect, int yvect);
void   rotatepoint(int xpivot, int ypivot, int x, int y, short daang, int *x2, int *y2);
int   lastwall(short point);
int   nextsectorneighborz(short sectnum, int thez, short topbottom, short direction);
int   getceilzofslope(short sectnum, int dax, int day);
int   getflorzofslope(short sectnum, int dax, int day);
void   getzsofslope(short sectnum, int dax, int day, int *ceilz, int *florz);
void   alignceilslope(short dasect, int x, int y, int z);
void   alignflorslope(short dasect, int x, int y, int z);
int   sectorofwall(short theline);
int   loopnumofsector(short sectnum, short wallnum);

int   insertsprite(short sectnum, short statnum);
int   deletesprite(short spritenum);
int   changespritesect(short spritenum, short newsectnum);
int   changespritestat(short spritenum, short newstatnum);
int   setsprite(short spritenum, int newx, int newy, int newz);
int   setspritez(short spritenum, int newx, int newy, int newz);

int   screencapture(const char* filename, char mode);	// mode&1 == invert, mode&2 == wait for nextpage

#if USE_POLYMOST
int   setrendermode(int renderer);
int   getrendermode();
# define POLYMOST_RENDERMODE_CLASSIC() (getrendermode() == 0)
# define POLYMOST_RENDERMODE_POLYMOST() (getrendermode() > 0)
# define POLYMOST_RENDERMODE_POLYGL() (getrendermode() == 3)

void    setrollangle(int rolla);
#else
# define POLYMOST_RENDERMODE_CLASSIC() (1)
# define POLYMOST_RENDERMODE_POLYMOST() (0)
# define POLYMOST_RENDERMODE_POLYGL() (0)
#endif

#if USE_OPENGL
inline int glswapinterval{1};
#endif

#if USE_POLYMOST && USE_OPENGL
//  pal: pass -1 to invalidate all palettes for the tile, or >=0 for a particular palette
//  how: pass -1 to invalidate all instances of the tile in texture memory, or a bitfield
//         bit 0: opaque or masked (non-translucent) texture, using repeating
//         bit 1: ignored
//         bit 2: 33% translucence, using repeating
//         bit 3: 67% translucence, using repeating
//         bit 4: opaque or masked (non-translucent) texture, using clamping
//         bit 5: ignored
//         bit 6: 33% translucence, using clamping
//         bit 7: 67% translucence, using clamping
//       clamping is for sprites, repeating is for walls
void invalidatetile(short tilenume, int pal, int how);

void setpolymost2dview();   // sets up GL for 2D drawing

int polymost_drawtilescreen(int tilex, int tiley, int wallnum, int dimen);
void polymost_glreset();
void polymost_precache_begin();
void polymost_precache(int dapicnum, int dapalnum, int datype);
int  polymost_precache_run(int* done, int* total);

inline int glanisotropy{0}; // 0 = maximum supported by card
inline int glusetexcompr{1};
inline int gltexfiltermode{5}; // GL_LINEAR_MIPMAP_LINEAR
inline int glredbluemode{0};
inline int glusetexcache{1};
inline int glmultisample{0};
inline int glnvmultisamplehint{0};
inline int glsampleshading{0};

void gltexapplyprops ();

inline int polymosttexfullbright{256};	// set to the first index of the fullbright palette

// effect bitset: 1 = greyscale, 2 = invert
void hicsetpalettetint(int palnum, unsigned char r, unsigned char g, unsigned char b, unsigned char effect);
// flags bitset: 1 = don't compress
int hicsetsubsttex(int picnum, int palnum, const std::string& filen, float alphacut, unsigned char flags);
int hicsetskybox(int picnum, int palnum, std::span<const std::string> faces);
int hicclearsubst(int picnum, int palnum);

int md_loadmodel(const char *fn);
int md_setmisc(int modelid, float scale, int shadeoff, float zadd);
int md_tilehasmodel(int tilenume);
int md_defineframe(int modelid, std::string_view framename, int tilenume, int skinnum);
int md_defineanimation(int modelid, std::string_view framestart, std::string_view frameend, int fps, int flags);
int md_defineskin(int modelid, const char *skinfn, int palnum, int skinnum, int surfnum);
int md_definehud (int modelid, int tilex, double xadd, double yadd, double zadd, double angadd, int flags);
int md_undefinetile(int tile);
int md_undefinemodel(int modelid);

#endif //USE_POLYMOST && USE_OPENGL

int loaddefinitionsfile(const char *fn);

inline int mapversion{7L};	// JBF 20040211: default mapversion to 7;
                            // if loadboard() fails with -2 return, try loadoldboard(). if it fails with -2, board is dodgy
int loadoldboard(const std::string& filename, char fromwhere, int *daposx, int *daposy, int *daposz, short *daang, short *dacursectnum);

template<typename... Args>
void buildprintf(std::string_view form, Args&&... args)
{
	fmt::vprint(stdout, form, fmt::make_format_args(args...));

	if (logfile) {
		fmt::vprint(logfile, form, fmt::make_format_args(args...));
	}

	const auto tmpstr = fmt::vformat(form, fmt::make_format_args(args...));

	initputs(tmpstr.c_str());
	OSD_Puts(tmpstr);
}

void buildputs(std::string_view str);
void buildsetlogfile(const char *fn);

#ifdef _MSC_VER
#pragma pack()
#endif

#ifdef __WATCOMC__
#pragma pack(pop)
#endif

#undef BPACK

#endif // __build_h__
