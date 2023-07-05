// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)


#define ENGINE

#include "build.hpp"
#include "pragmas.hpp"
#include "cache1d.hpp"
#include "a.hpp"
#include "osd.hpp"
#include "crc32.hpp"
#include "screencapture.hpp"
#include "string_utils.hpp"
#include "textfonts.hpp"
#include "version.hpp"

#include "baselayer.hpp"
#include "baselayer_priv.hpp"

#include "algo_utils.hpp"

#include "engine_priv.hpp"
#if USE_POLYMOST
# include "polymost_priv.hpp"
# if USE_OPENGL
#  include "hightile_priv.hpp"
#  include "polymosttex_priv.hpp"
#  include "polymosttexcache.hpp"
#  include "mdsprite_priv.hpp"
# endif
# ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
# endif
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <csignal>
#include <numbers>
#include <numeric>
#include <span>
#include <utility>

void loadvoxel(int voxindex) { std::ignore = voxindex; }

#define kloadvoxel loadvoxel

	//These variables need to be copied into BUILD
constexpr auto MAXXSIZ{256};
constexpr auto MAXYSIZ{256};
constexpr auto MAXZSIZ{255};

unsigned char voxlock[MAXVOXELS][MAXVOXMIPS];
bool beforedrawrooms{true};

constexpr auto SLOPALOOKUPSIZ = MAXXDIM << 1;

int artversion;
void *pic{nullptr};

int lastageclock;
std::array<int, MAXTILES> tilefileoffs;

static constexpr std::array<int, 2048> reciptable = []() {
	std::array<int, 2048> rectable;
	std::ranges::generate(rectable, [n = 0]() mutable {
				return divscalen<30>(2048L, (n++) + 2048);
			});
	return rectable;
}();

int fpuasm;

std::array<short, MAXXDIM> umost;
std::array<short, MAXXDIM> dmost;

int wx1;
int wy1;
int wx2;
int wy2;

std::array<short, MAXXDIM> uplc;
std::array<short, MAXXDIM> dplc;

unsigned char *globalpalwritten;
int globaluclip;
int globaldclip;
int globvis;
int globalvisibility;
int globalhisibility;
int globalpisibility;
int globalcisibility;
bool globparaceilclip{false};
bool globparaflorclip{false};

int viewingrangerecip;

int asm1;
int asm2;
intptr_t asm3;
std::array<int, 4> vplce;
std::array<int, 4> vince;
std::array<intptr_t, 4> palookupoffse;
std::array<intptr_t, 4> bufplce;
unsigned char globalxshift;
unsigned char globalyshift;
int globalxpanning;
int globalypanning;
short globalshiftval;
int globalzd;
int globalyscale;
intptr_t globalbufplc;
int globalx3;
int globaly3;

short linehighlight{-1};
short highlightcnt{0};

constexpr auto FASTPALGRIDSIZ{8};

const struct textfontspec textfonts[3] = {
	{	//8x8
		textfont,
		8, 8,
		8, 0, 0
	},
	{	// 4x6, centred on 8x8 cells
		smalltextfont,
		4, 6,
		8, 2, 1
	},
	{	// 8x14
		talltextfont,
		8, 14,
		14, 0, 0
	}
};

short searchwall;

int totalclocklock;

namespace {

constexpr std::array<int, 1024> lowrecip = [](){
	std::array<int, 1024> lowreciparr{};
	// Skip over first term, a zero.
	std::ranges::subrange afterfirst = {std::ranges::next(lowreciparr.begin()), lowreciparr.end()};
	std::ranges::generate(afterfirst, [n = 1] () mutable {
		return ((1 << 24) - 1) / n++;
	});

	return lowreciparr;
}();

int nytooclose;
constexpr int nytoofar = 65536 * 16384 - 1048576; // NOTE: Previously hardcoded in dosetaspect
std::array<unsigned int, 65536> distrecip;

std::vector<int> lookups;

int oxdimen{-1};
int oviewingrange{-1};
int oxyaspect{-1};

	//Textured Map variables
unsigned char globalpolytype;

std::array<unsigned char, MAXWALLS> tempbuf;
std::array<unsigned char, MAXTILES> tilefilenum;
std::array<short, 1280> radarang;
std::array<short, MAXXDIM> radarang2;
std::array<char, 128> kensmessage;

//unsigned int ratelimitlast[32], ratelimitn = 0, ratelimit = 60;

#if defined(__WATCOMC__) && USE_ASM

//
// Watcom Inline Assembly Routines
//

	//0x007ff000 is (11<<13), 0x3f800000 is (127<<23)
#pragma aux krecipasm =\
	"mov fpuasm, eax",\
	"fild dword ptr fpuasm",\
	"add eax, eax",\
	"fstp dword ptr fpuasm",\
	"sbb ebx, ebx",\
	"mov eax, fpuasm",\
	"mov ecx, eax",\
	"and eax, 0x007ff000",\
	"shr eax, 10",\
	"sub ecx, 0x3f800000",\
	"shr ecx, 23",\
	"mov eax, dword ptr reciptable[eax]",\
	"sar eax, cl",\
	"xor eax, ebx",\
	parm [eax]\
	modify exact [eax ebx ecx]
int krecipasm(int);

#pragma aux getclipmask =\
	"sar eax, 31",\
	"add ebx, ebx",\
	"adc eax, eax",\
	"add ecx, ecx",\
	"adc eax, eax",\
	"add edx, edx",\
	"adc eax, eax",\
	"mov ebx, eax",\
	"shl ebx, 4",\
	"or al, 0xf0",\
	"xor eax, ebx",\
	parm [eax][ebx][ecx][edx]\
	modify exact [eax ebx ecx edx]
int getclipmask(int,int,int,int);

#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM	// __WATCOMC__

	//0x007ff000 is (11<<13), 0x3f800000 is (127<<23)
inline int krecipasm(int a)
{
	_asm {
		push ebx
		mov eax, a
		mov fpuasm, eax
		fild dword ptr fpuasm
		add eax, eax
		fstp dword ptr fpuasm
		sbb ebx, ebx
		mov eax, fpuasm
		mov ecx, eax
		and eax, 0x007ff000
		shr eax, 10
		sub ecx, 0x3f800000
		shr ecx, 23
		mov eax, dword ptr reciptable[eax]
		sar eax, cl
		xor eax, ebx
		pop ebx
	}
}

inline int getclipmask(int a, int b, int c, int d)
{
	_asm {
		push ebx
		mov eax, a
		mov ebx, b
		mov ecx, c
		mov edx, d
		sar eax, 31
		add ebx, ebx
		adc eax, eax
		add ecx, ecx
		adc eax, eax
		add edx, edx
		adc eax, eax
		mov ebx, eax
		shl ebx, 4
		or al, 0xf0
		xor eax, ebx
		pop ebx
	}
}

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM	// _MSC_VER

//
// GCC "Inline" Assembly Routines
//

#define krecipasm(a) \
	({ int __a=(a); \
	   __asm__ __volatile__ ( \
			"movl %%eax, (%[fpuasm]); fildl (%[fpuasm]); " \
			"addl %%eax, %%eax; fstps (%[fpuasm]); sbbl %%ebx, %%ebx; " \
			"movl (%[fpuasm]), %%eax; movl %%eax, %%ecx; " \
			"andl $0x007ff000, %%eax; shrl $10, %%eax; subl $0x3f800000, %%ecx; " \
			"shrl $23, %%ecx; movl %[reciptable](%%eax), %%eax; " \
			"sarl %%cl, %%eax; xorl %%ebx, %%eax" \
		: "=a" (__a) \
		: "a" (__a), [fpuasm] "m" (fpuasm), [reciptable] "m" (reciptable[0]) \
		: "ebx", "ecx", "memory", "cc"); \
	 __a; })

#define getclipmask(a,b,c,d) \
	({ int __a=(a), __b=(b), __c=(c), __d=(d); \
	   __asm__ __volatile__ ("sarl $31, %%eax; addl %%ebx, %%ebx; adcl %%eax, %%eax; " \
				"addl %%ecx, %%ecx; adcl %%eax, %%eax; addl %%edx, %%edx; " \
				"adcl %%eax, %%eax; movl %%eax, %%ebx; shl $4, %%ebx; " \
				"orb $0xf0, %%al; xorl %%ebx, %%eax" \
		: "=a" (__a), "=b" (__b), "=c" (__c), "=d" (__d) \
		: "a" (__a), "b" (__b), "c" (__c), "d" (__d) : "cc"); \
	 __a; })

#else	// __GNUC__ && __i386__

inline int krecipasm(int i)
{ // Ken did this
	const float f = (float)i;
	i = *(int *)&f;
	return((reciptable[(i>>12)&2047]>>(((i-0x3f800000)>>23)&31))^(i>>31));
}


inline int getclipmask(int a, int b, int c, int d)
{ // Ken did this
	d = ((a<0)*8) + ((b<0)*4) + ((c<0)*2) + (d<0);
	return(((d<<4)^0xf0)|d);
}

#endif

std::array<int, MAXWALLSB> yb1;
std::array<int, MAXWALLSB> xb2;
std::array<int, MAXWALLSB> yb2;

std::array<int, MAXWALLSB> rx2;
std::array<int, MAXWALLSB> ry2;

std::array<short, MAXYSAVES> smost;
short smostcnt;
std::array<short, MAXWALLSB> smoststart;
std::array<unsigned char, MAXWALLSB> smostwalltype;
std::array<int, MAXWALLSB> smostwall;
int smostwallcnt{-1L};

std::array<int, MAXSPRITESONSCREEN> spritesx;
std::array<int, MAXSPRITESONSCREEN + 1> spritesy;
std::array<int, MAXSPRITESONSCREEN> spritesz;
std::array<short, MAXXDIM> bakumost;
std::array<short, MAXXDIM> bakdmost;
std::array<short, MAXXDIM> uwall;
std::array<short, MAXXDIM> dwall;
std::array<int, MAXXDIM> swplc;
std::array<int, MAXXDIM> lplc;
std::array<int, MAXXDIM> swall;
std::array<int, MAXXDIM + 4> lwall;
std::array<int, 8> nrx1;
std::array<int, 8> nry1;
std::array<int, 8> nrx2;
std::array<int, 8> nry2;	// JBF 20031206: Thanks Ken

std::array<int, 8> rxi;
std::array<int, 8> ryi;
std::array<int, 8> rzi;
std::array<int, 8> rxi2;
std::array<int, 8> ryi2;
std::array<int, 8> rzi2;

std::array<int, 8> xsi;
std::array<int, 8> ysi;
int* horizlookup{nullptr};
int* horizlookup2{nullptr};
int horizycent;

std::array<int, 129> rdist;
std::array<int, 129> gdist;
std::array<int, 129> bdist;
constexpr auto TOTALPALGRIDSIZING = (FASTPALGRIDSIZ + 2) * (FASTPALGRIDSIZ + 2) * (FASTPALGRIDSIZ + 2);
std::array<unsigned char, (TOTALPALGRIDSIZING >> 3)> colhere;
std::array<unsigned char, TOTALPALGRIDSIZING> colhead;
std::array<int, 256> colnext;

constexpr std::array<unsigned char, 8> coldist = {
	0, 1, 2, 3, 4, 3, 2, 1
};

std::array<int, 27> colscan;

short clipnum;
std::array<short, 4> hitwalls;

struct linetype {
	int x1;
	int y1;
	int x2;
	int y2;
};

std::array<linetype, MAXCLIPNUM> clipit;
std::array<short, MAXCLIPNUM> clipsectorlist;
short clipsectnum;
std::array<short, MAXCLIPNUM> clipobjectval;

struct permfifotype
{
	int sx;
	int sy;
	int z;
	short a;
	short picnum;
	signed char dashade;
	unsigned char dapalnum;
	unsigned char dastat;
	unsigned char pagesleft;
	int cx1;
	int cy1;
	int cx2;
	int cy2;
	int uniqid;	//JF extension
};

std::array<permfifotype, MAXPERMS> permfifo;
int permhead{0};
int permtail{0};

// FIXME: Consider grouping the 4 numbers together in a struct
constexpr std::array<unsigned char, 4 * 256> vgapal16 = {
	00,00,00,00, 42,00,00,00, 00,42,00,00, 42,42,00,00, 00,00,42,00,
	42,00,42,00, 00,21,42,00, 42,42,42,00, 21,21,21,00, 63,21,21,00,
	21,63,21,00, 63,63,21,00, 21,21,63,00, 63,21,63,00, 21,63,63,00,
	63,63,63,00
};

std::array<char, 20> artfilename;
int numtilefiles;
int artfil{-1};
int artfilnum;
int artfilplc;

int mirrorsx1;
int mirrorsy1;
int mirrorsx2;
int mirrorsy2;

int setviewcnt{0};	// interface layers use this now
std::array<intptr_t, 4> bakframeplace;
std::array<int, 4> bakxsiz;
std::array<int, 4> bakysiz;
std::array<int, 4> bakwindowx1;
std::array<int, 4> bakwindowy1;
std::array<int, 4> bakwindowx2;
std::array<int, 4> bakwindowy2;

#if USE_POLYMOST
rendmode_t bakrendmode;
int baktile;
#endif

//
// Internal Engine Functions
//
//int cacheresets = 0,cacheinvalidates = 0;

//
// getpalookup (internal)
//
inline int getpalookup(int davis, int dashade)
{
	return std::min(std::max(dashade + (davis >> 8), 0), static_cast<int>(numpalookups) - 1);
}


//
// scansector (internal)
//
void scansector(short sectnum)
{
	walltype* wal;
	walltype* wal2;
	spritetype *spr;
	int xs;
	int ys;
	int x1;
	int y1;
	int x2;
	int y2;
	int xp1;
	int yp1;
	int xp2{0};
	int yp2{0};
	int templong;
	short z;
	short zz;
	short startwall;
	short endwall;
	short numscansbefore;
	short scanfirst;
	short bunchfrst;

	if (sectnum < 0) {
		return;
	}

	if (automapping) {
		show2dsector[sectnum>>3] |= pow2char[sectnum & 7];
	}

	sectorborder[0] = sectnum, sectorbordercnt = 1;

	do
	{
		sectnum = sectorborder[--sectorbordercnt];

		for(z=headspritesect[sectnum];z>=0;z=nextspritesect[z])
		{
			spr = &sprite[z];
			if ((((spr->cstat&0x8000) == 0) || (showinvisibility)) &&
				  (spr->xrepeat > 0) && (spr->yrepeat > 0) &&
				  (spritesortcnt < MAXSPRITESONSCREEN))
			{
				xs = spr->x-globalpos.x; ys = spr->y-globalpos.y;
				if ((spr->cstat&48) || (xs*cosglobalang+ys*singlobalang > 0))
				{
					copybufbyte(spr,&tsprite[spritesortcnt],sizeof(spritetype));
					tsprite[spritesortcnt++].owner = z;
				}
			}
		}

		gotsector[sectnum>>3] |= pow2char[sectnum & 7];

		bunchfrst = numbunches;
		numscansbefore = numscans;

		startwall = g_sector[sectnum].wallptr;
		endwall = startwall + g_sector[sectnum].wallnum;
		scanfirst = numscans;
		for(z=startwall,wal=&wall[z];z<endwall;z++,wal++)
		{
			const short nextsectnum = wal->nextsector;

			wal2 = &wall[wal->point2];
			x1 = wal->pt.x - globalpos.x;
			y1 = wal->pt.y - globalpos.y;
			x2 = wal2->pt.x - globalpos.x;
			y2 = wal2->pt.y - globalpos.y;

			if ((nextsectnum >= 0) && ((wal->cstat&32) == 0))
				if ((gotsector[nextsectnum>>3] & pow2char[nextsectnum & 7]) == 0)
				{
					templong = x1*y2-x2*y1;
					if (((unsigned)templong+262144) < 524288)
						if (mulscalen<5>(templong,templong) <= (x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))
							sectorborder[sectorbordercnt++] = nextsectnum;
				}

			if ((z == startwall) || (wall[z-1].point2 != z))
			{
				xp1 = dmulscalen<6>(y1,cosglobalang,-x1,singlobalang);
				yp1 = dmulscalen<6>(x1,cosviewingrangeglobalang,y1,sinviewingrangeglobalang);
			}
			else
			{
				xp1 = xp2;
				yp1 = yp2;
			}
			xp2 = dmulscalen<6>(y2,cosglobalang,-x2,singlobalang);
			yp2 = dmulscalen<6>(x2,cosviewingrangeglobalang,y2,sinviewingrangeglobalang);
			if ((yp1 < 256) && (yp2 < 256)) goto skipitaddwall;

				//If wall's NOT facing you
			if (dmulscalen<32>(xp1,yp2,-xp2,yp1) >= 0) goto skipitaddwall;

			if (xp1 >= -yp1)
			{
				if ((xp1 > yp1) || (yp1 == 0)) goto skipitaddwall;
				xb1[numscans] = halfxdimen + scale(xp1,halfxdimen,yp1);
				if (xp1 >= 0) xb1[numscans]++;   //Fix for SIGNED divide
				if (xb1[numscans] >= xdimen) xb1[numscans] = xdimen-1;
				yb1[numscans] = yp1;
			}
			else
			{
				if (xp2 < -yp2) goto skipitaddwall;
				xb1[numscans] = 0;
				templong = yp1-yp2+xp1-xp2;
				if (templong == 0) goto skipitaddwall;
				yb1[numscans] = yp1 + scale(yp2-yp1,xp1+yp1,templong);
			}
			if (yb1[numscans] < 256) goto skipitaddwall;

			if (xp2 <= yp2)
			{
				if ((xp2 < -yp2) || (yp2 == 0)) goto skipitaddwall;
				xb2[numscans] = halfxdimen + scale(xp2,halfxdimen,yp2) - 1;
				if (xp2 >= 0) xb2[numscans]++;   //Fix for SIGNED divide
				if (xb2[numscans] >= xdimen) xb2[numscans] = xdimen-1;
				yb2[numscans] = yp2;
			}
			else
			{
				if (xp1 > yp1) goto skipitaddwall;
				xb2[numscans] = xdimen-1;
				templong = xp2-xp1+yp1-yp2;
				if (templong == 0) goto skipitaddwall;
				yb2[numscans] = yp1 + scale(yp2-yp1,yp1-xp1,templong);
			}
			if ((yb2[numscans] < 256) || (xb1[numscans] > xb2[numscans])) goto skipitaddwall;

				//Made it all the way!
			thesector[numscans] = sectnum; thewall[numscans] = z;
			rx1[numscans] = xp1; ry1[numscans] = yp1;
			rx2[numscans] = xp2; ry2[numscans] = yp2;
			p2[numscans] = numscans+1;
			numscans++;
skipitaddwall:

			if ((wall[z].point2 < z) && (scanfirst < numscans))
				p2[numscans-1] = scanfirst, scanfirst = numscans;
		}

		for(z=numscansbefore;z<numscans;z++)
			if ((wall[thewall[z]].point2 != thewall[p2[z]]) || (xb2[z] >= xb1[p2[z]]))
				bunchfirst[numbunches++] = p2[z], p2[z] = -1;

		for(z=bunchfrst;z<numbunches;z++)
		{
			for(zz=bunchfirst[z];p2[zz]>=0;zz=p2[zz]);
			bunchlast[z] = zz;
		}
	} while (sectorbordercnt > 0);
}


//
// maskwallscan (internal)
//
void maskwallscan(int x1, int x2, std::span<const short> uwal, std::span<const short> dwal, std::span<const int> swal, std::span<const int> lwal)
{
	std::array<int, 4> y1ve;
	std::array<int, 4> y2ve;

	int tsizx = tilesizx[globalpicnum];
	int tsizy = tilesizy[globalpicnum];

	setgotpic(globalpicnum);

	if ((tsizx <= 0) || (tsizy <= 0)) {
		return;
	}

	if ((uwal[x1] > ydimen) && (uwal[x2] > ydimen)) {
		return;
	}

	if ((dwal[x1] < 0) && (dwal[x2] < 0)) {
		return;
	}

	if (waloff[globalpicnum] == 0) {
		loadtile(globalpicnum);
	}

	const int startx{x1};

	const bool xnice = pow2long[picsiz[globalpicnum] & 15] == tsizx;
	
	if(xnice) {
		tsizx = tsizx - 1;
	}
	
	const bool ynice = pow2long[picsiz[globalpicnum] >> 4] == tsizy;
	
	if(ynice) {
		tsizy = picsiz[globalpicnum] >> 4;
	}

	const auto fpalookup = (intptr_t)palookup[globalpal].data();

	setupmvlineasm(globalshiftval);

#ifndef USING_A_C
    intptr_t i;
	int u4;
	int d4;
	int dax;
	int z;
	char bad;
	
	x = startx;
	while ((startumost[x+windowx1] > startdmost[x+windowx1]) && (x <= x2)) x++;

	p = x+frameoffset;

	for(;(x<=x2)&&(p&3);x++,p++)
	{
		y1ve[0] = std::max(static_cast<int>(uwal[x]), static_cast<int>(startumost[x + windowx1]) - windowy1);
		y2ve[0] = std::min(static_cast<int>(dwal[x]), static_cast<int>(startdmost[x + windowx1]) - windowy1);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(p+ylookup[y1ve[0]]));
	}
	for(;x<=x2-3;x+=4,p+=4)
	{
		bad = 0;
		for(z=3,dax=x+3;z>=0;z--,dax--)
		{
			y1ve[z] = std::max(static_cast<int>(uwal[dax]), static_cast<int>(startumost[dax + windowx1]) - windowy1);
			y2ve[z] = std::min(static_cast<int>(dwal[dax]), static_cast<int>(startdmost[dax + windowx1]) - windowy1) - 1;
			if (y2ve[z] < y1ve[z]) { bad += pow2char[z]; continue; }

			i = lwal[dax] + globalxpanning;
			if (i >= tsizx) { if (xnice == 0) i %= tsizx; else i &= tsizx; }
			if (ynice == 0) i *= tsizy; else i <<= tsizy;
			bufplce[z] = waloff[globalpicnum]+i;

			vince[z] = swal[dax]*globalyscale;
			vplce[z] = globalzd + vince[z]*(y1ve[z]-globalhoriz+1);
		}
		if (bad == 15) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x],globvis),globalshade)<<8);
		palookupoffse[3] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x+3],globvis),globalshade)<<8);

		if ((palookupoffse[0] == palookupoffse[3]) && ((bad&0x9) == 0))
		{
			palookupoffse[1] = palookupoffse[0];
			palookupoffse[2] = palookupoffse[0];
		}
		else
		{
			palookupoffse[1] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x+1],globvis),globalshade)<<8);
			palookupoffse[2] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x+2],globvis),globalshade)<<8);
		}

		u4 = std::max(std::max(y1ve[0], y1ve[1]), std::max(y1ve[2], y1ve[3]));
		d4 = std::min(std::min(y2ve[0], y2ve[1]), std::min(y2ve[2], y2ve[3]));

		if ((bad > 0) || (u4 >= d4))
		{
			if (!(bad&1)) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
			if (!(bad&2)) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
			if (!(bad&4)) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
			if (!(bad&8)) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));
			continue;
		}

		if (u4 > y1ve[0]) vplce[0] = mvlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
		if (u4 > y1ve[1]) vplce[1] = mvlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
		if (u4 > y1ve[2]) vplce[2] = mvlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
		if (u4 > y1ve[3]) vplce[3] = mvlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));

		if (d4 >= u4) mvlineasm4(d4-u4+1,(void *)(ylookup[u4]+p));

		i = p+ylookup[d4+1];
		if (y2ve[0] > d4) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
		if (y2ve[1] > d4) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
		if (y2ve[2] > d4) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
		if (y2ve[3] > d4) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
	}
	for(;x<=x2;x++,p++)
	{
		y1ve[0] = std::max(static_cast<int>(uwal[x]), static_cast<int>(startumost[x + windowx1]) - windowy1);
		y2ve[0] = std::min(static_cast<int>(dwal[x]), static_cast<int>(startdmost[x + windowx1]) - windowy1);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(p+ylookup[y1ve[0]]));
	}

#else	// USING_A_C

	intptr_t p = startx + frameoffset;
	
	for(int x{startx}; x <= x2; x++, p++)
	{
		y1ve[0] = std::max(static_cast<int>(uwal[x]), static_cast<int>(startumost[x + windowx1]) - windowy1);
		y2ve[0] = std::min(static_cast<int>(dwal[x]), static_cast<int>(startdmost[x + windowx1]) - windowy1);
		
		if (y2ve[0] <= y1ve[0])
			continue;

		palookupoffse[0] = fpalookup + (getpalookup((int)mulscalen<16>(swal[x], globvis), globalshade) << 8);

		bufplce[0] = lwal[x] + globalxpanning;

		if (bufplce[0] >= tsizx) {
			if (xnice == 0)
				bufplce[0] %= tsizx;
			else
				bufplce[0] &= tsizx;
		}

		if (ynice == 0)
			bufplce[0] *= tsizy;
		else
			bufplce[0] <<= tsizy;

		vince[0] = swal[x] * globalyscale;
		vplce[0] = globalzd + vince[0] * (y1ve[0] - globalhoriz + 1);

		mvlineasm1(vince[0], (void *)palookupoffse[0], y2ve[0] - y1ve[0] - 1, vplce[0], (void *)(bufplce[0] + waloff[globalpicnum]), (void *)(p+ylookup[y1ve[0]]));
	}

#endif

	faketimerhandler();
}

} // namespace

//
// wallfront (internal)
//
int wallfront(int l1, int l2)
{
	walltype* wal = &wall[thewall[l1]];
	const int x11 = wal->pt.x;
	const int y11 = wal->pt.y;

	wal = &wall[wal->point2];
	const int x21 = wal->pt.x;
	const int y21 = wal->pt.y;

	wal = &wall[thewall[l2]];
	const int x12 = wal->pt.x;
	const int y12 = wal->pt.y;

	wal = &wall[wal->point2];
	const int x22 = wal->pt.x;
	const int y22 = wal->pt.y;

	int dx = x21 - x11;
	int dy = y21 - y11;

	int t1 = dmulscalen<2>(x12 - x11, dy, -dx, y12 - y11); //p1(l2) vs. l1
	int t2 = dmulscalen<2>(x22 - x11, dy, -dx, y22 - y11); //p2(l2) vs. l1
	
	if (t1 == 0) {
		t1 = t2;
		if (t1 == 0)
			return -1;
	}

	if (t2 == 0)
		t2 = t1;
	
	if ((t1 ^ t2) >= 0)
	{
		t2 = dmulscalen<2>(globalpos.x-x11,dy,-dx,globalpos.y-y11); //pos vs. l1
		return (t2 ^ t1) >= 0;
	}

	dx = x22 - x12;
	dy = y22 - y12;

	t1 = dmulscalen<2>(x11 - x12, dy, -dx, y11 - y12); //p1(l1) vs. l2
	t2 = dmulscalen<2>(x21 - x12, dy, -dx, y21 - y12); //p2(l1) vs. l2
	
	if (t1 == 0) {
		t1 = t2;
		if (t1 == 0)
			return -1;
	}

	if (t2 == 0)
		t2 = t1;

	if ((t1 ^ t2) >= 0)
	{
		t2 = dmulscalen<2>(globalpos.x - x12, dy, -dx, globalpos.y - y12); //pos vs. l2
		return (t2 ^ t1) < 0;
	}

	return -2;
}


namespace {

//
// spritewallfront (internal)
//
bool spritewallfront(const spritetype *s, int w)
{
	walltype* wal = &wall[w];
	const int x1 = wal->pt.x;
	const int y1 = wal->pt.y;

	wal = &wall[wal->point2];

	return dmulscalen<32>(wal->pt.x - x1, s->y - y1, -(s->x - x1), wal->pt.y - y1) >= 0;
}


//
// bunchfront (internal)
//
int bunchfront(int b1, int b2)
{
	const int b1f = bunchfirst[b1];
	const int x1b1 = xb1[b1f];
	const int x2b2 = xb2[bunchlast[b2]] + 1;

	if (x1b1 >= x2b2) {
		return -1;
	}

	const int b2f = bunchfirst[b2];
	const int x1b2 = xb1[b2f];
	const int x2b1 = xb2[bunchlast[b1]]+1;

	if (x1b2 >= x2b1) {
		return -1;
	}

	if (x1b1 >= x1b2)
	{
		int i{b2f};
		for(; xb2[i] < x1b1; i = p2[i]);

		return wallfront(b1f, i);
	}

	int i{b1f};
	for(; xb2[i] < x1b2; i=p2[i]);

	return wallfront(i, b2f);
}


//
// hline (internal)
//
void hline(int xr, int yp)
{
	const int xl = lastx[yp];

	if (xl > xr) {
		return;
	}

	const int r = horizlookup2[yp - globalhoriz + horizycent];
	asm1 = g_pt1.x * r;
	asm2 = g_pt2.y * r;
	const int s = ((int)getpalookup((int)mulscalen<16>(r,globvis),globalshade)<<8);

	hlineasm4(xr - xl, 0L, s, g_pt2.x * r + globalypanning, g_pt1.y * r + globalxpanning,
		(void *)(ylookup[yp]+xr+frameoffset));
}


//
// slowhline (internal)
//
void slowhline(int xr, int yp)
{
	const int xl = lastx[yp];

	if (xl > xr) {
		return;
	}

	const int r = horizlookup2[yp-globalhoriz+horizycent];

	asm1 = g_pt1.x * r;
	asm2 = g_pt2.y * r;

	asm3 = (intptr_t)globalpalwritten + ((int)getpalookup((int)mulscalen<16>(r,globvis),globalshade)<<8);
	if ((globalorientation & 256) == 0)
	{
		mhline((void *)globalbufplc,g_pt1.y*r+globalxpanning-asm1*(xr-xl),(xr-xl)<<16,0L,
			g_pt2.x*r+globalypanning-asm2*(xr-xl),(void *)(ylookup[yp]+xl+frameoffset));
		return;
	}

	thline((void *)globalbufplc,g_pt1.y*r+globalxpanning-asm1*(xr-xl),(xr-xl)<<16,0L,
		g_pt2.x*r+globalypanning-asm2*(xr-xl),(void *)(ylookup[yp]+xl+frameoffset));
}


//
// prepwall (internal)
//
void prepwall(int z, const walltype *wal)
{
	int l{0};
	int ol{0};

	int walxrepeat = (wal->xrepeat << 3);

	//lwall calculation
	int i = xb1[z]-halfxdimen;
	const int topinc = -(ry1[z]>>2);
	const int botinc = ((ry2[z]-ry1[z])>>8);
	int top = mulscalen<5>(rx1[z],xdimen)+mulscalen<2>(topinc,i);
	int bot = mulscalen<11>(rx1[z]-rx2[z],xdimen)+mulscalen<2>(botinc,i);

	int splc = mulscalen<19>(ry1[z],xdimscale);
	const int sinc = mulscalen<16>(ry2[z]-ry1[z],xdimscale);

	int x = xb1[z];

	if (bot != 0) {
		l = divscalen<12>(top,bot);
		swall[x] = mulscalen<21>(l,sinc)+splc;
		l *= walxrepeat;
		lwall[x] = (l>>18);
	}

	while (x+4 <= xb2[z]) {
		top += topinc;
		bot += botinc;

		if (bot != 0) {
			ol = l;
			l = divscalen<12>(top,bot);
			swall[x+4] = mulscalen<21>(l,sinc)+splc;
			l *= walxrepeat;
			lwall[x+4] = (l>>18);
		}

		i = ((ol+l)>>1);
		lwall[x+2] = (i>>18);
		lwall[x+1] = ((ol+i)>>19);
		lwall[x+3] = ((l+i)>>19);
		swall[x+2] = ((swall[x]+swall[x+4])>>1);
		swall[x+1] = ((swall[x]+swall[x+2])>>1);
		swall[x+3] = ((swall[x+4]+swall[x+2])>>1);
		x += 4;
	}

	if (x+2 <= xb2[z])
	{
		top += (topinc>>1);
		bot += (botinc>>1);
		
		if (bot != 0) {
			ol = l;
			l = divscalen<12>(top,bot);
			swall[x+2] = mulscalen<21>(l,sinc)+splc;
			l *= walxrepeat;
			lwall[x+2] = (l>>18);
		}

		lwall[x+1] = ((l+ol)>>19);
		swall[x+1] = ((swall[x]+swall[x+2])>>1);
		x += 2;
	}

	if (x+1 <= xb2[z])
	{
		bot += (botinc>>2);

		if (bot != 0) {
			l = divscalen<12>(top+(topinc>>2),bot);
			swall[x+1] = mulscalen<21>(l,sinc)+splc;
			lwall[x+1] = mulscalen<18>(l,walxrepeat);
		}
	}

	if (lwall[xb1[z]] < 0)
		lwall[xb1[z]] = 0;
	
	if ((lwall[xb2[z]] >= walxrepeat) && (walxrepeat))
		lwall[xb2[z]] = walxrepeat-1;
	
	if (wal->cstat&8) {
		walxrepeat--;

		std::ranges::for_each(std::next(lwall.begin(), xb1[z]),
		                      std::next(lwall.begin(), xb2[z] + 1),
							  [walxrepeat](auto& aWall) {
								aWall = walxrepeat - aWall;
							  });
	}
}

} // namespace

//
// animateoffs (internal)
//
int animateoffs(short tilenum, short fakevar)
{
	std::ignore = fakevar;

	const int i = (totalclocklock >> ((picanm[tilenum] >> 24) & 15));

	if ((picanm[tilenum] & 63) > 0)
	{
		switch(picanm[tilenum]&192)
		{
			case 64: {
				const int k = (i%((picanm[tilenum]&63)<<1));
				if (k < (picanm[tilenum] & 63)) {
					return k;
		}
				else {
					return (((picanm[tilenum] & 63) << 1) - k);
	}
			}
			case 128: {
				return (i % ((picanm[tilenum] & 63) + 1));
			}
			case 192: {
				return -(i % ((picanm[tilenum] & 63) + 1));
			}
		}
	}

	return 0;
}


namespace {

//
// owallmost (internal)
//
int owallmost(std::span<short> mostbuf, int w, int z)
{
	z <<= 7;
	const int s1 = mulscalen<20>(globaluclip, yb1[w]);
	const int s2 = mulscalen<20>(globaluclip, yb2[w]);
	const int s3 = mulscalen<20>(globaldclip, yb1[w]);
	const int s4 = mulscalen<20>(globaldclip, yb2[w]);
	const int bad = (z < s1) + ((z < s2) << 1) + ((z > s3) << 2) + ((z > s4) << 3);

	int ix1 = xb1[w];
	int iy1 = yb1[w];
	int ix2 = xb2[w];
	int iy2 = yb2[w];

	if ((bad & 3) == 3)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),0L);
		std::ranges::subrange submost{ std::next(mostbuf.begin(), ix1),
		                               std::next(mostbuf.begin(), ix2 + 1) };

		std::ranges::fill(submost, 0);

		return bad;
	}

	if ((bad & 12) == 12)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
		std::ranges::subrange submost{ std::next(mostbuf.begin(), ix1),
								       std::next(mostbuf.begin(), ix2 + 1) };

		std::ranges::fill(submost, ydimen);

		return bad;
	}

	if (bad & 3)
	{
		const int t = divscalen<30>(z - s1, s2 - s1);
		const int inty = yb1[w] + mulscalen<30>(yb2[w] - yb1[w], t);
		const int xcross = xb1[w] + scale(mulscalen<30>(yb2[w], t), xb2[w] - xb1[w], inty);

		if ((bad & 3) == 2)
		{
			if (xb1[w] <= xcross) {
				iy2 = inty;
				ix2 = xcross;
			}
			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),0L);
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xcross + 1),
								           std::next(mostbuf.begin(), xb2[w] + 1) };

			std::ranges::fill(submost, 0);
		}
		else
		{
			if (xcross <= xb2[w]) {
				iy1 = inty;
				ix1 = xcross;
			}
			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),0L);
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xb1[w]),
								           std::next(mostbuf.begin(), xcross + 1) };

			std::ranges::fill(submost, 0);
		}
	}

	if (bad & 12)
	{
		const int t = divscalen<30>(z - s3, s4 - s3);
		const int inty = yb1[w] + mulscalen<30>(yb2[w] - yb1[w], t);
		const int xcross = xb1[w] + scale(mulscalen<30>(yb2[w], t), xb2[w] - xb1[w], inty);

		if ((bad & 12) == 8)
		{
			if (xb1[w] <= xcross) {
				iy2 = inty;
				ix2 = xcross;
			}

			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xcross + 1),
								           std::next(mostbuf.begin(), xb2[w] + 1) };

			std::ranges::fill(submost, ydimen);
		}
		else
		{
			if (xcross <= xb2[w]) {
				iy1 = inty;
				ix1 = xcross;
			}

			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xb1[w]),
								           std::next(mostbuf.begin(), xcross + 1) };

			std::ranges::fill(submost, ydimen);
		}
	}

	const int y = (scale(z, xdimenscale, iy1) << 4);
	const int yinc = ((scale(z, xdimenscale, iy2) << 4) - y) / (ix2 - ix1 + 1);

	qinterpolatedown16short(&mostbuf[ix1], ix2 - ix1 + 1, y + (globalhoriz << 16), yinc);

	if (mostbuf[ix1] < 0)
		mostbuf[ix1] = 0;

	if (mostbuf[ix1] > ydimen)
		mostbuf[ix1] = ydimen;

	if (mostbuf[ix2] < 0)
		mostbuf[ix2] = 0;

	if (mostbuf[ix2] > ydimen)
		mostbuf[ix2] = ydimen;

	return bad;
}

} // namespace

//
// wallmost (internal)
//
int wallmost(std::span<short> mostbuf, int w, int sectnum, unsigned char dastat)
{
	int bad;
	int j;
	int t;
	int y;
	int z;
	int inty;
	int intz;
	int xcross;
	int yinc;
	int z1;
	int z2;
	int xv;
	int yv;
	int oz1;
	int oz2;
	int s1;
	int s2;
	int s3;
	int s4;
	int ix1;
	int ix2;
	int iy1;
	int iy2;

	if (dastat == 0)
	{
		z = g_sector[sectnum].ceilingz-globalpos.z;
		if ((g_sector[sectnum].ceilingstat&2) == 0) return(owallmost(mostbuf,w,z));
	}
	else
	{
		z = g_sector[sectnum].floorz-globalpos.z;
		if ((g_sector[sectnum].floorstat&2) == 0) return(owallmost(mostbuf,w,z));
	}

	int i = thewall[w];
	
	if (i == g_sector[sectnum].wallptr)
		return(owallmost(mostbuf,w,z));

	const int x1 = wall[i].pt.x;
	const int x2 = wall[wall[i].point2].pt.x - x1;
	const int y1 = wall[i].pt.y;
	const int y2 = wall[wall[i].point2].pt.y - y1;

	const int fw = g_sector[sectnum].wallptr;
	i = wall[fw].point2;
	const int dx = wall[i].pt.x - wall[fw].pt.x;
	const int dy = wall[i].pt.y - wall[fw].pt.y;
	const int dasqr = krecipasm(static_cast<int>(std::hypot(dx, dy)));

	if (xb1[w] == 0) {
		xv = cosglobalang+sinviewingrangeglobalang;
		yv = singlobalang-cosviewingrangeglobalang;
	}
	else {
		xv = x1 - globalpos.x;
		yv = y1 - globalpos.y;
	}

	i = xv * (y1 - globalpos.y) - yv * (x1 - globalpos.x);
	j = yv * x2 - xv * y2;

	if (std::abs(j) > std::abs(i >> 3))
		i = divscalen<28>(i, j);

	if (dastat == 0) {
		t = mulscalen<15>(g_sector[sectnum].ceilingheinum,dasqr);
		z1 = g_sector[sectnum].ceilingz;
	}
	else {
		t = mulscalen<15>(g_sector[sectnum].floorheinum,dasqr);
		z1 = g_sector[sectnum].floorz;
	}

	z1 = dmulscalen<24>(dx*t,mulscalen<20>(y2,i)+((y1-wall[fw].pt.y)<<8),
						 -dy*t,mulscalen<20>(x2,i)+((x1-wall[fw].pt.x)<<8))+((z1-globalpos.z)<<7);


	if (xb2[w] == xdimen - 1) {
		xv = cosglobalang - sinviewingrangeglobalang;
		yv = singlobalang + cosviewingrangeglobalang;
	}
	else {
		xv = (x2 + x1) - globalpos.x;
		yv = (y2 + y1) - globalpos.y;
	}

	i = xv * (y1 - globalpos.y) - yv * (x1 - globalpos.x);
	j = yv * x2 - xv * y2;

	if (std::abs(j) > std::abs(i >> 3))
		i = divscalen<28>(i, j);

	if (dastat == 0)
	{
		t = mulscalen<15>(g_sector[sectnum].ceilingheinum,dasqr);
		z2 = g_sector[sectnum].ceilingz;
	}
	else
	{
		t = mulscalen<15>(g_sector[sectnum].floorheinum,dasqr);
		z2 = g_sector[sectnum].floorz;
	}

	z2 = dmulscalen<24>(dx*t,mulscalen<20>(y2,i)+((y1-wall[fw].pt.y)<<8),
						 -dy*t,mulscalen<20>(x2,i)+((x1-wall[fw].pt.x)<<8))+((z2-globalpos.z)<<7);


	s1 = mulscalen<20>(globaluclip,yb1[w]);
	s2 = mulscalen<20>(globaluclip,yb2[w]);
	s3 = mulscalen<20>(globaldclip,yb1[w]);
	s4 = mulscalen<20>(globaldclip,yb2[w]);
	bad = (z1<s1)+((z2<s2)<<1)+((z1>s3)<<2)+((z2>s4)<<3);

	ix1 = xb1[w];
	ix2 = xb2[w];
	iy1 = yb1[w];
	iy2 = yb2[w];
	oz1 = z1;
	oz2 = z2;

	if ((bad & 3) == 3)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),0L);
		std::ranges::subrange submost{ std::next(mostbuf.begin(), ix1),
		                               std::next(mostbuf.begin(), ix2 + 1)};

		std::ranges::fill(submost, 0);

		return bad;
	}

	if ((bad & 12) == 12)
	{
		//clearbufbyte(&mostbuf[ix1],(ix2-ix1+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
		std::ranges::subrange submost{ std::next(mostbuf.begin(), ix1),
		                               std::next(mostbuf.begin(), ix2 + 1)};

		std::ranges::fill(submost, ydimen);

		return bad;
	}

	if (bad & 3)
	{
			//inty = intz / (globaluclip>>16)
		t = divscalen<30>(oz1-s1,s2-s1+oz1-oz2);
		inty = yb1[w] + mulscalen<30>(yb2[w]-yb1[w],t);
		intz = oz1 + mulscalen<30>(oz2-oz1,t);
		xcross = xb1[w] + scale(mulscalen<30>(yb2[w],t),xb2[w]-xb1[w],inty);

		//t = divscalen<30>((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscalen<30>(yb2[w]-yb1[w],t);
		//intz = z1 + mulscalen<30>(z2-z1,t);

		if ((bad&3) == 2)
		{
			if (xb1[w] <= xcross) {
				z2 = intz;
				iy2 = inty;
				ix2 = xcross;
			}
			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),0L);
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xcross + 1),
		                                   std::next(mostbuf.begin(), xb2[w] + 1)};

			std::ranges::fill(submost, 0);
		}
		else
		{
			if (xcross <= xb2[w]) {
				z1 = intz;
				iy1 = inty;
				ix1 = xcross;
			}
			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),0L);
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xb1[w]),
		                                   std::next(mostbuf.begin(), xcross + 1)};

			std::ranges::fill(submost, 0);
		}
	}

	if (bad & 12)
	{
			//inty = intz / (globaldclip>>16)
		t = divscalen<30>(oz1-s3,s4-s3+oz1-oz2);
		inty = yb1[w] + mulscalen<30>(yb2[w]-yb1[w],t);
		intz = oz1 + mulscalen<30>(oz2-oz1,t);
		xcross = xb1[w] + scale(mulscalen<30>(yb2[w],t),xb2[w]-xb1[w],inty);

		//t = divscalen<30>((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscalen<30>(yb2[w]-yb1[w],t);
		//intz = z1 + mulscalen<30>(z2-z1,t);

		if ((bad & 12) == 8)
		{
			if (xb1[w] <= xcross) {
				z2 = intz;
				iy2 = inty;
				ix2 = xcross;
			}
			//clearbufbyte(&mostbuf[xcross+1],(xb2[w]-xcross)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xcross + 1),
		                                   std::next(mostbuf.begin(), xb2[w] + 1)};

				std::ranges::fill(submost, ydimen);		
		}
		else
		{
			if (xcross <= xb2[w]) {
				z1 = intz;
				iy1 = inty;
				ix1 = xcross;
			}

			//clearbufbyte(&mostbuf[xb1[w]],(xcross-xb1[w]+1)*sizeof(mostbuf[0]),ydimen+(ydimen<<16));
			std::ranges::subrange submost{ std::next(mostbuf.begin(), xb1[w]),
		                                   std::next(mostbuf.begin(), xcross + 1)};

			std::ranges::fill(submost, ydimen);
		}
	}

	y = (scale(z1, xdimenscale, iy1) << 4);
	yinc = ((scale(z2, xdimenscale, iy2) << 4) - y) / (ix2 - ix1 + 1);
	qinterpolatedown16short(&mostbuf[ix1],ix2-ix1+1,y+(globalhoriz<<16),yinc);

	if (mostbuf[ix1] < 0)
		mostbuf[ix1] = 0;

	if (mostbuf[ix1] > ydimen)
		mostbuf[ix1] = ydimen;

	if (mostbuf[ix2] < 0)
		mostbuf[ix2] = 0;

	if (mostbuf[ix2] > ydimen)
		mostbuf[ix2] = ydimen;

	return bad;
}

namespace {

//
// ceilscan (internal)
//
void ceilscan(int x1, int x2, int sectnum)
{
	const sectortype* sec = &g_sector[sectnum];
	
	if (palookup[sec->ceilingpal].data() != globalpalwritten)
	{
		globalpalwritten = palookup[sec->ceilingpal].data();
		if (!globalpalwritten) globalpalwritten = palookup[globalpal].data();	// JBF: fixes null-pointer crash
		setpalookupaddress(globalpalwritten);
	}

	globalzd = sec->ceilingz-globalpos.z;
	
	if (globalzd > 0)
		return;
	
	globalpicnum = sec->ceilingpicnum;
	
	if ((unsigned)globalpicnum >= (unsigned)MAXTILES)
		globalpicnum = 0;
	
	setgotpic(globalpicnum);
	
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0))
		return;

	if (picanm[globalpicnum] & 192)
		globalpicnum += animateoffs((short)globalpicnum,(short)sectnum);

	if (waloff[globalpicnum] == 0)
		loadtile(globalpicnum);
	
	globalbufplc = waloff[globalpicnum];

	globalshade = (int)sec->ceilingshade;
	globvis = globalcisibility;
	
	if (sec->visibility != 0)
		globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));

	globalorientation = (int)sec->ceilingstat;

	if ((globalorientation&64) == 0)
	{
		g_pt1 = {singlobalang, cosglobalang};
		g_pt2 = {singlobalang, cosglobalang};
		globalxpanning = (globalpos.x<<20);
		globalypanning = -(globalpos.y<<20);
	}
	else
	{
		int j = sec->wallptr;
		auto opt = wall[wall[j].point2].pt - wall[j].pt;
		int i = static_cast<int>(std::hypot(opt.x, opt.y));
		
		if (i == 0)
			i = 1024;
		else
			i = 1048576 / i;

		g_pt1.x = mulscalen<10>(dmulscalen<10>(opt.x,singlobalang,-opt.y,cosglobalang),i);
		g_pt1.y = mulscalen<10>(dmulscalen<10>(opt.x,cosglobalang,opt.y,singlobalang),i);
		g_pt2 = -g_pt1;
		
		opt.x = (wall[j].pt.x - globalpos.x);
		opt.y = (wall[j].pt.y - globalpos.y);
		opt <<= 6;

		i = dmulscalen<14>(opt.y, cosglobalang,-opt.x, singlobalang);
		j = dmulscalen<14>(opt.x, cosglobalang, opt.y, singlobalang);
		opt.x = i;
		opt.y = j;
		globalxpanning = g_pt1.x * opt.x - g_pt1.y * opt.y;
		globalypanning = g_pt2.y * opt.x + g_pt2.x * opt.y;
	}

	g_pt2.x = mulscalen<16>(g_pt2.x,viewingrangerecip);
	g_pt1.y = mulscalen<16>(g_pt1.y,viewingrangerecip);
	globalxshift = (8-(picsiz[globalpicnum]&15));
	globalyshift = (8-(picsiz[globalpicnum]>>4));

	if (globalorientation & 8) {
		globalxshift++;
		globalyshift++;
	}

	if ((globalorientation&0x4) > 0)
	{
		std::swap(globalxpanning, globalypanning);

		const int i = g_pt2.x;
		g_pt2.x = -g_pt1.y;
		g_pt1.y = -i;

		std::swap(g_pt1.x, g_pt2.y);
	}

	if ((globalorientation&0x10) > 0) {
		g_pt1 = -g_pt1;
		globalxpanning = -globalxpanning;
	}

	if ((globalorientation&0x20) > 0) {
		g_pt2 = -g_pt2;
		globalypanning = -globalypanning;
	}

	g_pt1 <<= globalxshift;
	g_pt2 <<= globalyshift;
	globalxpanning <<= globalxshift;
	globalypanning <<= globalyshift;
	globalxpanning += (((int)sec->ceilingxpanning)<<24);
	globalypanning += (((int)sec->ceilingypanning)<<24);
	g_pt1.y = (-g_pt1.x-g_pt1.y)*halfxdimen;
	g_pt2.x = (g_pt2.x-g_pt2.y)*halfxdimen;

	sethlinesizes(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4,(void *)globalbufplc);

	g_pt2.x += g_pt2.y*(x1-1);
	g_pt1.y += g_pt1.x*(x1-1);
	g_pt1 = mulscalen<16>(g_pt1, globalzd);
	g_pt2 = mulscalen<16>(g_pt2, globalzd);
	globvis = std::abs(mulscalen<10>(globvis,globalzd));

	if (!(globalorientation&0x180))
	{
		int y1 = umost[x1];
		int y2 = y1;

		for(int x{x1}; x <= x2; ++x)
		{
			const int twall = umost[x]-1;
			const int bwall = std::min(uplc[x], dmost[x]);

			if (twall < bwall-1)
			{
				if (twall >= y2) {
					while (y1 < y2-1) {
						hline(x - 1, ++y1);
					}

					y1 = twall;
				}
				else {
					while (y1 < twall) hline(x-1,++y1);
					while (y1 > twall) lastx[y1--] = x;
				}

				while (y2 > bwall) {
					hline(x-1,--y2);
				}

				while (y2 < bwall) {
					lastx[y2++] = x;
				}
			}
			else
			{
				while (y1 < y2-1) {
					hline(x-1,++y1);
				}

				if (x == x2) {
					g_pt2.x += g_pt2.y;
					g_pt1.y += g_pt1.x;
					break;
				}

				y1 = umost[x+1];
				y2 = y1;
			}
			
			g_pt2.x += g_pt2.y;
			g_pt1.y += g_pt1.x;
		}
		while (y1 < y2-1) {
			hline(x2, ++y1);
		}

		faketimerhandler();

		return;
	}

	switch(globalorientation&0x180)
	{
		case 128:
			msethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 256:
			settransnormal();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 384:
			settransreverse();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
	}

	int y1 = umost[x1];
	int y2 = y1;
	for(int x{x1}; x <= x2; ++x)
	{
		const int twall = umost[x]-1;
		const int bwall = std::min(uplc[x], dmost[x]);
		
		if (twall < bwall-1) {
			if (twall >= y2) {
				while (y1 < y2-1) slowhline(x-1,++y1);
				y1 = twall;
			}
			else {
				while (y1 < twall) {
					slowhline(x - 1, ++y1);
				}
				
				while (y1 > twall) {
					lastx[y1--] = x;
				}
			}

			while (y2 > bwall) {
				slowhline(x - 1, --y2);
			}

			while (y2 < bwall) {
				lastx[y2++] = x;
			}
		}
		else
		{
			while (y1 < y2 - 1) {
				slowhline(x - 1, ++y1);
			}

			if (x == x2) {
				g_pt2.x += g_pt2.y;
				g_pt1.y += g_pt1.x;
				break;
			}

			y1 = umost[x + 1];
			y2 = y1;
		}

		g_pt2.x += g_pt2.y;
		g_pt1.y += g_pt1.x;
	}

	while (y1 < y2-1) {
		slowhline(x2, ++y1);
	}

	faketimerhandler();
}


//
// florscan (internal)
//
void florscan(int x1, int x2, int sectnum)
{
	int i;
	int j;
	int x;
	int y1;
	int y2;
	int twall;
	int bwall;

	const sectortype* sec = &g_sector[sectnum];

	if (palookup[sec->floorpal].data() != globalpalwritten)
	{
		globalpalwritten = palookup[sec->floorpal].data();
		if (!globalpalwritten) globalpalwritten = palookup[globalpal].data();	// JBF: fixes null-pointer crash
		setpalookupaddress(globalpalwritten);
	}

	globalzd = globalpos.z-sec->floorz;
	
	if (globalzd > 0)
		return;
	
	globalpicnum = sec->floorpicnum;
	
	if ((unsigned)globalpicnum >= (unsigned)MAXTILES)
		globalpicnum = 0;
	
	setgotpic(globalpicnum);
	
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0))
		return;
	
	if (picanm[globalpicnum] & 192)
		globalpicnum += animateoffs((short)globalpicnum,(short)sectnum);

	if (waloff[globalpicnum] == 0)
		loadtile(globalpicnum);
	
	globalbufplc = waloff[globalpicnum];

	globalshade = (int)sec->floorshade;
	globvis = globalcisibility;
	
	if (sec->visibility != 0)
		globvis = mulscalen<4>(globvis, (int)((unsigned char)(sec->visibility + 16)));

	globalorientation = (int)sec->floorstat;

	point2di opt;

	if ((globalorientation&64) == 0)
	{
		g_pt1 = {singlobalang, cosglobalang};
		g_pt2 = {singlobalang, cosglobalang};
		globalxpanning = globalpos.x << 20;
		globalypanning = -(globalpos.y << 20);
	}
	else
	{
		j = sec->wallptr;
		opt = wall[wall[j].point2].pt - wall[j].pt;
		i = static_cast<int>(std::hypot(opt.x, opt.y));
		
		if (i == 0)
			i = 1024;
		else
			i = 1048576/i;

		g_pt1.x = mulscalen<10>(dmulscalen<10>(opt.x, singlobalang, -opt.y, cosglobalang), i);
		g_pt1.y = mulscalen<10>(dmulscalen<10>(opt.x, cosglobalang,  opt.y, singlobalang), i);
		g_pt2 = -g_pt1;

		opt.x = ((wall[j].pt.x - globalpos.x) << 6);
		opt.y = ((wall[j].pt.y - globalpos.y) << 6);
		i = dmulscalen<14>(opt.y,cosglobalang,-opt.x,singlobalang);
		j = dmulscalen<14>(opt.x,cosglobalang,opt.y,singlobalang);
		opt.x = i;
		opt.y = j;
		globalxpanning = g_pt1.x*opt.x - g_pt1.y*opt.y;
		globalypanning = g_pt2.y*opt.x + g_pt2.x*opt.y;
	}
	g_pt2.x = mulscalen<16>(g_pt2.x,viewingrangerecip);
	g_pt1.y = mulscalen<16>(g_pt1.y,viewingrangerecip);
	globalxshift = (8-(picsiz[globalpicnum]&15));
	globalyshift = (8-(picsiz[globalpicnum]>>4));
	if (globalorientation&8) { globalxshift++; globalyshift++; }

	if ((globalorientation&0x4) > 0)
	{
		std::swap(globalxpanning, globalypanning);

		i = g_pt2.x;
		g_pt2.x = -g_pt1.y;
		g_pt1.y = -i;
		
		std::swap(g_pt1.x, g_pt2.y);
	}

	if ((globalorientation&0x10) > 0) {
		g_pt1 = -g_pt1;
		globalxpanning = -globalxpanning;
	}

	if ((globalorientation&0x20) > 0) {
		g_pt2 = -g_pt2;
		globalypanning = -globalypanning;
	}

	g_pt1 <<= globalxshift;
	g_pt2 <<= globalyshift;
	globalxpanning <<= globalxshift;
	globalypanning <<= globalyshift;
	globalxpanning += (((int)sec->floorxpanning)<<24);
	globalypanning += (((int)sec->floorypanning)<<24);
	g_pt1.y = (-g_pt1.x-g_pt1.y)*halfxdimen;
	g_pt2.x = (g_pt2.x-g_pt2.y)*halfxdimen;

	sethlinesizes(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4,(void *)globalbufplc);

	g_pt2.x += g_pt2.y*(x1-1);
	g_pt1.y += g_pt1.x*(x1-1);
	g_pt1 = mulscalen<16>(g_pt1, globalzd);
	g_pt2 = mulscalen<16>(g_pt2, globalzd);
	globvis = std::abs(mulscalen<10>(globvis,globalzd));

	if (!(globalorientation&0x180))
	{
		y1 = std::max(dplc[x1], umost[x1]);
		y2 = y1;
		for(x=x1;x<=x2;x++)
		{
			twall = std::max(dplc[x], umost[x]) - 1;
			bwall = dmost[x];
			if (twall < bwall-1)
			{
				if (twall >= y2)
				{
					while (y1 < y2-1) hline(x-1,++y1);
					y1 = twall;
				}
				else
				{
					while (y1 < twall)
						hline(x - 1, ++y1);
					while (y1 > twall)
						lastx[y1--] = x;
				}
				while (y2 > bwall) hline(x-1,--y2);
				while (y2 < bwall) lastx[y2++] = x;
			}
			else
			{
				while (y1 < y2-1) hline(x-1,++y1);
				if (x == x2) {
					g_pt2.x += g_pt2.y;
					g_pt1.y += g_pt1.x;
					break;
				}
				y1 = std::max(dplc[x + 1], umost[x + 1]);
				y2 = y1;
			}

			g_pt2.x += g_pt2.y;
			g_pt1.y += g_pt1.x;
		}
		while (y1 < y2-1) hline(x2,++y1);
		faketimerhandler();
		return;
	}

	switch(globalorientation&0x180)
	{
		case 128:
			msethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 256:
			settransnormal();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
		case 384:
			settransreverse();
			tsethlineshift(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4);
			break;
	}

	y1 = std::max(dplc[x1], umost[x1]);
	y2 = y1;
	for(x=x1;x<=x2;x++)
	{
		twall = std::max(dplc[x], umost[x]) - 1;
		bwall = dmost[x];
		if (twall < bwall-1)
		{
			if (twall >= y2)
			{
				while (y1 < y2-1) slowhline(x-1,++y1);
				y1 = twall;
			}
			else
			{
				while (y1 < twall) slowhline(x-1,++y1);
				while (y1 > twall) lastx[y1--] = x;
			}
			while (y2 > bwall) slowhline(x-1,--y2);
			while (y2 < bwall) lastx[y2++] = x;
		}
		else
		{
			while (y1 < y2-1) slowhline(x-1,++y1);
			if (x == x2) {
				g_pt2.x += g_pt2.y;
				g_pt1.y += g_pt1.x;
				break;
			}
			y1 = std::max(dplc[x + 1], umost[x + 1]);
			y2 = y1;
		}
		g_pt2.x += g_pt2.y;
		g_pt1.y += g_pt1.x;
	}

	while (y1 < y2-1)
		slowhline(x2,++y1);

	faketimerhandler();
}


//
// wallscan (internal)
//
void wallscan(int x1, int x2, std::span<const short> uwal, std::span<const short> dwal, std::span<const int> swal, std::span<const int> lwal)
{
	int x;
	intptr_t i;
	intptr_t fpalookup;
	std::array<int, 4> y1ve;
	std::array<int, 4> y2ve;
	int z;
	char bad;

	if (x2 >= xdim)
		x2 = xdim - 1;

	int tsizx = tilesizx[globalpicnum];
	int tsizy = tilesizy[globalpicnum];
	setgotpic(globalpicnum);
	if ((tsizx <= 0) || (tsizy <= 0))
		return;

	if ((uwal[x1] > ydimen) && (uwal[x2] > ydimen))
		return;

	if ((dwal[x1] < 0) && (dwal[x2] < 0))
		return;

	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);

	const bool xnice = (pow2long[picsiz[globalpicnum] & 15] == tsizx);
	if (xnice)
		--tsizx;

	const bool ynice = (pow2long[picsiz[globalpicnum] >> 4] == tsizy);
	if (ynice)
		tsizy = (picsiz[globalpicnum]>>4);

	fpalookup = (intptr_t)palookup[globalpal].data();

	setupvlineasm(globalshiftval);

#ifndef USING_A_C

	x = x1;
	while ((umost[x] > dmost[x]) && (x <= x2))
		++x;

	for(;(x<=x2)&&((x+frameoffset)&3);x++)
	{
		y1ve[0] = std::max(uwal[x], umost[x]);
		y2ve[0] = std::min(dwal[x], dmost[x]);
		
		if (y2ve[0] <= y1ve[0])
			continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) {
			if (xnice == 0)
				bufplce[0] %= tsizx;
			else
				bufplce[0] &= tsizx;
		}

		if (ynice == 0)
			bufplce[0] *= tsizy;
		else
			bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		vlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(x+frameoffset+ylookup[y1ve[0]]));
	}
	for(;x<=x2-3;x+=4)
	{
		bad = 0;
		for(z=3;z>=0;z--)
		{
			y1ve[z] = std::max(uwal[x + z], umost[x + z]);
			y2ve[z] = std::min(dwal[x + z], dmost[x + z]) - 1;
			
			if (y2ve[z] < y1ve[z]) {
				bad += pow2char[z];
				continue;
			}

			i = lwal[x+z] + globalxpanning;
			if (i >= tsizx) {
				if (xnice == 0)
					i %= tsizx;
				else
					i &= tsizx;
			}

			if (ynice == 0)
				i *= tsizy;
			else
				i <<= tsizy;

			bufplce[z] = waloff[globalpicnum]+i;

			vince[z] = swal[x+z]*globalyscale;
			vplce[z] = globalzd + vince[z]*(y1ve[z]-globalhoriz+1);
		}
		
		if (bad == 15)
			continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x],globvis),globalshade)<<8);
		palookupoffse[3] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x+3],globvis),globalshade)<<8);

		if ((palookupoffse[0] == palookupoffse[3]) && ((bad&0x9) == 0))
		{
			palookupoffse[1] = palookupoffse[0];
			palookupoffse[2] = palookupoffse[0];
		}
		else
		{
			palookupoffse[1] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x+1],globvis),globalshade)<<8);
			palookupoffse[2] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x+2],globvis),globalshade)<<8);
		}

		int u4 = *std::ranges::max_element(y1ve);
		int d4 = *std::ranges::min_element(y2ve);

		if ((bad != 0) || (u4 >= d4))
		{
			if (!(bad&1)) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+x+frameoffset+0));
			if (!(bad&2)) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+x+frameoffset+1));
			if (!(bad&4)) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+x+frameoffset+2));
			if (!(bad&8)) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+x+frameoffset+3));
			continue;
		}

		if (u4 > y1ve[0]) vplce[0] = prevlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+x+frameoffset+0));
		if (u4 > y1ve[1]) vplce[1] = prevlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+x+frameoffset+1));
		if (u4 > y1ve[2]) vplce[2] = prevlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+x+frameoffset+2));
		if (u4 > y1ve[3]) vplce[3] = prevlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+x+frameoffset+3));

		if (d4 >= u4) vlineasm4(d4-u4+1,(void *)(ylookup[u4]+x+frameoffset));

		i = x+frameoffset+ylookup[d4+1];
		if (y2ve[0] > d4) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
		if (y2ve[1] > d4) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
		if (y2ve[2] > d4) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
		if (y2ve[3] > d4) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
	}
	for(;x<=x2;x++)
	{
		y1ve[0] = std::max(uwal[x], umost[x]);
		y2ve[0] = std::min(dwal[x], dmost[x]);
		
		if (y2ve[0] <= y1ve[0])
			continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) {
			if (xnice == 0)
				bufplce[0] %= tsizx;
			else
				bufplce[0] &= tsizx;
		}

		if (ynice == 0)
			bufplce[0] *= tsizy;
		else
			bufplce[0] <<= tsizy;

		vince[0] = swal[x] * globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		vlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(x+frameoffset+ylookup[y1ve[0]]));
	}

#else	// USING_A_C
	std::ignore = i;
	std::ignore = z;
	std::ignore = bad;

	for(x=x1;x<=x2;x++)
	{
		y1ve[0] = std::max(uwal[x], umost[x]);
		y2ve[0] = std::min(dwal[x], dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		palookupoffse[0] = fpalookup+(getpalookup((int)mulscalen<16>(swal[x],globvis),globalshade)<<8);

		bufplce[0] = lwal[x] + globalxpanning;
		if (bufplce[0] >= tsizx) { if (xnice == 0) bufplce[0] %= tsizx; else bufplce[0] &= tsizx; }
		if (ynice == 0) bufplce[0] *= tsizy; else bufplce[0] <<= tsizy;

		vince[0] = swal[x]*globalyscale;
		vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);

		vlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0]-1,vplce[0],(void *)(bufplce[0]+waloff[globalpicnum]),(void *)(x+frameoffset+ylookup[y1ve[0]]));
	}

#endif

	faketimerhandler();
}


//
// transmaskvline (internal)
//
void transmaskvline(int x)
{
	if ((x < 0) || (x >= xdimen))
		return;

	const short y1v = std::max(static_cast<int>(uwall[x]), static_cast<int>(startumost[x + windowx1]) - windowy1);
	const short y2v = std::min(static_cast<int>(dwall[x]), static_cast<int>(startdmost[x + windowx1]) - windowy1) - 1;
	
	if (y2v < y1v)
		return;

	auto palookupoffs = (intptr_t)palookup[globalpal].data() + (getpalookup((int)mulscalen<16>(swall[x],globvis),globalshade)<<8);

	const int vinc = swall[x]*globalyscale;
	const int vplc = globalzd + vinc*(y1v-globalhoriz+1);

	int i = lwall[x] + globalxpanning;
	if (i >= tilesizx[globalpicnum])
		i %= tilesizx[globalpicnum];

	intptr_t bufplc = waloff[globalpicnum] + i * tilesizy[globalpicnum];

	intptr_t p = ylookup[y1v] + x + frameoffset;

	tvlineasm1(vinc,(void *)palookupoffs,y2v-y1v,vplc,(void *)bufplc,(void *)p);
}


//
// transmaskvline2 (internal)
//
#ifndef USING_A_C
void transmaskvline2(int x)
{
	if ((x < 0) || (x >= xdimen))
		return;
	
	if (x == xdimen-1) {
		transmaskvline(x);
		return;
	}

	const int x2 = x + 1;

	std::array<short, 2> y1ve{};
	std::array<short, 2> y2ve{};

	y1ve[0] = std::max(static_cast<int>(uwall[x]), static_cast<int>(startumost[x + windowx1]) - windowy1);
	y2ve[0] = std::min(static_cast<int>(dwall[x]), static_cast<int>(startdmost[x + windowx1]) - windowy1) - 1;
	
	if (y2ve[0] < y1ve[0]) {
		transmaskvline(x2);
		return;
	}

	y1ve[1] = std::max(static_cast<int>(uwall[x2]), static_cast<int>(startumost[x2 + windowx1]) - windowy1);
	y2ve[1] = std::min(static_cast<int>(dwall[x2]), static_cast<int>(startdmost[x2 + windowx1]) - windowy1) - 1;
	if (y2ve[1] < y1ve[1]) { transmaskvline(x); return; }

	palookupoffse[0] = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscalen<16>(swall[x],globvis),globalshade)<<8);
	palookupoffse[1] = (intptr_t)palookup[globalpal] + (getpalookup((int)mulscalen<16>(swall[x2],globvis),globalshade)<<8);

	setuptvlineasm2(globalshiftval,(void *)palookupoffse[0],(void *)palookupoffse[1]);

	vince[0] = swall[x]*globalyscale;
	vince[1] = swall[x2]*globalyscale;
	vplce[0] = globalzd + vince[0]*(y1ve[0]-globalhoriz+1);
	vplce[1] = globalzd + vince[1]*(y1ve[1]-globalhoriz+1);

	intptr_t i = lwall[x] + globalxpanning;

	if (i >= tilesizx[globalpicnum])
		i %= tilesizx[globalpicnum];
	
	bufplce[0] = waloff[globalpicnum] + i * tilesizy[globalpicnum];

	i = lwall[x2] + globalxpanning;
	if (i >= tilesizx[globalpicnum])
		i %= tilesizx[globalpicnum];

	bufplce[1] = waloff[globalpicnum] + i * tilesizy[globalpicnum];

	const int y1 = *std::max_element(y1ve);
	const int y2 = *std::min_element(y2ve);

	i = x + frameoffset;

	if (y1ve[0] != y1ve[1])
	{
		if (y1ve[0] < y1)
			vplce[0] = tvlineasm1(vince[0],(void *)palookupoffse[0],y1-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+i));
		else
			vplce[1] = tvlineasm1(vince[1],(void *)palookupoffse[1],y1-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+i+1));
	}

	if (y2 > y1)
	{
		asm1 = vince[1];
		asm2 = ylookup[y2]+i+1;
		tvlineasm2(vplce[1],vince[0],(void *)bufplce[0],(void *)bufplce[1],vplce[0],(void *)(ylookup[y1]+i));
	}
	else
	{
		asm1 = vplce[0];
		asm2 = vplce[1];
	}

	if (y2ve[0] > y2ve[1])
		tvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y2-1,asm1,(void *)bufplce[0],(void *)(ylookup[y2+1]+i));
	else if (y2ve[0] < y2ve[1])
		tvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y2-1,asm2,(void *)bufplce[1],(void *)(ylookup[y2+1]+i+1));

	faketimerhandler();
}
#endif

//
// transmaskwallscan (internal)
//
void transmaskwallscan(int x1, int x2)
{
	setgotpic(globalpicnum);
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) return;

	if (waloff[globalpicnum] == 0) loadtile(globalpicnum);

	setuptvlineasm(globalshiftval);

	int x{x1};
	while ((startumost[x+windowx1] > startdmost[x+windowx1]) && (x <= x2)) x++;
#ifndef USING_A_C
	if ((x <= x2) && (x&1)) transmaskvline(x), x++;
	while (x < x2) transmaskvline2(x), x += 2;
#endif
	while (x <= x2) transmaskvline(x), x++;
	faketimerhandler();
}


//
// ceilspritehline (internal)
//
void ceilspritehline(int x2, int y)
{
	//x = x1 + (x2-x1)t + (y1-y2)u  ~  x = 160v
	//y = y1 + (y2-y1)t + (x2-x1)u  ~  y = (scrx-160)v
	//z = z1 = z2                   ~  z = posz + (scry-horiz)v

	const int x1 = lastx[y];
	
	if (x2 < x1)
		return;

	const int v = mulscalen<20>(globalzd, horizlookup[y - globalhoriz + horizycent]);
	const int bx = mulscalen<14>(g_pt2.x * x1 + g_pt1.x, v) + globalxpanning;
	const int by = mulscalen<14>(g_pt2.y * x1 + g_pt1.y, v) + globalypanning;

	asm1 = mulscalen<14>(g_pt2.x,v);
	asm2 = mulscalen<14>(g_pt2.y,v);

	asm3 = (intptr_t)palookup[globalpal].data() + (getpalookup((int)mulscalen<28>(std::abs(v), globvis), globalshade) << 8);

	if ((globalorientation&2) == 0)
		mhline((void *)globalbufplc, bx, (x2 - x1) << 16, 0L, by, (void *)(ylookup[y] + x1 + frameoffset));
	else
	{
		thline((void *)globalbufplc, bx, (x2 - x1) << 16, 0L, by, (void *)(ylookup[y] + x1 + frameoffset));
	}
}


//
// ceilspritescan (internal)
//
void ceilspritescan(int x1, int x2)
{
	int y1 = uwall[x1];
	int y2 = y1;
	
	for(int x{x1}; x <= x2; ++x)
	{
		const int twall = uwall[x]- 1;
		const int bwall = dwall[x];
		
		if (twall < bwall - 1)
		{
			if (twall >= y2) {
				while (y1 < y2 - 1)
					ceilspritehline(x - 1, ++y1);
				y1 = twall;
			}
			else {
				while (y1 < twall)
					ceilspritehline(x - 1,++y1);
				
				while (y1 > twall)
					lastx[y1--] = x;
			}

			while (y2 > bwall)
				ceilspritehline(x - 1, --y2);

			while (y2 < bwall)
				lastx[y2++] = x;
		}
		else
		{
			while (y1 < y2 - 1)
				ceilspritehline(x - 1, ++y1);
			
			if (x == x2)
				break;

			y1 = uwall[x + 1];
			y2 = y1;
		}
	}

	while (y1 < y2 - 1)
		ceilspritehline(x2, ++y1);
	
	faketimerhandler();
}


//
// grouscan (internal)
//
constexpr auto BITSOFPRECISION{3};  //Don't forget to change this in A.ASM also!
void grouscan(int dax1, int dax2, int sectnum, unsigned char dastat)
{
	auto* sec = &g_sector[sectnum];

	int daslope{0};
	int daz{0};
	if (dastat == 0)
	{
		if (globalpos.z <= getceilzofslope(sectnum,globalpos.x,globalpos.y))
			return;  //Back-face culling
		globalorientation = sec->ceilingstat;
		globalpicnum = sec->ceilingpicnum;
		globalshade = sec->ceilingshade;
		globalpal = sec->ceilingpal;
		daslope = sec->ceilingheinum;
		daz = sec->ceilingz;
	}
	else
	{
		if (globalpos.z >= getflorzofslope(sectnum,globalpos.x,globalpos.y))
			return;  //Back-face culling
		globalorientation = sec->floorstat;
		globalpicnum = sec->floorpicnum;
		globalshade = sec->floorshade;
		globalpal = sec->floorpal;
		daslope = sec->floorheinum;
		daz = sec->floorz;
	}

	if (palookup[globalpal].empty())
		globalpal = 0;
	if ((picanm[globalpicnum] & 192) != 0)
		globalpicnum += animateoffs(globalpicnum,sectnum);
	
	setgotpic(globalpicnum);
	
	if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0))
		return;
	
	if (waloff[globalpicnum] == 0)
		loadtile(globalpicnum);

	auto* wal = &wall[sec->wallptr];
	int wx = wall[wal->point2].pt.x - wal->pt.x;
	int wy = wall[wal->point2].pt.y - wal->pt.y;
	const int dasqr = krecipasm(static_cast<int>(std::hypot(wx, wy)));
	int i = mulscalen<21>(daslope, dasqr);
	wx *= i;
	wy *= i;

	int globalx = -mulscalen<19>(singlobalang,xdimenrecip);
	int globaly = mulscalen<19>(cosglobalang,xdimenrecip);
	g_pt1.x = (globalpos.x<<8);
	g_pt1.y = -(globalpos.y<<8);
	i = (dax1-halfxdimen)*xdimenrecip;
	g_pt2.x = mulscalen<16>(cosglobalang<<4,viewingrangerecip) - mulscalen<27>(singlobalang,i);
	g_pt2.y = mulscalen<16>(singlobalang<<4,viewingrangerecip) + mulscalen<27>(cosglobalang,i);
	globalzd = (xdimscale<<9);
	int globalzx = -dmulscalen<17>(wx,g_pt2.y,-wy,g_pt2.x) + mulscalen<10>(1-globalhoriz,globalzd);
	const int globalz = -dmulscalen<25>(wx,globaly,-wy,globalx);

	if (globalorientation&64)  //Relative alignment
	{
		const int dx = mulscalen<14>(wall[wal->point2].pt.x - wal->pt.x, dasqr);
		const int dy = mulscalen<14>(wall[wal->point2].pt.y - wal->pt.y, dasqr);

		i = static_cast<int>(std::sqrt(daslope * daslope+16777216)); // FIXME: Magic number.

		int x{globalx};
		int y{globaly};

		globalx = dmulscalen<16>(x,dx,y,dy);
		globaly = mulscalen<12>(dmulscalen<16>(-y,dx,x,dy),i);

		x = ((wal->pt.x - globalpos.x)<<8);
		y = ((wal->pt.y - globalpos.y)<<8);
		g_pt1.x = dmulscalen<16>(-x,dx,-y,dy);
		g_pt1.y = mulscalen<12>(dmulscalen<16>(-y,dx,x,dy),i);

		x = g_pt2.x;
		y = g_pt2.y;
		g_pt2.x = dmulscalen<16>(x,dx,y,dy);
		g_pt2.y = mulscalen<12>(dmulscalen<16>(-y,dx,x,dy),i);
	}

	if (globalorientation&0x4)
	{
		i = globalx;
		globalx = -globaly;
		globaly = -i;
		std::swap(g_pt1.x, g_pt1.y);

		i = g_pt2.x;
		g_pt2.x = -g_pt2.y;
		g_pt2.y = -i;
	}

	if (globalorientation & 0x10) {
		g_pt1.x = -g_pt1.x;
		g_pt2.x = -g_pt2.x;
		globalx = -globalx;
	}

	if (globalorientation & 0x20) {
		g_pt1.y = -g_pt1.y;
		g_pt2.y = -g_pt2.y;
		globaly = -globaly;
	}

	daz = dmulscalen<9>(wx,globalpos.y-wal->pt.y, -wy,globalpos.x-wal->pt.x) + ((daz-globalpos.z)<<8);
	g_pt2.x = mulscalen<20>(g_pt2.x,daz);
	globalx = mulscalen<28>(globalx,daz);
	g_pt2.y = mulscalen<20>(g_pt2.y,-daz);
	globaly = mulscalen<28>(globaly,-daz);

	i = 8 - (picsiz[globalpicnum] & 15);
	int j = 8 - (picsiz[globalpicnum] >> 4);
	
	if (globalorientation & 8) {
		++i;
		++j;
	}

	g_pt1.x <<= (i + 12);
	g_pt2.x <<= i;
	globalx <<= i;
	g_pt1.y <<= (j + 12);
	g_pt2.y <<= j;
	globaly <<= j;

	if (dastat == 0)
	{
		g_pt1.x += (((int)sec->ceilingxpanning) << 24);
		g_pt1.y += (((int)sec->ceilingypanning) << 24);
	}
	else
	{
		g_pt1.x += (((int)sec->floorxpanning) << 24);
		g_pt1.y += (((int)sec->floorypanning) << 24);
	}

	asm1 = -(globalzd>>(16-BITSOFPRECISION));

	globvis = globalvisibility;

	if (sec->visibility != 0)
		globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));

	globvis = mulscalen<13>(globvis,daz);
	globvis = mulscalen<16>(globvis,xdimscale);

	setupslopevlin(((int)(picsiz[globalpicnum]&15))+(((int)(picsiz[globalpicnum]>>4))<<8),(void *)waloff[globalpicnum],-ylookup[1]);

	const int l = (globalzd >> 16);

	assert(SLOPALOOKUPSIZ - 4 - ydimen > 0);

	const int shinc = mulscalen<16>(globalz, xdimenscale);
	int shoffs{0};
	if (shinc > 0)
		shoffs = (4 << 15);
	else
		shoffs = ((SLOPALOOKUPSIZ-4-ydimen)<<15);

	int y1{0};

	if (dastat == 0)
		y1 = umost[dax1];
	else
		y1 = std::max(umost[dax1], dplc[dax1]);
	
	int m1 = mulscalen<16>(y1,globalzd) + (globalzx>>6);
		//Avoid visibility overflow by crossing horizon
	if (globalzd > 0)
		m1 += (globalzd >> 16);
	else
		m1 -= (globalzd >> 16);

	int m2 = m1 + l;
	std::vector<intptr_t> slopalookup(SLOPALOOKUPSIZ);

	intptr_t* mptr1 = &slopalookup[y1 + (shoffs >> 15)];
	intptr_t* mptr2 = mptr1 + 1;

	assert(y1 + (shoffs >> 15) >= 0);
	assert(y1 + (shoffs >> 15) <= SLOPALOOKUPSIZ-2);

	for(int x{dax1}; x <= dax2; ++x)
	{
		int y2{0};

		if (dastat == 0) {
			y1 = umost[x];
			y2 = std::min(dmost[x], uplc[x]) - 1;
		}
		else {
			y1 = std::max(umost[x], dplc[x]);
			y2 = dmost[x]-1;
		}
		
		if (y1 <= y2)
		{
			assert(y1 + (shoffs >> 15) >= 0);
			assert(y1 + (shoffs >> 15) <= SLOPALOOKUPSIZ - 1);
			assert(y2 + (shoffs >> 15) >= 0);
			assert(y2 + (shoffs >> 15) <= SLOPALOOKUPSIZ - 1);

			const intptr_t* nptr1 = &slopalookup[y1 + (shoffs >> 15)];
			intptr_t* nptr2 = &slopalookup[y2 + (shoffs >> 15)];
			
			while (nptr1 <= mptr1)
			{
				*mptr1-- = (intptr_t)palookup[globalpal].data() + (getpalookup((int)mulscalen<24>(krecipasm(m1),globvis),globalshade)<<8);
				m1 -= l;
			}
			
			while (nptr2 >= mptr2)
			{
				*mptr2++ = (intptr_t)palookup[globalpal].data() + (getpalookup((int)mulscalen<24>(krecipasm(m2),globvis),globalshade)<<8);
				m2 += l;
			}

			globalx3 = (g_pt2.x >> 10);
			globaly3 = (g_pt2.y >> 10);
			asm3 = mulscalen<16>(y2, globalzd) + (globalzx >> 6);
			slopevlin((void *)(ylookup[y2] + x + frameoffset), krecipasm((int)asm3 >> 3), nptr2, y2 - y1 + 1, g_pt1.x, g_pt1.y);

			if ((x & 15) == 0) faketimerhandler();
		}
		g_pt2.x += globalx;
		g_pt2.y += globaly;
		globalzx += globalz;
		shoffs += shinc;
	}
}


//
// parascan (internal)
//
void parascan(int dax1, int dax2, int sectnum, unsigned char dastat, int bunch)
{
	std::ignore = dax1;
	std::ignore = dax2;

	sectnum = thesector[bunchfirst[bunch]];
	auto* sec = &g_sector[sectnum];

	const int globalhorizbak{globalhoriz};

	if (parallaxyscale != 65536)
		globalhoriz = mulscalen<16>(globalhoriz - (ydimen >> 1), parallaxyscale) + (ydimen >> 1);

	globvis = globalpisibility;
	//globalorientation = 0L;
	if (sec->visibility != 0)
		globvis = mulscalen<4>(globvis, (int)((unsigned char)(sec->visibility + 16)));

	std::span<short> topptr{};
	std::span<short> botptr{};

	if (dastat == 0)
	{
		globalpal = sec->ceilingpal;
		globalpicnum = sec->ceilingpicnum;
		globalshade = (int)sec->ceilingshade;
		globalxpanning = (int)sec->ceilingxpanning;
		globalypanning = (int)sec->ceilingypanning;
		topptr = umost;
		botptr = uplc;
	}
	else
	{
		globalpal = sec->floorpal;
		globalpicnum = sec->floorpicnum;
		globalshade = (int)sec->floorshade;
		globalxpanning = (int)sec->floorxpanning;
		globalypanning = (int)sec->floorypanning;
		topptr = dplc;
		botptr = dmost;
	}

	if (palookup[globalpal].empty())
		globalpal = 0;

	if ((unsigned)globalpicnum >= (unsigned)MAXTILES)
		globalpicnum = 0;

	if (picanm[globalpicnum] & 192)
		globalpicnum += animateoffs(globalpicnum, (short)sectnum);

	globalshiftval = (picsiz[globalpicnum] >> 4);

	if (pow2long[globalshiftval] != tilesizy[globalpicnum])
		++globalshiftval;

	globalshiftval = 32 - globalshiftval;
	globalzd = (((tilesizy[globalpicnum] >> 1) + parallaxyoffs) << globalshiftval) + (globalypanning << 24);
	globalyscale = (8 << (globalshiftval - 19));
	//if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

	const int k = 11 - (picsiz[globalpicnum] & 15) - pskybits;
	int x{-1};
	for(int z = bunchfirst[bunch]; z >= 0; z = p2[z])
	{
		const int wallnum = thewall[z];
		const int nextsectnum = wall[wallnum].nextsector;

		int stat{0};
		if (dastat == 0)
			stat = g_sector[nextsectnum].ceilingstat;
		else
			stat = g_sector[nextsectnum].floorstat;

		if ((nextsectnum < 0) || (wall[wallnum].cstat&32) || ((stat & 1) == 0))
		{
			if (x == -1) x = xb1[z];

			if (parallaxtype == 0)
			{
				const int n = mulscalen<16>(xdimenrecip,viewingrange);
				for(int h = xb1[z]; h <= xb2[z]; ++h) {
					lplc[h] = (((mulscalen<23>(h - halfxdimen, n) + globalang) & 2047) >> k);
				}
			}
			else
			{
				for(int h = xb1[z]; h <= xb2[z]; ++h) {
					lplc[h] = (((static_cast<int>(radarang2[h]) + globalang) & 2047) >> k);
				}
			}

			if (parallaxtype == 2)
			{
				const int n = mulscalen<16>(xdimscale,viewingrange);
				for(int h = xb1[z]; h <= xb2[z]; ++h) {
					swplc[h] = mulscalen<14>(sintable[(static_cast<int>(radarang2[h]) + 512) & 2047] ,n);
				}
			}
			else
				clearbuf(&swplc[xb1[z]],xb2[z]-xb1[z]+1,mulscalen<16>(xdimscale,viewingrange));
		}
		else if (x >= 0)
		{
			const int l{globalpicnum};
			const int m = (picsiz[globalpicnum] & 15);
			globalpicnum = l+pskyoff[lplc[x] >> m];

			if (((lplc[x]^lplc[xb1[z]-1]) >> m) == 0)
				wallscan(x, xb1[z] - 1, topptr, botptr, swplc, lplc);
			else
			{
				int j{x};
				while (x < xb1[z])
				{
					const int n = l+pskyoff[lplc[x] >> m];
					if (n != globalpicnum)
					{
						wallscan(j, x - 1, topptr, botptr, swplc, lplc);
						j = x;
						globalpicnum = n;
					}
					x++;
				}
				if (j < x)
					wallscan(j, x - 1, topptr, botptr, swplc, lplc);
			}

			globalpicnum = l;
			x = -1;
		}
	}

	if (x >= 0)
	{
		const int l = globalpicnum;
		const int m = (picsiz[globalpicnum] & 15);
		globalpicnum = l + pskyoff[lplc[x] >> m];

		if (((lplc[x]^lplc[xb2[bunchlast[bunch]]]) >> m) == 0)
			wallscan(x,xb2[bunchlast[bunch]],topptr,botptr,swplc,lplc);
		else
		{
			int j{x};
			while (x <= xb2[bunchlast[bunch]])
			{
				const int n = l + pskyoff[lplc[x] >> m];
				if (n != globalpicnum)
				{
					wallscan(j,x-1,topptr,botptr,swplc,lplc);
					j = x;
					globalpicnum = n;
				}

				++x;
			}

			if (j <= x)
				wallscan(j, x, topptr, botptr, swplc, lplc);
		}
		globalpicnum = l;
	}

	globalhoriz = globalhorizbak;
}


//
// drawalls (internal)
//
void drawalls(int bunch)
{
	int z = bunchfirst[bunch];
	const int sectnum = thesector[z];
	const sectortype* sec = &g_sector[sectnum];

	unsigned char andwstat1{ 0xff };
	unsigned char andwstat2{ 0xff };

	for(; z >= 0; z = p2[z])  //uplc/dplc calculation
	{
		andwstat1 &= wallmost(uplc,z,sectnum,(char)0);
		andwstat2 &= wallmost(dplc,z,sectnum,(char)1);
	}

	if ((andwstat1 & 3) != 3)     //draw ceilings
	{
		if ((sec->ceilingstat&3) == 2)
			grouscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,0);
		else if ((sec->ceilingstat&1) == 0)
			ceilscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum);
		else
			parascan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,0,bunch);
	}
	if ((andwstat2&12) != 12)   //draw floors
	{
		if ((sec->floorstat&3) == 2)
			grouscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,1);
		else if ((sec->floorstat&1) == 0)
			florscan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum);
		else
			parascan(xb1[bunchfirst[bunch]],xb2[bunchlast[bunch]],sectnum,1,bunch);
	}

		//DRAW WALLS SECTION!
	for(z=bunchfirst[bunch];z>=0;z=p2[z])
	{
		const int x1 = xb1[z];
		const int x2 = xb2[z];

		if (umost[x2] >= dmost[x2]) {
			int x{x1};
			for(; x < x2; ++x) {
				if (umost[x] < dmost[x])
					break;
			}

			if (x >= x2) {
				smostwall[smostwallcnt] = z;
				smostwalltype[smostwallcnt] = 0;
				smostwallcnt++;
				continue;
			}
		}

		short wallnum = thewall[z];
		const auto* wal = &wall[wallnum];
		const short nextsectnum = wal->nextsector;
		const auto* nextsec = &g_sector[nextsectnum];

		int gotswall{0};

		const int startsmostwallcnt{smostwallcnt};
		const int startsmostcnt{smostcnt};

		if ((searchit == 2) && (searchx >= x1) && (searchx <= x2))
		{
			if (searchy <= uplc[searchx]) //ceiling
			{
				searchsector = sectnum;
				searchwall = wallnum;
				searchstat = 1;
				searchit = 1;
			}
			else if (searchy >= dplc[searchx]) //floor
			{
				searchsector = sectnum;
				searchwall = wallnum;
				searchstat = 2;
				searchit = 1;
			}
		}

		std::array<ceilfloorz, 5> cfz;
	
		if (nextsectnum >= 0)
		{
			cfz[0] = getzsofslope((short)sectnum,wal->pt.x, wal->pt.y);
			cfz[1] = getzsofslope((short)sectnum,wall[wal->point2].pt.x, wall[wal->point2].pt.y);
			cfz[2] = getzsofslope((short)nextsectnum,wal->pt.x, wal->pt.y);
			cfz[3] = getzsofslope((short)nextsectnum,wall[wal->point2].pt.x, wall[wal->point2].pt.y);
			cfz[4] = getzsofslope((short)nextsectnum,globalpos.x,globalpos.y);

			if ((wal->cstat&48) == 16)
				maskwall[maskwallcnt++] = z;

			if (((sec->ceilingstat&1) == 0) || ((nextsec->ceilingstat&1) == 0)) {
				if ((cfz[2].ceilz <= cfz[0].ceilz) && (cfz[3].ceilz <= cfz[1].ceilz)) {
					if (globparaceilclip) {
						for(int x{x1}; x <= x2; ++x) {
							if (uplc[x] > umost[x]) {
								if (umost[x] <= dmost[x]) {
									umost[x] = uplc[x];
									if (umost[x] > dmost[x])
										numhits--;
								}
							}
						}
					}
				}
				else
				{
					wallmost(dwall,z,nextsectnum,(char)0);
					
                    if ((cfz[2].ceilz > cfz[0].floorz) || (cfz[3].ceilz > cfz[1].floorz)) {
						const std::ranges::subrange dplcrange{std::next(dplc.begin(), x1), std::next(dplc.begin(), x2 + 1)};
						const std::ranges::subrange dwallstart{std::next(dwall.begin(), x1), std::next(dwall.begin(), x2 + 1)};
						ReplaceIfComp(dplcrange.begin(), dplcrange.end(), dwallstart.begin(), std::less<int>());
					}

					if ((searchit == 2) && (searchx >= x1) && (searchx <= x2)) {
						if (searchy <= dwall[searchx]) { // wall
							searchsector = sectnum;
							searchwall = wallnum;
							searchstat = 0;
							searchit = 1;
						}
					}

					globalorientation = (int)wal->cstat;
					globalpicnum = wal->picnum;
					if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
					globalxpanning = (int)wal->xpanning;
					globalypanning = (int)wal->ypanning;
					globalshiftval = (picsiz[globalpicnum]>>4);
					if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
					globalshiftval = 32-globalshiftval;
					if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
					globalshade = (int)wal->shade;
					globvis = globalvisibility;
					if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));
					globalpal = (int)wal->pal;
					if (palookup[globalpal].empty()) globalpal = 0;	// JBF: fixes crash
					globalyscale = (wal->yrepeat<<(globalshiftval-19));
					if ((globalorientation&4) == 0)
						globalzd = (((globalpos.z-nextsec->ceilingz)*globalyscale)<<8);
					else
						globalzd = (((globalpos.z-sec->ceilingz)*globalyscale)<<8);
					globalzd += (globalypanning<<24);
					if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

					if (gotswall == 0) {
						gotswall = 1;
						prepwall(z, wal);
					}

					wallscan(x1, x2, uplc, dwall, swall, lwall);

					if ((cfz[2].ceilz >= cfz[0].ceilz) && (cfz[3].ceilz >= cfz[1].ceilz)) {
						for(int x{x1}; x <= x2; ++x) {
							if (dwall[x] > umost[x]) {
								if (umost[x] <= dmost[x]) {
									umost[x] = dwall[x];
									if (umost[x] > dmost[x])
										--numhits;
								}
							}
						}
					}
					else
					{
						for(int x{x1}; x <= x2; ++x) {
							if (umost[x] <= dmost[x]) {
								const int i = std::max(uplc[x], dwall[x]);
								if (i > umost[x]) {
									umost[x] = i;
									if (umost[x] > dmost[x])
										--numhits;
								}
							}
						}
					}
				}
				if ((cfz[2].ceilz < cfz[0].ceilz) || (cfz[3].ceilz < cfz[1].ceilz) || (globalpos.z < cfz[4].ceilz))
				{
					const int i = x2 - x1 + 1;
					if (smostcnt + i < MAXYSAVES)
					{
						smoststart[smostwallcnt] = smostcnt;
						smostwall[smostwallcnt] = z;
						smostwalltype[smostwallcnt] = 1;   //1 for umost
						smostwallcnt++;
						copybufbyte(&umost[x1],&smost[smostcnt], i * sizeof(smost[0]));
						smostcnt += i;
					}
				}
			}

			if (((sec->floorstat&1) == 0) || ((nextsec->floorstat&1) == 0))
			{
				if ((cfz[2].floorz >= cfz[0].floorz) && (cfz[3].floorz >= cfz[1].floorz)) {
					if (globparaflorclip) {
						for(int x{x1}; x <= x2; ++x) {
							if (dplc[x] < dmost[x]) {
								if (umost[x] <= dmost[x]) {
									dmost[x] = dplc[x];
									if (umost[x] > dmost[x])
										--numhits;
								}
							}
						}
					}
				}
				else {
					wallmost(uwall,z,nextsectnum,(char)1);

					if ((cfz[2].floorz < cfz[0].ceilz) || (cfz[3].floorz < cfz[1].ceilz)) {
						for(int i{x1}; i <= x2; ++i) {
							if (uwall[i] < uplc[i])
								uwall[i] = uplc[i];
						}
					}

					if ((searchit == 2) && (searchx >= x1) && (searchx <= x2)) {
						if (searchy >= uwall[searchx]) { // wall
							searchsector = sectnum;
							searchwall = wallnum;
							
							if ((wal->cstat&2) > 0)
								searchwall = wal->nextwall;
							
							searchstat = 0;
							searchit = 1;
						}
					}

					if ((wal->cstat & 2) > 0) {
						wallnum = wal->nextwall;
						wal = &wall[wallnum];
						globalorientation = (int)wal->cstat;
						globalpicnum = wal->picnum;

						if ((unsigned)globalpicnum >= (unsigned)MAXTILES)
							globalpicnum = 0;

						globalxpanning = (int)wal->xpanning;
						globalypanning = (int)wal->ypanning;
					
						if (picanm[globalpicnum]&192)
							globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
					
						globalshade = (int)wal->shade;
						globalpal = (int)wal->pal;
						wallnum = thewall[z];
						wal = &wall[wallnum];
					}
					else {
						globalorientation = (int)wal->cstat;
						globalpicnum = wal->picnum;
						
						if ((unsigned)globalpicnum >= (unsigned)MAXTILES)
							globalpicnum = 0;
						
						globalxpanning = (int)wal->xpanning;
						globalypanning = (int)wal->ypanning;
						
						if (picanm[globalpicnum]&192)
							globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
						
						globalshade = (int)wal->shade;
						globalpal = (int)wal->pal;
					}

					if (palookup[globalpal].empty()) 	// JBF: fixes crash
						globalpal = 0;

					globvis = globalvisibility;
					
					if (sec->visibility != 0)
						globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));
					
					globalshiftval = (picsiz[globalpicnum]>>4);
					
					if (pow2long[globalshiftval] != tilesizy[globalpicnum])
						++globalshiftval;

					globalshiftval = 32-globalshiftval;
					globalyscale = (wal->yrepeat<<(globalshiftval-19));

					if ((globalorientation&4) == 0)
						globalzd = (((globalpos.z-nextsec->floorz)*globalyscale)<<8);
					else
						globalzd = (((globalpos.z-sec->ceilingz)*globalyscale)<<8);

					globalzd += (globalypanning << 24);

					if (globalorientation&256) {
						globalyscale = -globalyscale;
						globalzd = -globalzd;
					}

					if (gotswall == 0) {
						gotswall = 1;
						prepwall(z, wal);
					}

					wallscan(x1,x2,uwall,dplc,swall,lwall);

					if ((cfz[2].floorz <= cfz[0].floorz) && (cfz[3].floorz <= cfz[1].floorz)) {
						for(int x{x1}; x <= x2; ++x) {
							if (uwall[x] < dmost[x]) {
								if (umost[x] <= dmost[x]) {
									dmost[x] = uwall[x];
									if (umost[x] > dmost[x])
										--numhits;
								}
							}
						}
					}
					else
					{
						for(int x{x1}; x <= x2; ++x) {
							if (umost[x] <= dmost[x]) {
								const int i = std::min(dplc[x], uwall[x]);
								if (i < dmost[x]) {
									dmost[x] = i;
									if (umost[x] > dmost[x])
										--numhits;
								}
							}
						}
					}
				}

				if ((cfz[2].floorz > cfz[0].floorz) || (cfz[3].floorz > cfz[1].floorz) || (globalpos.z > cfz[4].floorz)) {
					const int i = x2 - x1 + 1;
					
					if (smostcnt + i < MAXYSAVES) {
						smoststart[smostwallcnt] = smostcnt;
						smostwall[smostwallcnt] = z;
						smostwalltype[smostwallcnt] = 2;   //2 for dmost
						smostwallcnt++;
						copybufbyte(&dmost[x1],&smost[smostcnt],i*sizeof(smost[0]));
						smostcnt += i;
					}
				}
			}

			if (numhits < 0)
				return;
			
			if ((!(wal->cstat&32)) && ((gotsector[nextsectnum>>3] & pow2char[nextsectnum & 7]) == 0)) {
				if (umost[x2] < dmost[x2])
					scansector(nextsectnum);
				else {
					int x{x1};
					
					for(; x < x2; ++x) {
						if (umost[x] < dmost[x]) {
							scansector(nextsectnum);
							break;
						}
					}
						//If can't see sector beyond, then cancel smost array and just
						//store wall!
					if (x == x2) {
						smostwallcnt = startsmostwallcnt;
						smostcnt = startsmostcnt;
						smostwall[smostwallcnt] = z;
						smostwalltype[smostwallcnt] = 0;
						smostwallcnt++;
					}
				}
			}
		}

		if ((nextsectnum < 0) || (wal->cstat&32)) {  //White/1-way wall
			globalorientation = (int)wal->cstat;
			if (nextsectnum < 0) globalpicnum = wal->picnum;
								  else globalpicnum = wal->overpicnum;
			if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
			globalxpanning = (int)wal->xpanning;
			globalypanning = (int)wal->ypanning;
			if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)wallnum+16384);
			globalshade = (int)wal->shade;
			globvis = globalvisibility;
			if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));
			globalpal = (int)wal->pal;

			if (palookup[globalpal].empty()) 	// JBF: fixes crash
				globalpal = 0;
			
			globalshiftval = (picsiz[globalpicnum]>>4);
			
			if (pow2long[globalshiftval] != tilesizy[globalpicnum])
				++globalshiftval;

			globalshiftval = 32-globalshiftval;
			globalyscale = (wal->yrepeat<<(globalshiftval-19));
			if (nextsectnum >= 0) {
				if ((globalorientation&4) == 0) globalzd = globalpos.z-nextsec->ceilingz;
													else globalzd = globalpos.z-sec->ceilingz;
			}
			else {
				if ((globalorientation&4) == 0)
					globalzd = globalpos.z-sec->ceilingz;
				else
					globalzd = globalpos.z-sec->floorz;
			}

			globalzd = ((globalzd*globalyscale)<<8) + (globalypanning<<24);
			
			if (globalorientation&256) {
				globalyscale = -globalyscale;
				globalzd = -globalzd;
			}

			if (gotswall == 0) {
				gotswall = 1;
				prepwall(z, wal);
			}

			wallscan(x1, x2, uplc, dplc, swall, lwall);

			for(int x{x1}; x <= x2; ++x) {
				if (umost[x] <= dmost[x]) {
					umost[x] = 1;
					dmost[x] = 0;
					--numhits;
				}
			}

			smostwall[smostwallcnt] = z;
			smostwalltype[smostwallcnt] = 0;
			++smostwallcnt;

			if ((searchit == 2) && (searchx >= x1) && (searchx <= x2)) {
				searchit = 1;
				searchsector = sectnum;
				searchwall = wallnum;
				
				if (nextsectnum < 0)
					searchstat = 0;
				else
					searchstat = 4;
			}
		}
	}
}


//
// drawvox
//
void drawvox(int dasprx, int daspry, int dasprz, int dasprang,
		  int daxscale, int dayscale, unsigned char daindex,
		  signed char dashade, unsigned char dapal, std::span<const int> daumost, std::span<const int> dadmost)
{
	int i;
	int j;
	int k;
	int x;
	int y;
	int syoff;
	int ggxstart;
	int ggystart;
	int nxoff;
	int backx;
	int backy;
	int gxinc;
	int gyinc;
	int daxsiz;
	int daysiz;
	int dazsiz;
	int daxpivot;
	int daypivot;
	int dazpivot;
	int daxscalerecip;
	int dayscalerecip;
	int cnt;
	int gxstart;
	int gystart;
	int odayscale;
	int l1;
	int l2;
	int xyvoxoffs;
	int* longptr;
	intptr_t slabxoffs;
	int lx;
	int rx;
	int nx;
	int ny;
	int x1{0};
	int y1{0};
	int z1;
	int x2{0};
	int y2{0};
	int z2;
	int yplc;
	int yinc{0};
	int yoff;
	int xs{0};
    int ys{0};
    int xe{0};
	int ye;
    int xi{0};
    int yi{0};
	int cbackx;
	int cbacky;
	int dagxinc;
	int dagyinc;
	short* shortptr;
	unsigned char* voxptr;
	unsigned char* voxend;
	unsigned char* davoxptr;
	unsigned char oand;
	unsigned char oand16;
	unsigned char oand32;

	std::ignore = dazsiz;

	int cosang = sintable[(globalang+512)&2047];
	int sinang = sintable[globalang&2047];
	int sprcosang = sintable[(dasprang+512)&2047];
	int sprsinang = sintable[dasprang&2047];

	i = std::abs(dmulscalen<6>(dasprx-globalpos.x,cosang,daspry-globalpos.y,sinang));
	j = (int)(getpalookup((int)mulscalen<21>(globvis,i),(int)dashade)<<8);
	setupdrawslab(ylookup[1], palookup[dapal].data() + j);
	j = 1310720;
	j *= std::min(daxscale, dayscale);
	j >>= 6;  //New hacks (for sized-down voxels)
	for(k=0;k<MAXVOXMIPS;k++)
	{
		if (i < j) { i = k; break; }
		j <<= 1;
	}
	if (k >= MAXVOXMIPS) i = MAXVOXMIPS-1;

	if (novoxmips)
		i = 0;

	davoxptr = (unsigned char *)voxoff[daindex][i];
	if (!davoxptr && i > 0) { davoxptr = (unsigned char *)voxoff[daindex][0]; i = 0; }
	if (!davoxptr) return;

	if (voxscale[daindex] == 65536)
		{ daxscale <<= (i+8); dayscale <<= (i+8); }
	else
	{
		daxscale = mulscalen<8>(daxscale<<i,voxscale[daindex]);
		dayscale = mulscalen<8>(dayscale<<i,voxscale[daindex]);
	}

	odayscale = dayscale;
	daxscale = mulscalen<16>(daxscale,xyaspect);
	daxscale = scale(daxscale,xdimenscale,xdimen<<8);
	dayscale = scale(dayscale,mulscalen<16>(xdimenscale,viewingrangerecip),xdimen<<8);

	daxscalerecip = (1<<30)/daxscale;
	dayscalerecip = (1<<30)/dayscale;

	longptr = (int *)davoxptr;
	daxsiz = longptr[0];
	daysiz = longptr[1];
	dazsiz = longptr[2];
	daxpivot = longptr[3];
	daypivot = longptr[4];
	dazpivot = longptr[5];
	davoxptr += (6<<2);

	x = mulscalen<16>(globalpos.x-dasprx,daxscalerecip);
	y = mulscalen<16>(globalpos.y-daspry,daxscalerecip);
	backx = ((dmulscalen<10>(x,sprcosang,y,sprsinang)+daxpivot)>>8);
	backy = ((dmulscalen<10>(y,sprcosang,x,-sprsinang)+daypivot)>>8);
	cbackx = std::min(std::max(backx, 0), daxsiz - 1);
	cbacky = std::min(std::max(backy, 0), daysiz - 1);

	sprcosang = mulscalen<14>(daxscale,sprcosang);
	sprsinang = mulscalen<14>(daxscale,sprsinang);

	x = (dasprx-globalpos.x) - dmulscalen<18>(daxpivot,sprcosang,daypivot,-sprsinang);
	y = (daspry-globalpos.y) - dmulscalen<18>(daypivot,sprcosang,daxpivot,sprsinang);

	cosang = mulscalen<16>(cosang,dayscalerecip);
	sinang = mulscalen<16>(sinang,dayscalerecip);

	gxstart = y*cosang - x*sinang;
	gystart = x*cosang + y*sinang;
	gxinc = dmulscalen<10>(sprsinang,cosang,sprcosang,-sinang);
	gyinc = dmulscalen<10>(sprcosang,cosang,sprsinang,sinang);
	
	j = std::max(daxsiz, daysiz);

	const auto gg_gen = [](auto cnt, auto ginc){
		std::array<int, MAXXSIZ + 1> gg_arr{};
		std::ranges::generate_n(gg_arr.begin(), cnt + 1, [ginc, n = 0]() mutable {
			return (n++) * ginc;
		});

		return gg_arr;
	};

	const std::array<int, MAXXSIZ + 1> ggxinc = gg_gen(j, gxinc);
	const std::array<int, MAXYSIZ + 1> ggyinc = gg_gen(j, gyinc);


	if ((std::abs(globalpos.z-dasprz)>>10) >= std::abs(odayscale)) return;
	syoff = divscalen<21>(globalpos.z-dasprz,odayscale) + (dazpivot<<7);
	yoff = ((std::abs(gxinc)+std::abs(gyinc))>>1);
	longptr = (int *)davoxptr;
	xyvoxoffs = ((daxsiz+1)<<2);

	for(cnt=0;cnt<8;cnt++)
	{
		switch(cnt)
		{
			case 0: xs = 0;        ys = 0;        xi = 1;  yi = 1;  break;
			case 1: xs = daxsiz-1; ys = 0;        xi = -1; yi = 1;  break;
			case 2: xs = 0;        ys = daysiz-1; xi = 1;  yi = -1; break;
			case 3: xs = daxsiz-1; ys = daysiz-1; xi = -1; yi = -1; break;
			case 4: xs = 0;        ys = cbacky;   xi = 1;  yi = 2;  break;
			case 5: xs = daxsiz-1; ys = cbacky;   xi = -1; yi = 2;  break;
			case 6: xs = cbackx;   ys = 0;        xi = 2;  yi = 1;  break;
			case 7: xs = cbackx;   ys = daysiz-1; xi = 2;  yi = -1; break;
		}
		xe = cbackx; ye = cbacky;
		if (cnt < 4)
		{
			if ((xi < 0) && (xe >= xs)) continue;
			if ((xi > 0) && (xe <= xs)) continue;
			if ((yi < 0) && (ye >= ys)) continue;
			if ((yi > 0) && (ye <= ys)) continue;
		}
		else
		{
			if ((xi < 0) && (xe > xs)) continue;
			if ((xi > 0) && (xe < xs)) continue;
			if ((yi < 0) && (ye > ys)) continue;
			if ((yi > 0) && (ye < ys)) continue;
			xe += xi; ye += yi;
		}

		i = ksgn(ys-backy)+ksgn(xs-backx)*3+4;
		switch(i)
		{
			case 6: case 7: x1 = 0; y1 = 0; break;
			case 8: case 5: x1 = gxinc; y1 = gyinc; break;
			case 0: case 3: x1 = gyinc; y1 = -gxinc; break;
			case 2: case 1: x1 = gxinc+gyinc; y1 = gyinc-gxinc; break;
		}
		switch(i)
		{
			case 2: case 5: x2 = 0; y2 = 0; break;
			case 0: case 1: x2 = gxinc; y2 = gyinc; break;
			case 8: case 7: x2 = gyinc; y2 = -gxinc; break;
			case 6: case 3: x2 = gxinc+gyinc; y2 = gyinc-gxinc; break;
		}
		oand = pow2char[(xs < backx) + 0] + pow2char[(ys < backy) + 2];
		oand16 = oand+16;
		oand32 = oand+32;

		if (yi > 0) { dagxinc = gxinc; dagyinc = mulscalen<16>(gyinc,viewingrangerecip); }
		else { dagxinc = -gxinc; dagyinc = -mulscalen<16>(gyinc,viewingrangerecip); }

			//Fix for non 90 degree viewing ranges
		nxoff = mulscalen<16>(x2-x1,viewingrangerecip);
		x1 = mulscalen<16>(x1,viewingrangerecip);

		ggxstart = gxstart + ggyinc[ys];
		ggystart = gystart - ggxinc[ys];

		for(x=xs;x!=xe;x+=xi)
		{
			slabxoffs = (intptr_t)&davoxptr[longptr[x]];
			shortptr = (short *)&davoxptr[((x*(daysiz+1))<<1)+xyvoxoffs];

			nx = mulscalen<16>(ggxstart+ggxinc[x],viewingrangerecip)+x1;
			ny = ggystart + ggyinc[x];
			for(y=ys;y!=ye;y+=yi,nx+=dagyinc,ny-=dagxinc)
			{
				if ((ny <= nytooclose) || (ny >= nytoofar)) continue;
				voxptr = (unsigned char *)(shortptr[y] + slabxoffs);
				voxend = (unsigned char *)(shortptr[y+1] + slabxoffs);
				if (voxptr == voxend) continue;

				lx = mulscalen<32>(nx >> 3, distrecip[(ny + y1) >> 14]) + halfxdimen;
				if (lx < 0) lx = 0;
				rx = mulscalen<32>((nx + nxoff) >> 3, distrecip[(ny + y2) >> 14]) + halfxdimen;
				if (rx > xdimen) rx = xdimen;
				if (rx <= lx) continue;
				rx -= lx;

				l1 = distrecip[(ny - yoff) >> 14];
				l2 = distrecip[(ny + yoff) >> 14];
				for(;voxptr<voxend;voxptr+=voxptr[1]+3)
				{
					j = (voxptr[0]<<15)-syoff;
					if (j < 0)
					{
						k = j+(voxptr[1]<<15);
						if (k < 0)
						{
							if ((voxptr[2]&oand32) == 0) continue;
							z2 = mulscalen<32>(l2,k) + globalhoriz;     //Below slab
						}
						else
						{
							if ((voxptr[2]&oand) == 0) continue;    //Middle of slab
							z2 = mulscalen<32>(l1,k) + globalhoriz;
						}
						z1 = mulscalen<32>(l1,j) + globalhoriz;
					}
					else
					{
						if ((voxptr[2]&oand16) == 0) continue;
						z1 = mulscalen<32>(l2,j) + globalhoriz;        //Above slab
						z2 = mulscalen<32>(l1,j+(voxptr[1]<<15)) + globalhoriz;
					}

					if (voxptr[1] == 1)
					{
						yplc = 0; yinc = 0;
						if (z1 < daumost[lx]) z1 = daumost[lx];
					}
					else
					{
						if (z2-z1 >= 1024) yinc = divscalen<16>(voxptr[1],z2-z1);
						else if (z2 > z1) yinc = (lowrecip[z2 - z1] * voxptr[1] >> 8);
						if (z1 < daumost[lx]) {
							yplc = yinc * (daumost[lx]-z1);
							z1 = daumost[lx];
						}
						else
							yplc = 0;
					}
					if (z2 > dadmost[lx]) z2 = dadmost[lx];
					z2 -= z1; if (z2 <= 0) continue;

					drawslab(rx,yplc,z2,yinc,&voxptr[3],(void *)(ylookup[z1]+lx+frameoffset));
				}
			}
		}
	}
}


//
// drawsprite (internal)
//
void drawsprite(int snum)
{
	int startum;
	int startdm;
	int sectnum;
	int siz;
	int xsiz;
	int ysiz;
	int xspan;
	int yspan;
	int x1;
	int y1;
	int x2;
	int y2;
	int lx;
	int rx;
	int dalx2;
	int darx2;
	int i;
	int j;
	int k;
	int x;
	int linum;
	int linuminc;
	int yinc;
	int z;
	int z1;
	int z2;
	int xp1;
	int yp1;
	int xp2;
	int yp2;
	int xv;
	int yv;
	int top;
	int topinc;
	int bot;
	int botinc;
	int hplc;
	int hinc;
	int cosang;
	int sinang;
	int dax;
	int day;
	int lpoint;
	int lmax;
	int rpoint;
	int rmax;
	int dax1;
	int dax2;
	int y;
	int npoints;
	int npoints2;
	int zz;
	int t;
	int zsgn;
	int zzsgn;
	int* longptr;
	int vtilenum{ 0 };
	unsigned char swapped;
	unsigned char daclip;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	if (rendmode != rendmode_t::Classic) { polymost_drawsprite(snum); return; }
#endif
	//============================================================================= //POLYMOST ENDS

	spritetype* tspr = tspriteptr[snum];

	const int xb = spritesx[snum];
	const int yp = spritesy[snum];
	int tilenum = tspr->picnum;
	const int spritenum = tspr->owner;
	int cstat = tspr->cstat;

	if ((cstat & 48) == 48) {
		vtilenum = tilenum;	// if the game wants voxels, it gets voxels
	}
	else if ((cstat&48)!=48 && usevoxels && (tiletovox[tilenum] != -1)
#if USE_POLYMOST && USE_OPENGL
		 && (!(spriteext[tspr->owner].flags&SPREXT_NOTMD))
#endif
	   ) {
		vtilenum = tiletovox[tilenum];
		cstat |= 48;
	}

	if ((cstat & 48) != 48)
	{
		if (picanm[tilenum] & 192) {
			tilenum += animateoffs(tilenum, spritenum + 32768);
		}

		if ((tilesizx[tilenum] <= 0) || (tilesizy[tilenum] <= 0) || (spritenum < 0)) {
			return;
	}
	}
	if ((tspr->xrepeat <= 0) || (tspr->yrepeat <= 0)) return;

	sectnum = tspr->sectnum;
	const sectortype* sec = &g_sector[sectnum];
	globalpal = tspr->pal;
	
	if (palookup[globalpal].empty()) {
		globalpal = 0;	// JBF: fixes null-pointer crash
	}

	globalshade = tspr->shade;
	if (cstat&2)
	{
		if (cstat&512) settransreverse(); else settransnormal();
	}

	int xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)tspr->xoffset);
	int yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)tspr->yoffset);

	if ((cstat&48) == 0)
	{
		if (yp <= (4<<8)) return;

		siz = divscalen<19>(xdimenscale,yp);

		xv = mulscalen<16>(((int)tspr->xrepeat)<<16,xyaspect);

		xspan = tilesizx[tilenum];
		yspan = tilesizy[tilenum];
		xsiz = mulscalen<30>(siz,xv*xspan);
		ysiz = mulscalen<14>(siz,tspr->yrepeat*yspan);

		if (((tilesizx[tilenum]>>11) >= xsiz) || (yspan >= (ysiz>>1)))
			return;  //Watch out for divscale overflow

		x1 = xb-(xsiz>>1);
		if (xspan&1) x1 += mulscalen<31>(siz,xv);  //Odd xspans
		i = mulscalen<30>(siz,xv*xoff);
		if ((cstat&4) == 0) x1 -= i; else x1 += i;

		y1 = mulscalen<16>(tspr->z-globalpos.z,siz);
		y1 -= mulscalen<14>(siz,tspr->yrepeat*yoff);
		y1 += (globalhoriz<<8)-ysiz;
		if (cstat&128)
		{
			y1 += (ysiz>>1);
			if (yspan&1) y1 += mulscalen<15>(siz,tspr->yrepeat);  //Odd yspans
		}

		x2 = x1+xsiz-1;
		y2 = y1+ysiz-1;
		if ((y1|255) >= (y2|255)) return;

		lx = (x1>>8)+1; if (lx < 0) lx = 0;
		rx = (x2>>8); if (rx >= xdimen) rx = xdimen-1;
		if (lx > rx) return;

		yinc = divscalen<32>(yspan,ysiz);

		if ((sec->ceilingstat&3) == 0)
			startum = globalhoriz+mulscalen<24>(siz,sec->ceilingz-globalpos.z)-1;
		else
			startum = 0;
		if ((sec->floorstat&3) == 0)
			startdm = globalhoriz+mulscalen<24>(siz,sec->floorz-globalpos.z)+1;
		else
			startdm = 0x7fffffff;
		if ((y1>>8) > startum) startum = (y1>>8);
		if ((y2>>8) < startdm) startdm = (y2>>8);

		if (startum < -32768) startum = -32768;
		if (startdm > 32767) startdm = 32767;
		if (startum >= startdm) return;

		if ((cstat&4) == 0)
		{
			linuminc = divscalen<24>(xspan,xsiz);
			linum = mulscalen<8>((lx<<8)-x1,linuminc);
		}
		else
		{
			linuminc = -divscalen<24>(xspan,xsiz);
			linum = mulscalen<8>((lx<<8)-x2,linuminc);
		}
		if ((cstat&8) > 0)
		{
			yinc = -yinc;
			std::swap(y1, y2);
		}

		for(x=lx;x<=rx;x++)
		{
			uwall[x] = std::max(static_cast<int>(startumost[x+windowx1]) - windowy1, startum);
			dwall[x] = std::min(static_cast<int>(startdmost[x + windowx1]) - windowy1, startdm);
		}
		daclip = 0;
		for(i=smostwallcnt-1;i>=0;i--)
		{
			if (smostwalltype[i]&daclip) continue;
			j = smostwall[i];
			if ((xb1[j] > rx) || (xb2[j] < lx)) continue;
			if ((yp <= yb1[j]) && (yp <= yb2[j])) continue;
			if (spritewallfront(tspr, (int) thewall[j]) && ((yp <= yb1[j]) || (yp <= yb2[j]))) continue;

			dalx2 = std::max(xb1[j], lx);
			darx2 = std::min(xb2[j], rx);

			switch(smostwalltype[i])
			{
				case 0:
					if (dalx2 <= darx2)
					{
						if ((dalx2 == lx) && (darx2 == rx))
							return;

						std::fill_n(std::next(dwall.begin(), dalx2), darx2 + 1, 0);
					}
					break;
				case 1:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++) {
						if (smost[k+x] > uwall[x])
							uwall[x] = smost[k+x];
					}

					if ((dalx2 == lx) && (darx2 == rx))
						daclip |= 1;
					break;
				case 2:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++) {
						if (smost[k+x] < dwall[x])
							dwall[x] = smost[k+x];
					}

					if ((dalx2 == lx) && (darx2 == rx))
						daclip |= 2;
					break;
			}
		}

		if (uwall[rx] >= dwall[rx])
		{
			for(x=lx;x<rx;x++)
				if (uwall[x] < dwall[x]) break;
			if (x == rx) return;
		}

			//sprite
		if ((searchit >= 1) && (searchx >= lx) && (searchx <= rx))
			if ((searchy >= uwall[searchx]) && (searchy < dwall[searchx]))
			{
				searchsector = sectnum; searchwall = spritenum;
				searchstat = 3; searchit = 1;
			}

		z2 = tspr->z - ((yoff*tspr->yrepeat)<<2);
		if (cstat&128)
		{
			z2 += ((yspan*tspr->yrepeat)<<1);
			if (yspan&1) z2 += (tspr->yrepeat<<1);        //Odd yspans
		}
		z1 = z2 - ((yspan*tspr->yrepeat)<<2);

		globalorientation = 0;
		globalpicnum = tilenum;
		if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
		globalxpanning = 0L;
		globalypanning = 0L;
		globvis = globalvisibility;
		if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));
		globalshiftval = (picsiz[globalpicnum]>>4);
		if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
		globalshiftval = 32-globalshiftval;
		globalyscale = divscale(512,tspr->yrepeat,globalshiftval-19);
		globalzd = (((globalpos.z-z1)*globalyscale)<<8);
		if ((cstat&8) > 0)
		{
			globalyscale = -globalyscale;
			globalzd = (((globalpos.z-z2)*globalyscale)<<8);
		}

		qinterpolatedown16(&lwall[lx],rx-lx+1,linum,linuminc);
		clearbuf(&swall[lx],rx-lx+1,mulscalen<19>(yp,xdimscale));

		if ((cstat&2) == 0)
			maskwallscan(lx,rx,uwall,dwall,swall,lwall);
		else
			transmaskwallscan(lx,rx);
	}
	else if ((cstat&48) == 16)
	{
		if ((cstat&4) > 0) xoff = -xoff;
		if ((cstat&8) > 0) yoff = -yoff;

		xspan = tilesizx[tilenum]; yspan = tilesizy[tilenum];
		xv = tspr->xrepeat*sintable[(tspr->ang+2560+1536)&2047];
		yv = tspr->xrepeat*sintable[(tspr->ang+2048+1536)&2047];
		i = (xspan>>1)+xoff;
		x1 = tspr->x-globalpos.x-mulscalen<16>(xv,i); x2 = x1+mulscalen<16>(xv,xspan);
		y1 = tspr->y-globalpos.y-mulscalen<16>(yv,i); y2 = y1+mulscalen<16>(yv,xspan);

		yp1 = dmulscalen<6>(x1,cosviewingrangeglobalang,y1,sinviewingrangeglobalang);
		yp2 = dmulscalen<6>(x2,cosviewingrangeglobalang,y2,sinviewingrangeglobalang);
		if ((yp1 <= 0) && (yp2 <= 0)) return;
		xp1 = dmulscalen<6>(y1,cosglobalang,-x1,singlobalang);
		xp2 = dmulscalen<6>(y2,cosglobalang,-x2,singlobalang);

		x1 += globalpos.x; y1 += globalpos.y;
		x2 += globalpos.x; y2 += globalpos.y;

		swapped = 0;
		if (dmulscalen<32>(xp1,yp2,-xp2,yp1) >= 0)  //If wall's NOT facing you
		{
			if ((cstat&64) != 0)
				return;
			
			std::swap(xp1, xp2);
			std::swap(yp1, yp2);
			std::swap(x1, x2);
			std::swap(y1, y2);
			swapped = 1;
		}

		if (xp1 >= -yp1)
		{
			if (xp1 > yp1)
				return;

			if (yp1 == 0)
				return;

			xb1.back() = halfxdimen + scale(xp1,halfxdimen,yp1);
			if (xp1 >= 0) xb1.back()++;   //Fix for SIGNED divide
			if (xb1.back() >= xdimen) xb1.back() = xdimen-1;
			yb1.back() = yp1;
		}
		else
		{
			if (xp2 < -yp2)
				return;
			xb1.back() = 0;
			i = yp1-yp2+xp1-xp2;
			if (i == 0) return;
			yb1.back() = yp1 + scale(yp2-yp1,xp1+yp1,i);
		}

		if (xp2 <= yp2)
		{
			if (xp2 < -yp2)
				return;

			if (yp2 == 0)
				return;

			xb2.back() = halfxdimen + scale(xp2,halfxdimen,yp2) - 1;
			if (xp2 >= 0) xb2.back()++;   //Fix for SIGNED divide
			if (xb2.back() >= xdimen) xb2.back() = xdimen-1;
			yb2.back() = yp2;
		}
		else
		{
			if (xp1 > yp1)
				return;

			xb2.back() = xdimen-1;
			i = xp2-xp1+yp1-yp2;
			if (i == 0)
				return;

			yb2.back() = yp1 + scale(yp2-yp1,yp1-xp1,i);
		}

		if ((yb1.back() < 256) || (yb2.back() < 256) || (xb1.back() > xb2.back()))
			return;

		topinc = -mulscalen<10>(yp1,xspan);
		top = (((mulscalen<10>(xp1,xdimen) - mulscalen<9>(xb1.back()-halfxdimen,yp1))*xspan)>>3);
		botinc = ((yp2-yp1)>>8);
		bot = mulscalen<11>(xp1-xp2,xdimen) + mulscalen<2>(xb1.back()-halfxdimen,botinc);

		j = xb2.back()+3;
		z = mulscalen<20>(top,krecipasm(bot));
		lwall[xb1.back()] = (z>>8);
		for(x=xb1.back()+4;x<=j;x+=4)
		{
			top += topinc;
			bot += botinc;
			zz = z;
			z = mulscalen<20>(top,krecipasm(bot));
			lwall[x] = (z>>8);
			i = ((z+zz)>>1);
			lwall[x-2] = (i>>8);
			lwall[x-3] = ((i+zz)>>9);
			lwall[x-1] = ((i+z)>>9);
		}

		if (lwall[xb1.back()] < 0) lwall[xb1.back()] = 0;
		if (lwall[xb2.back()] >= xspan) lwall[xb2.back()] = xspan-1;

		if ((swapped^((cstat&4)>0)) > 0)
		{
			j = xspan-1;
			for(x=xb1.back();x<=xb2.back();x++)
				lwall[x] = j-lwall[x];
		}

		rx1.back() = xp1;
		ry1.back() = yp1;
		rx2.back() = xp2;
		ry2.back() = yp2;

		hplc = divscalen<19>(xdimenscale, yb1.back());
		hinc = divscalen<19>(xdimenscale, yb2.back());
		hinc = (hinc-hplc) / (xb2.back() - xb1.back() + 1);

		z2 = tspr->z - ((yoff*tspr->yrepeat)<<2);
		if (cstat&128)
		{
			z2 += ((yspan*tspr->yrepeat)<<1);
			if (yspan&1) z2 += (tspr->yrepeat<<1);        //Odd yspans
		}
		z1 = z2 - ((yspan*tspr->yrepeat)<<2);

		globalorientation = 0;
		globalpicnum = tilenum;
		if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
		globalxpanning = 0L;
		globalypanning = 0L;
		globvis = globalvisibility;
		if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));
		globalshiftval = (picsiz[globalpicnum]>>4);
		if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
		globalshiftval = 32-globalshiftval;
		globalyscale = divscale(512,tspr->yrepeat,globalshiftval-19);
		globalzd = (((globalpos.z-z1)*globalyscale)<<8);
		if ((cstat&8) > 0)
		{
			globalyscale = -globalyscale;
			globalzd = (((globalpos.z-z2)*globalyscale)<<8);
		}

		if (((sec->ceilingstat&1) == 0) && (z1 < sec->ceilingz))
			z1 = sec->ceilingz;
		if (((sec->floorstat&1) == 0) && (z2 > sec->floorz))
			z2 = sec->floorz;

		owallmost(uwall,(int)(MAXWALLSB-1),z1-globalpos.z);
		owallmost(dwall,(int)(MAXWALLSB-1),z2-globalpos.z);
		for(i=xb1.back(); i <= xb2.back() ;i++)
			{ swall[i] = (krecipasm(hplc)<<2); hplc += hinc; }

		for(i=smostwallcnt-1;i>=0;i--)
		{
			j = smostwall[i];

			if ((xb1[j] > xb2.back()) || (xb2[j] < xb1.back())) continue;

			dalx2 = xb1[j]; darx2 = xb2[j];
			if (std::max(yb1.back(), yb2.back()) > std::min(yb1[j], yb2[j]))
			{
				if (std::min(yb1.back(), yb2.back()) > std::max(yb1[j], yb2[j]))
				{
					x = 0x80000000;
				}
				else
				{
					x = thewall[j];
					xp1 = wall[x].pt.x;
					yp1 = wall[x].pt.y;
					x = wall[x].point2;
					xp2 = wall[x].pt.x;
					yp2 = wall[x].pt.y;

					z1 = (xp2-xp1)*(y1-yp1) - (yp2-yp1)*(x1-xp1);
					z2 = (xp2-xp1)*(y2-yp1) - (yp2-yp1)*(x2-xp1);

					if ((z1^z2) >= 0)
						x = (z1+z2);
					else
					{
						z1 = (x2-x1)*(yp1-y1) - (y2-y1)*(xp1-x1);
						z2 = (x2-x1)*(yp2-y1) - (y2-y1)*(xp2-x1);

						if ((z1^z2) >= 0)
							x = -(z1+z2);
						else
						{
							if ((xp2-xp1)*(tspr->y-yp1) == (tspr->x-xp1)*(yp2-yp1))
							{
								if (wall[thewall[j]].nextsector == tspr->sectnum)
									x = 0x80000000;
								else
									x = 0x7fffffff;
							}
							else
							{     //INTERSECTION!
								x = (xp1-globalpos.x) + scale(xp2-xp1,z1,z1-z2);
								y = (yp1-globalpos.y) + scale(yp2-yp1,z1,z1-z2);

								yp1 = dmulscalen<14>(x, cosglobalang, y, singlobalang);

								if (yp1 > 0)
								{
									xp1 = dmulscalen<14>(y,cosglobalang,-x,singlobalang);

									x = halfxdimen + scale(xp1,halfxdimen,yp1);
									if (xp1 >= 0) x++;   //Fix for SIGNED divide

									if (z1 < 0)
										{ if (dalx2 < x) dalx2 = x; }
									else
										{ if (darx2 > x) darx2 = x; }
									x = 0x80000001;
								}
								else
									x = 0x7fffffff;
							}
						}
					}
				}
				if (x < 0)
				{
					if (dalx2 < xb1.back())
						dalx2 = xb1.back();

					if (darx2 > xb2.back())
						darx2 = xb2.back();

					switch(smostwalltype[i])
					{
						case 0:
							if (dalx2 <= darx2)
							{
								if ((dalx2 == xb1.back()) && (darx2 == xb2.back()))
									return;

								std::fill_n(std::next(dwall.begin(), dalx2), darx2 + 1, 0);
							}
							break;
						case 1:
							k = smoststart[i] - xb1[j];
							for(x=dalx2;x<=darx2;x++)
								if (smost[k+x] > uwall[x]) uwall[x] = smost[k+x];
							break;
						case 2:
							k = smoststart[i] - xb1[j];
							for(x=dalx2;x<=darx2;x++)
								if (smost[k+x] < dwall[x]) dwall[x] = smost[k+x];
							break;
					}
				}
			}
		}

			//sprite
		if ((searchit >= 1) && (searchx >= xb1.back()) && (searchx <= xb2.back()))
			if ((searchy >= uwall[searchx]) && (searchy <= dwall[searchx]))
			{
				searchsector = sectnum;
				searchwall = spritenum;
				searchstat = 3;
				searchit = 1;
			}

		if ((cstat&2) == 0) {
			maskwallscan(xb1.back(), xb2.back(), uwall, dwall, swall, lwall);
		} else {
			transmaskwallscan(xb1.back(), xb2.back());
		}
	}
	else if ((cstat&48) == 32)
	{
		if ((cstat&64) != 0)
			if ((globalpos.z > tspr->z) == ((cstat&8)==0))
				return;

		if ((cstat&4) > 0)
			xoff = -xoff;

		if ((cstat&8) > 0)
			yoff = -yoff;
		
		xspan = tilesizx[tilenum];
		yspan = tilesizy[tilenum];

			//Rotate center point
		dax = tspr->x-globalpos.x;
		day = tspr->y-globalpos.y;
		rzi[0] = dmulscalen<10>(cosglobalang,dax,singlobalang,day);
		rxi[0] = dmulscalen<10>(cosglobalang,day,-singlobalang,dax);

			//Get top-left corner
		i = ((tspr->ang+2048-globalang)&2047);
		cosang = sintable[(i+512)&2047]; sinang = sintable[i];
		dax = ((xspan>>1)+xoff)*tspr->xrepeat;
		day = ((yspan>>1)+yoff)*tspr->yrepeat;
		rzi[0] += dmulscalen<12>(sinang,dax,cosang,day);
		rxi[0] += dmulscalen<12>(sinang,day,-cosang,dax);

			//Get other 3 corners
		dax = xspan*tspr->xrepeat;
		day = yspan*tspr->yrepeat;
		rzi[1] = rzi[0]-mulscalen<12>(sinang,dax);
		rxi[1] = rxi[0]+mulscalen<12>(cosang,dax);
		dax = -mulscalen<12>(cosang,day);
		day = -mulscalen<12>(sinang,day);
		rzi[2] = rzi[1]+dax;
		rxi[2] = rxi[1]+day;
		rzi[3] = rzi[0]+dax;
		rxi[3] = rxi[0]+day;

			//Put all points on same z
		ryi[0] = scale((tspr->z-globalpos.z),yxaspect,320<<8);
		if (ryi[0] == 0) return;
		ryi[1] = ryi[2] = ryi[3] = ryi[0];

		if ((cstat&4) == 0) {
			z = 0;
			z1 = 1;
			z2 = 3;
		}
		else {
			z = 1;
			z1 = 0;
			z2 = 2;
		}

		dax = rzi[z1]-rzi[z];
		day = rxi[z1]-rxi[z];
		bot = dmulscalen<8>(dax,dax,day,day);
		if (((std::abs(dax)>>13) >= bot) || ((std::abs(day)>>13) >= bot)) return;
		g_pt1.x = divscalen<18>(dax,bot);
		g_pt2.x = divscalen<18>(day,bot);

		dax = rzi[z2]-rzi[z]; day = rxi[z2]-rxi[z];
		bot = dmulscalen<8>(dax,dax,day,day);
		if (((std::abs(dax)>>13) >= bot) || ((std::abs(day)>>13) >= bot)) return;
		g_pt1.y = divscalen<18>(dax,bot);
		g_pt2.y = divscalen<18>(day,bot);

			//Calculate globals for hline texture mapping function
		globalxpanning = (rxi[z]<<12);
		globalypanning = (rzi[z]<<12);
		globalzd = (ryi[z]<<12);

		rzi[0] = mulscalen<16>(rzi[0],viewingrange);
		rzi[1] = mulscalen<16>(rzi[1],viewingrange);
		rzi[2] = mulscalen<16>(rzi[2],viewingrange);
		rzi[3] = mulscalen<16>(rzi[3],viewingrange);

		if (ryi[0] < 0)   //If ceilsprite is above you, reverse order of points
		{
			std::swap(rxi[1], rxi[3]);
			std::swap(rzi[1], rzi[3]);
		}


			//Clip polygon in 3-space
		npoints = 4;

			//Clip edge 1
		npoints2 = 0;
		zzsgn = rxi[0] + rzi[0];

		for(z=0;z<npoints;z++)
		{
			zz = z+1; if (zz == npoints) zz = 0;
			zsgn = zzsgn; zzsgn = rxi[zz]+rzi[zz];
			if (zsgn >= 0)
			{
				rxi2[npoints2] = rxi[z];
				ryi2[npoints2] = ryi[z];
				rzi2[npoints2] = rzi[z];
				npoints2++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscalen<30>(zsgn,zsgn-zzsgn);
				rxi2[npoints2] = rxi[z] + mulscalen<30>(t,rxi[zz]-rxi[z]);
				ryi2[npoints2] = ryi[z] + mulscalen<30>(t,ryi[zz]-ryi[z]);
				rzi2[npoints2] = rzi[z] + mulscalen<30>(t,rzi[zz]-rzi[z]);
				npoints2++;
			}
		}
		if (npoints2 <= 2) return;

			//Clip edge 2
		npoints = 0;
		zzsgn = rxi2[0]-rzi2[0];
		for(z=0;z<npoints2;z++)
		{
			zz = z+1;
			if (zz == npoints2)
				zz = 0;
			zsgn = zzsgn;
			zzsgn = rxi2[zz] - rzi2[zz];
			if (zsgn <= 0)
			{
				rxi[npoints] = rxi2[z];
				ryi[npoints] = ryi2[z];
				rzi[npoints] = rzi2[z];
				npoints++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscalen<30>(zsgn,zsgn-zzsgn);
				rxi[npoints] = rxi2[z] + mulscalen<30>(t,rxi2[zz]-rxi2[z]);
				ryi[npoints] = ryi2[z] + mulscalen<30>(t,ryi2[zz]-ryi2[z]);
				rzi[npoints] = rzi2[z] + mulscalen<30>(t,rzi2[zz]-rzi2[z]);
				npoints++;
			}
		}
		if (npoints <= 2) return;

			//Clip edge 3
		npoints2 = 0;
		zzsgn = ryi[0]*halfxdimen + (rzi[0]*(globalhoriz-0));
		for(z=0;z<npoints;z++)
		{
			zz = z+1;
			if (zz == npoints)
				zz = 0;
			zsgn = zzsgn;
			zzsgn = ryi[zz]*halfxdimen + (rzi[zz]*(globalhoriz-0));
			if (zsgn >= 0)
			{
				rxi2[npoints2] = rxi[z];
				ryi2[npoints2] = ryi[z];
				rzi2[npoints2] = rzi[z];
				npoints2++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscalen<30>(zsgn,zsgn-zzsgn);
				rxi2[npoints2] = rxi[z] + mulscalen<30>(t,rxi[zz]-rxi[z]);
				ryi2[npoints2] = ryi[z] + mulscalen<30>(t,ryi[zz]-ryi[z]);
				rzi2[npoints2] = rzi[z] + mulscalen<30>(t,rzi[zz]-rzi[z]);
				npoints2++;
			}
		}
		if (npoints2 <= 2) return;

			//Clip edge 4
		npoints = 0;
		zzsgn = ryi2[0]*halfxdimen + (rzi2[0]*(globalhoriz-ydimen));
		for(z=0;z<npoints2;z++)
		{
			zz = z+1; if (zz == npoints2) zz = 0;
			zsgn = zzsgn; zzsgn = ryi2[zz]*halfxdimen + (rzi2[zz]*(globalhoriz-ydimen));
			if (zsgn <= 0)
			{
				rxi[npoints] = rxi2[z];
				ryi[npoints] = ryi2[z];
				rzi[npoints] = rzi2[z];
				npoints++;
			}
			if ((zsgn^zzsgn) < 0)
			{
				t = divscalen<30>(zsgn,zsgn-zzsgn);
				rxi[npoints] = rxi2[z] + mulscalen<30>(t,rxi2[zz]-rxi2[z]);
				ryi[npoints] = ryi2[z] + mulscalen<30>(t,ryi2[zz]-ryi2[z]);
				rzi[npoints] = rzi2[z] + mulscalen<30>(t,rzi2[zz]-rzi2[z]);
				npoints++;
			}
		}
		if (npoints <= 2) return;

			//Project onto screen
		lpoint = -1;
		lmax = 0x7fffffff;
		rpoint = -1;
		rmax = 0x80000000;
		
		for(z=0;z<npoints;z++)
		{
			xsi[z] = scale(rxi[z],xdimen<<15,rzi[z]) + (xdimen<<15);
			ysi[z] = scale(ryi[z],xdimen<<15,rzi[z]) + (globalhoriz<<16);
			if (xsi[z] < 0) xsi[z] = 0;
			if (xsi[z] > (xdimen<<16)) xsi[z] = (xdimen<<16);
			if (ysi[z] < ((int)0<<16)) ysi[z] = ((int)0<<16);
			if (ysi[z] > ((int)ydimen<<16)) ysi[z] = ((int)ydimen<<16);
			if (xsi[z] < lmax) lmax = xsi[z], lpoint = z;
			if (xsi[z] > rmax) rmax = xsi[z], rpoint = z;
		}

			//Get uwall arrays
		for(z=lpoint;z!=rpoint;z=zz)
		{
			zz = z+1; if (zz == npoints) zz = 0;

			dax1 = ((xsi[z]+65535)>>16);
			dax2 = ((xsi[zz]+65535)>>16);
			if (dax2 > dax1)
			{
				yinc = divscalen<16>(ysi[zz]-ysi[z],xsi[zz]-xsi[z]);
				y = ysi[z] + mulscalen<16>((dax1<<16)-xsi[z],yinc);
				qinterpolatedown16short(&uwall[dax1],dax2-dax1,y,yinc);
			}
		}

			//Get dwall arrays
		for(;z!=lpoint;z=zz)
		{
			zz = z+1; if (zz == npoints) zz = 0;

			dax1 = ((xsi[zz]+65535)>>16);
			dax2 = ((xsi[z]+65535)>>16);
			if (dax2 > dax1)
			{
				yinc = divscalen<16>(ysi[zz]-ysi[z],xsi[zz]-xsi[z]);
				y = ysi[zz] + mulscalen<16>((dax1<<16)-xsi[zz],yinc);
				qinterpolatedown16short(&dwall[dax1],dax2-dax1,y,yinc);
			}
		}


		lx = ((lmax+65535)>>16);
		rx = ((rmax+65535)>>16);
		for(x=lx;x<=rx;x++)
		{
			uwall[x] = std::max(static_cast<int>(uwall[x]), static_cast<int>(startumost[x + windowx1]) - windowy1);
			dwall[x] = std::min(static_cast<int>(dwall[x]), static_cast<int>(startdmost[x + windowx1]) - windowy1);
		}

			//Additional uwall/dwall clipping goes here
		for(i=smostwallcnt-1;i>=0;i--)
		{
			j = smostwall[i];
			if ((xb1[j] > rx) || (xb2[j] < lx)) continue;
			if ((yp <= yb1[j]) && (yp <= yb2[j])) continue;

				//if (spritewallfront(tspr,thewall[j]) == 0)
			x = thewall[j];
			xp1 = wall[x].pt.x;
			yp1 = wall[x].pt.y;
			x = wall[x].point2;
			xp2 = wall[x].pt.x;
			yp2 = wall[x].pt.y;
			x = (xp2-xp1)*(tspr->y-yp1)-(tspr->x-xp1)*(yp2-yp1);
			if ((yp > yb1[j]) && (yp > yb2[j])) x = -1;
			if ((x >= 0) && ((x != 0) || (wall[thewall[j]].nextsector != tspr->sectnum))) continue;

			dalx2 = std::max(xb1[j], lx);
			darx2 = std::min(xb2[j], rx);

			switch(smostwalltype[i])
			{
				case 0:
					if (dalx2 <= darx2)
					{
						if ((dalx2 == lx) && (darx2 == rx))
							return;

						std::fill_n(std::next(dwall.begin(), dalx2), darx2 + 1, 0);
					}
					break;
				case 1:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] > uwall[x]) uwall[x] = smost[k+x];
					break;
				case 2:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] < dwall[x]) dwall[x] = smost[k+x];
					break;
			}
		}

			//sprite
		if ((searchit >= 1) && (searchx >= lx) && (searchx <= rx))
			if ((searchy >= uwall[searchx]) && (searchy <= dwall[searchx]))
			{
				searchsector = sectnum; searchwall = spritenum;
				searchstat = 3; searchit = 1;
			}

		globalorientation = cstat;
		globalpicnum = tilenum;
		if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
		//if (picanm[globalpicnum]&192) globalpicnum += animateoffs((short)globalpicnum,spritenum+32768);

		if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
		setgotpic(globalpicnum);
		globalbufplc = waloff[globalpicnum];

		globvis = mulscalen<16>(globalhisibility,viewingrange);
		if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));

		x = picsiz[globalpicnum]; y = ((x>>4)&15); x &= 15;
		if (pow2long[x] != xspan)
		{
			x++;
			g_pt1.x = mulscale(g_pt1.x,xspan,x);
			g_pt2.x = mulscale(g_pt2.x,xspan,x);
		}

		dax = globalxpanning;
		day = globalypanning;
		globalxpanning = -dmulscalen<6>(g_pt1.x,day,g_pt2.x,dax);
		globalypanning = -dmulscalen<6>(g_pt1.y,day,g_pt2.y,dax);

		g_pt2 = mulscalen<16>(g_pt2, viewingrange);
		globalzd = mulscalen<16>(globalzd,viewingrangerecip);

		g_pt1.x = (g_pt1.x-g_pt2.x)*halfxdimen;
		g_pt1.y = (g_pt1.y-g_pt2.y)*halfxdimen;

		if ((cstat&2) == 0)
			msethlineshift(x,y);
		else
			tsethlineshift(x,y);

			//Draw it!
		ceilspritescan(lx,rx-1);
	}
	else if ((cstat&48) == 48)
	{
		int nxrepeat;
		int nyrepeat;

		lx = 0; rx = xdim-1;
		for(x=lx;x<=rx;x++)
		{
			lwall[x] = (int)startumost[x+windowx1]-windowy1;
			swall[x] = (int)startdmost[x+windowx1]-windowy1;
		}
		for(i=smostwallcnt-1;i>=0;i--)
		{
			j = smostwall[i];
			if ((xb1[j] > rx) || (xb2[j] < lx)) continue;
			if ((yp <= yb1[j]) && (yp <= yb2[j])) continue;
			if (spritewallfront(tspr, (int) thewall[j]) && ((yp <= yb1[j]) || (yp <= yb2[j]))) continue;

			dalx2 = std::max(xb1[j], lx);
			darx2 = std::min(xb2[j], rx);

			switch(smostwalltype[i])
			{
				case 0:
					if (dalx2 <= darx2)
					{
						if ((dalx2 == lx) && (darx2 == rx))
							return;

						std::fill_n(std::next(swall.begin(), dalx2), darx2 + 1, 0);
					}
					break;
				case 1:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] > lwall[x]) lwall[x] = smost[k+x];
					break;
				case 2:
					k = smoststart[i] - xb1[j];
					for(x=dalx2;x<=darx2;x++)
						if (smost[k+x] < swall[x]) swall[x] = smost[k+x];
					break;
			}
		}

		if (lwall[rx] >= swall[rx])
		{
			for(x=lx;x<rx;x++)
				if (lwall[x] < swall[x]) break;
			if (x == rx) return;
		}

		for(i=0;i<MAXVOXMIPS;i++) {
			if (!voxoff[vtilenum][i])
			{
				kloadvoxel(vtilenum);
				break;
			}
		}

		longptr = (int *)voxoff[vtilenum][0];

		if (voxscale[vtilenum] == 65536)
		{
			nxrepeat = (((int)tspr->xrepeat)<<16);
			nyrepeat = (((int)tspr->yrepeat)<<16);
		}
		else
		{
			nxrepeat = ((int)tspr->xrepeat)*voxscale[vtilenum];
			nyrepeat = ((int)tspr->yrepeat)*voxscale[vtilenum];
		}

		if (!(cstat&128)) tspr->z -= mulscalen<22>(longptr[5], nyrepeat);
		yoff = (int)((signed char)((picanm[sprite[tspr->owner].picnum]>>16)&255))+((int)tspr->yoffset);
		tspr->z -= mulscalen<14>(yoff,nyrepeat);

		globvis = globalvisibility;
		if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));

		if ((searchit >= 1) && (yp > (4<<8)) && (searchy >= lwall[searchx]) && (searchy < swall[searchx]))
		{
			siz = divscalen<19>(xdimenscale,yp);

			xv = mulscalen<16>(nxrepeat,xyaspect);

			xspan = (longptr[0] + longptr[1]) >> 1;
			yspan = longptr[2];
			xsiz = mulscalen<30>(siz,xv*xspan);
			ysiz = mulscalen<30>(siz,nyrepeat*yspan);

				//Watch out for divscale overflow
			if (((xspan>>11) < xsiz) && (yspan < (ysiz>>1)))
			{
				x1 = xb-(xsiz>>1);
				if (xspan&1) x1 += mulscalen<31>(siz,xv);  //Odd xspans
				i = mulscalen<30>(siz,xv*xoff);
				if ((cstat&4) == 0) x1 -= i; else x1 += i;

				y1 = mulscalen<16>(tspr->z-globalpos.z,siz);
				//y1 -= mulscalen<30>(siz,nyrepeat*yoff);
				y1 += (globalhoriz<<8)-ysiz;
				//if (cstat&128)  //Already fixed up above
				y1 += (ysiz>>1);

				x2 = x1+xsiz-1;
				y2 = y1+ysiz-1;
				if (((y1|255) < (y2|255)) && (searchx >= (x1>>8)+1) && (searchx <= (x2>>8)))
				{
					if ((sec->ceilingstat&3) == 0)
						startum = globalhoriz+mulscalen<24>(siz,sec->ceilingz-globalpos.z)-1;
					else
						startum = 0;
					if ((sec->floorstat&3) == 0)
						startdm = globalhoriz+mulscalen<24>(siz,sec->floorz-globalpos.z)+1;
					else
						startdm = 0x7fffffff;

						//sprite
					if ((searchy >= std::max(startum, y1 >> 8)) && (searchy < std::min(startdm, (y2 >> 8))))
					{
						searchsector = sectnum; searchwall = spritenum;
						searchstat = 3; searchit = 1;
					}
				}
			}
		}

		i = (int)tspr->ang+1536;
#if USE_POLYMOST && USE_OPENGL
		i += spriteext[tspr->owner].angoff;
#endif
		drawvox(tspr->x,tspr->y,tspr->z,i,(int)tspr->xrepeat,(int)tspr->yrepeat,vtilenum,tspr->shade,tspr->pal,lwall,swall);
	}

	if (automapping)
		show2dsprite[spritenum >> 3] |= pow2char[spritenum & 7];
}


//
// drawmaskwall (internal)
//
void drawmaskwall(short damaskwallcnt)
{
	int i;
	int j;
	int k;
	int x;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	if (rendmode != rendmode_t::Classic) { polymost_drawmaskwall(damaskwallcnt); return; }
#endif
	//============================================================================= //POLYMOST ENDS

	const int z = maskwall[damaskwallcnt];
	auto* wal = &wall[thewall[z]];
	const int sectnum = thesector[z];
	auto* sec = &g_sector[sectnum];
	auto* nsec = &g_sector[wal->nextsector];
	const int z1 = std::max(nsec->ceilingz, sec->ceilingz);
	const int z2 = std::min(nsec->floorz, sec->floorz);

	wallmost(uwall,z,sectnum,(char)0);
	wallmost(uplc,z,(int)wal->nextsector,(char)0);

	for(x=xb1[z];x<=xb2[z];x++) {
		if (uplc[x] > uwall[x])
			uwall[x] = uplc[x];
	}

	wallmost(dwall,z,sectnum,(char)1);
	wallmost(dplc,z,(int)wal->nextsector,(char)1);

	for(x=xb1[z];x<=xb2[z];x++) {
		if (dplc[x] < dwall[x])
			dwall[x] = dplc[x];
	}

	prepwall(z,wal);

	globalorientation = (int)wal->cstat;
	globalpicnum = wal->overpicnum;
	if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
	globalxpanning = (int)wal->xpanning;
	globalypanning = (int)wal->ypanning;
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)thewall[z]+16384);
	globalshade = (int)wal->shade;
	globvis = globalvisibility;
	if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));
	globalpal = (int)wal->pal;
	if (palookup[globalpal].empty())
		globalpal = 0;
	globalshiftval = (picsiz[globalpicnum]>>4);
	if (pow2long[globalshiftval] != tilesizy[globalpicnum]) globalshiftval++;
	globalshiftval = 32-globalshiftval;
	globalyscale = (wal->yrepeat<<(globalshiftval-19));
	if ((globalorientation&4) == 0)
		globalzd = (((globalpos.z-z1)*globalyscale)<<8);
	else
		globalzd = (((globalpos.z-z2)*globalyscale)<<8);
	globalzd += (globalypanning<<24);
	if (globalorientation&256) globalyscale = -globalyscale, globalzd = -globalzd;

	for(i=smostwallcnt-1;i>=0;i--)
	{
		j = smostwall[i];
		if ((xb1[j] > xb2[z]) || (xb2[j] < xb1[z])) continue;
		if (wallfront(j,z)) continue;

		const int lx = std::max(xb1[j], xb1[z]);
		const int rx = std::min(xb2[j], xb2[z]);

		switch(smostwalltype[i])
		{
			case 0:
				if (lx <= rx)
				{
					if ((lx == xb1[z]) && (rx == xb2[z])) return;
					//clearbufbyte(&dwall[lx],(rx-lx+1)*sizeof(dwall[0]),0L);
					std::ranges::fill(std::ranges::next(dwall.begin(), lx),
					                  std::ranges::next(dwall.begin(), rx + 1),
									  0);
				}
				break;
			case 1:
				k = smoststart[i] - xb1[j];
				for(x=lx;x<=rx;x++)
					if (smost[k+x] > uwall[x]) uwall[x] = smost[k+x];
				break;
			case 2:
				k = smoststart[i] - xb1[j];
				for(x=lx;x<=rx;x++)
					if (smost[k+x] < dwall[x]) dwall[x] = smost[k+x];
				break;
		}
	}

		//maskwall
	if ((searchit >= 1) && (searchx >= xb1[z]) && (searchx <= xb2[z]))
		if ((searchy >= uwall[searchx]) && (searchy <= dwall[searchx]))
		{
			searchsector = sectnum;
			searchwall = thewall[z];
			searchstat = 4;
			searchit = 1;
		}

	if ((globalorientation&128) == 0)
		maskwallscan(xb1[z],xb2[z],uwall,dwall, swall, lwall);
	else
	{
		if (globalorientation&128)
		{
			if (globalorientation&512)
				settransreverse();
			else
				settransnormal();
		}

		transmaskwallscan(xb1[z],xb2[z]);
	}
}


//
// fillpolygon (internal)
//
void fillpolygon(int npoints)
{
	std::vector<short*> dotp1{MAXYDIM};
	std::vector<short*> dotp2{MAXYDIM};

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == rendmode_t::OpenGL) {
		polymost_fillpolygon(npoints);
		return;
	}
#endif
	
	int miny{0x7fffffff};
	int maxy{static_cast<int>(0x80000000)}; // TODO: Correct to cast to int here?
	
	for(int z = npoints - 1; z >= 0; --z) {
		const int y = ry1[z];
		miny = std::min(miny, y);
		maxy = std::max(maxy, y);
	}

	miny = miny >> 12;
	maxy = maxy >> 12;
	
	if (miny < 0)
		miny = 0;
	if (maxy >= ydim)
		maxy = ydim - 1;
	
	short* ptr = &smost[0];    //They're pointers! - watch how you optimize this thing
	
	for(int y{miny}; y <= maxy; ++y)
	{
		dotp1[y] = ptr;
		dotp2[y] = ptr + (MAXNODESPERLINE >> 1);
		ptr += MAXNODESPERLINE;
	}

	for(int z = npoints - 1; z >= 0; --z)
	{
		const int zz = xb1[z];
		const int y1 = ry1[z];
		const int day1 = (y1 >> 12);
		const int y2 = ry1[zz];
		const int day2 = (y2 >> 12);
		
		if (day1 != day2)
		{
			int x1 = rx1[z];
			int x2 = rx1[zz];
			const int xinc = divscalen<12>(x2 - x1, y2 - y1);
			
			if (day2 > day1) {
				x1 += mulscalen<12>((day1 << 12) + 4095 - y1, xinc);
				for(int y{day1}; y < day2; ++y) {
					*dotp2[y]++ = (x1 >> 12);
					x1 += xinc;
				}
			}
			else {
				x2 += mulscalen<12>((day2 << 12) + 4095 - y2, xinc);
				for(int y{day2}; y < day1; ++y) {
					*dotp1[y]++ = (x2 >> 12);
					x2 += xinc;
				}
			}
		}
	}

	g_pt1.x = mulscalen<16>(g_pt1.x,xyaspect);
	g_pt2.y = mulscalen<16>(g_pt2.y,xyaspect);

	const int oy = miny + 1 - (ydim >> 1);
	globalpos.x += oy * g_pt1.x;
	globalpos.y += oy * g_pt2.y;

	setuphlineasm4(asm1, asm2);

	ptr = &smost[0];
	
	for(int y{miny}; y <= maxy; ++y) {
		const int cnt = (int)(dotp1[y] - ptr);
		short* ptr2 = ptr + (MAXNODESPERLINE >> 1);
		
		for(int z = cnt - 1; z >= 0; --z) {
			int day1{0};
			int day2{0};
			
			for(int zz{z}; zz > 0; --zz)
			{
				if (ptr[zz] < ptr[day1])
					day1 = zz;
				
				if (ptr2[zz] < ptr2[day2])
					day2 = zz;
			}
			
			const int x1 = ptr[day1];
			ptr[day1] = ptr[z];
			const int x2 = ptr2[day2] - 1;
			ptr2[day2] = ptr2[z];
			
			if (x1 > x2)
				continue;

			if (globalpolytype < 1) {
					//maphline
				const int ox = x2 + 1 - (xdim >> 1);
				const int bx = ox * asm1 + globalpos.x;
				const int by = ox * asm2 - globalpos.y;

				intptr_t p = ylookup[y] + x2 + frameplace;
				hlineasm4(x2 - x1, -1L, globalshade << 8, by, bx, (void *) p);
			}
			else {
					//maphline
				const int ox = x1 + 1 - (xdim >> 1);
				const int bx = ox * asm1 + globalpos.x;
				const int by = ox * asm2 - globalpos.y;

				intptr_t p = ylookup[y] + x1 + frameplace;
				
				if (globalpolytype == 1)
					mhline((void *) globalbufplc, bx, (x2 - x1) << 16, 0L, by, (void *) p);
				else
					thline((void *) globalbufplc, bx, (x2 - x1) << 16, 0L, by, (void *) p);
			}
		}

		globalpos.x += g_pt1.x;
		globalpos.y += g_pt2.y;
		ptr += MAXNODESPERLINE;
	}

	faketimerhandler();
}


//
// clippoly (internal)
//
int clippoly(int npoints, int clipstat)
{
	int z;
	int zz;
	int s1;
	int s2;
	int t;
	int npoints2;
	int start2;
	int z1;
	int z2;
	int z3;
	int z4;
	int splitcnt;

	int cx1 = windowx1;
	int cy1 = windowy1;
	int cx2 = windowx2 + 1;
	int cy2 = windowy2 + 1;

	cx1 <<= 12;
	cy1 <<= 12;
	cx2 <<= 12;
	cy2 <<= 12;

	if (clipstat&0xa)   //Need to clip top or left
	{
		npoints2 = 0;
		start2 = 0;
		z = 0;
		splitcnt = 0;
		do
		{
			s2 = cx1-rx1[z];
			do
			{
				zz = xb1[z];
				xb1[z] = -1;
				s1 = s2;
				s2 = cx1-rx1[zz];

				if (s1 < 0)
				{
					rx2[npoints2] = rx1[z];
					ry2[npoints2] = ry1[z];
					xb2[npoints2] = npoints2+1;
					npoints2++;
				}
				if ((s1^s2) < 0)
				{
					rx2[npoints2] = rx1[z]+scale(rx1[zz]-rx1[z],s1,s1-s2);
					ry2[npoints2] = ry1[z]+scale(ry1[zz]-ry1[z],s1,s1-s2);
					if (s1 < 0) p2[splitcnt++] = npoints2;
					xb2[npoints2] = npoints2+1;
					npoints2++;
				}
				z = zz;
			} while (xb1[z] >= 0);

			if (npoints2 >= start2+3)
				xb2[npoints2-1] = start2, start2 = npoints2;
			else
				npoints2 = start2;

			z = 1;
			while ((z < npoints) && (xb1[z] < 0)) z++;
		} while (z < npoints);
		if (npoints2 <= 2) return(0);
		
		for(z=1;z<splitcnt;z++)
			for(zz=0;zz<z;zz++)
			{
				z1 = p2[z];
				z2 = xb2[z1];
				z3 = p2[zz];
				z4 = xb2[z3];
				s1  = std::abs(rx2[z1]-rx2[z2])+std::abs(ry2[z1]-ry2[z2]);
				s1 += std::abs(rx2[z3]-rx2[z4])+std::abs(ry2[z3]-ry2[z4]);
				s2  = std::abs(rx2[z1]-rx2[z4])+std::abs(ry2[z1]-ry2[z4]);
				s2 += std::abs(rx2[z3]-rx2[z2])+std::abs(ry2[z3]-ry2[z2]);
				if (s2 < s1) {
					std::swap(xb2[p2[z]], xb2[p2[zz]]);
				}
		}


		npoints = 0;
		start2 = 0;
		z = 0;
		splitcnt = 0;
		
		do
		{
			s2 = cy1-ry2[z];
			do
			{
				zz = xb2[z]; xb2[z] = -1;
				s1 = s2; s2 = cy1-ry2[zz];
				if (s1 < 0)
				{
					rx1[npoints] = rx2[z]; ry1[npoints] = ry2[z];
					xb1[npoints] = npoints+1; npoints++;
				}
				if ((s1^s2) < 0)
				{
					rx1[npoints] = rx2[z]+scale(rx2[zz]-rx2[z],s1,s1-s2);
					ry1[npoints] = ry2[z]+scale(ry2[zz]-ry2[z],s1,s1-s2);
					if (s1 < 0) p2[splitcnt++] = npoints;
					xb1[npoints] = npoints+1;
					npoints++;
				}
				z = zz;
			} while (xb2[z] >= 0);

			if (npoints >= start2+3)
				xb1[npoints-1] = start2, start2 = npoints;
			else
				npoints = start2;

			z = 1;
			while ((z < npoints2) && (xb2[z] < 0)) z++;
		} while (z < npoints2);
		
		if (npoints <= 2)
			return 0;

		for(z=1;z<splitcnt;z++)
			for(zz=0;zz<z;zz++)
			{
				z1 = p2[z];
				z2 = xb1[z1];
				z3 = p2[zz];
				z4 = xb1[z3];
				s1  = std::abs(rx1[z1]-rx1[z2])+std::abs(ry1[z1]-ry1[z2]);
				s1 += std::abs(rx1[z3]-rx1[z4])+std::abs(ry1[z3]-ry1[z4]);
				s2  = std::abs(rx1[z1]-rx1[z4])+std::abs(ry1[z1]-ry1[z4]);
				s2 += std::abs(rx1[z3]-rx1[z2])+std::abs(ry1[z3]-ry1[z2]);
				if (s2 < s1) {
					std::swap(xb1[p2[z]], xb1[p2[zz]]);
				}
			}
	}
	if (clipstat&0x5)   //Need to clip bottom or right
	{
		npoints2 = 0;
		start2 = 0;
		z = 0;
		splitcnt = 0;
		do
		{
			s2 = rx1[z]-cx2;
			do
			{
				zz = xb1[z];
				xb1[z] = -1;
				s1 = s2;
				s2 = rx1[zz]-cx2;
				
				if (s1 < 0)
				{
					rx2[npoints2] = rx1[z];
					ry2[npoints2] = ry1[z];
					xb2[npoints2] = npoints2 + 1;
					++npoints2;
				}

				if ((s1^s2) < 0)
				{
					rx2[npoints2] = rx1[z] + scale(rx1[zz]-rx1[z], s1, s1-s2);
					ry2[npoints2] = ry1[z] + scale(ry1[zz]-ry1[z], s1, s1-s2);
					if (s1 < 0)
						p2[splitcnt++] = npoints2;
					xb2[npoints2] = npoints2+1;
					npoints2++;
				}
				z = zz;
			} while (xb1[z] >= 0);

			if (npoints2 >= start2+3) {
				xb2[npoints2-1] = start2;
				start2 = npoints2;
			}
			else
				npoints2 = start2;

			z = 1;
			while ((z < npoints) && (xb1[z] < 0)) z++;
		} while (z < npoints);
		
		if (npoints2 <= 2)
			return 0;
		
		for(z=1;z<splitcnt;z++) {
			for(zz=0;zz<z;zz++) {
				z1 = p2[z];
				z2 = xb2[z1];
				z3 = p2[zz];
				z4 = xb2[z3];
				s1  = std::abs(rx2[z1]-rx2[z2])+std::abs(ry2[z1]-ry2[z2]);
				s1 += std::abs(rx2[z3]-rx2[z4])+std::abs(ry2[z3]-ry2[z4]);
				s2  = std::abs(rx2[z1]-rx2[z4])+std::abs(ry2[z1]-ry2[z4]);
				s2 += std::abs(rx2[z3]-rx2[z2])+std::abs(ry2[z3]-ry2[z2]);
				if (s2 < s1) {
					std::swap(xb2[p2[z]], xb2[p2[zz]]);
				}
			}
		}


		npoints = 0;
		start2 = 0;
		z = 0;
		splitcnt = 0;
		
		do
		{
			s2 = ry2[z]-cy2;
			do
			{
				zz = xb2[z]; xb2[z] = -1;
				s1 = s2; s2 = ry2[zz]-cy2;
				if (s1 < 0)
				{
					rx1[npoints] = rx2[z]; ry1[npoints] = ry2[z];
					xb1[npoints] = npoints+1; npoints++;
				}
				if ((s1^s2) < 0)
				{
					rx1[npoints] = rx2[z]+scale(rx2[zz]-rx2[z],s1,s1-s2);
					ry1[npoints] = ry2[z]+scale(ry2[zz]-ry2[z],s1,s1-s2);
					if (s1 < 0) p2[splitcnt++] = npoints;
					xb1[npoints] = npoints+1;
					npoints++;
				}
				z = zz;
			} while (xb2[z] >= 0);

			if (npoints >= start2+3)
				xb1[npoints-1] = start2, start2 = npoints;
			else
				npoints = start2;

			z = 1;
			while ((z < npoints2) && (xb2[z] < 0)) z++;
		} while (z < npoints2);
		if (npoints <= 2) return(0);

		for(z=1;z<splitcnt;z++) {
			for(zz=0;zz<z;zz++) {
				z1 = p2[z];
				z2 = xb1[z1];
				z3 = p2[zz];
				z4 = xb1[z3];
				s1  = std::abs(rx1[z1]-rx1[z2])+std::abs(ry1[z1]-ry1[z2]);
				s1 += std::abs(rx1[z3]-rx1[z4])+std::abs(ry1[z3]-ry1[z4]);
				s2  = std::abs(rx1[z1]-rx1[z4])+std::abs(ry1[z1]-ry1[z4]);
				s2 += std::abs(rx1[z3]-rx1[z2])+std::abs(ry1[z3]-ry1[z2]);
				if (s2 < s1) {
					std::swap(xb1[p2[z]], xb1[p2[zz]]);
				}
			}
		}
	}

	return npoints;
}


//
// clippoly4 (internal)
//
	//Assume npoints=4 with polygon on &nrx1,&nry1
	//JBF 20031206: Thanks to Ken's hunting, s/(rx1|ry1|rx2|ry2)/n\1/ in this function
int clippoly4(int cx1, int cy1, int cx2, int cy2)
{
	int z{0};
	int nn{0};

	do
	{
		const int zz = ((z + 1) & 3);
		const int x1 = nrx1[z];
		const int x2 = nrx1[zz] - x1;

		if ((cx1 <= x1) && (x1 <= cx2)) {
			nrx2[nn] = x1;
			nry2[nn] = nry1[z];
			++nn;
		}

		int x{0};
		if (x2 <= 0)
			x = cx2;
		else
			x = cx1;
		
		int t = x - x1;

		if (((t - x2) ^ t) < 0) {
			nrx2[nn] = x;
			nry2[nn] = nry1[z] + scale(t, nry1[zz] - nry1[z], x2);
			++nn;
		}

		if (x2 <= 0)
			x = cx1;
		else
			x = cx2;
		
		t = x - x1;

		if (((t - x2) ^ t) < 0) {
			nrx2[nn] = x;
			nry2[nn] = nry1[z] + scale(t, nry1[zz] - nry1[z], x2);
			++nn;
		}

		z = zz;
	} while (z != 0);
	
	if (nn < 3)
		return 0;

	int n{0};
	z = 0;
	
	do
	{
		int zz = z + 1;
		
		if (zz == nn) 
			zz = 0;

		const int y1 = nry2[z];
		const int y2 = nry2[zz] - y1;

		if ((cy1 <= y1) && (y1 <= cy2)) {
			nry1[n] = y1;
			nrx1[n] = nrx2[z];
			++n;
		}

		int y{0};
		if (y2 <= 0)
			y = cy2;
		else
			y = cy1;
		
		int t = y - y1;
		
		if (((t - y2) ^ t) < 0) {
			nry1[n] = y;
			nrx1[n] = nrx2[z] + scale(t, nrx2[zz] - nrx2[z], y2);
			++n;
		}

		if (y2 <= 0)
			y = cy1;
		else
			y = cy2;

		t = y - y1;

		if (((t - y2) ^ t) < 0) {
			nry1[n] = y;
			nrx1[n] = nrx2[z] + scale(t, nrx2[zz] - nrx2[z], y2);
			++n;
		}

		z = zz;
	} while (z != 0);

	return n;
}


//
// dorotatesprite (internal)
//
	//JBF 20031206: Thanks to Ken's hunting, s/(rx1|ry1|rx2|ry2)/n\1/ in this function
void dorotatesprite(int sx, int sy, int z, short a, short picnum, signed char dashade,
	unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2, int uniqid)
{
	int x;
	int y;
	int x1;
	int y1;
	int x2;
	int y2;
	intptr_t i;
	intptr_t p;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	if (rendmode != rendmode_t::Classic) {
		polymost_dorotatesprite(sx, sy, z, a, picnum, dashade, dapalnum, dastat, cx1, cy1, cx2, cy2, uniqid);
		return;
	}
#else
	(void)uniqid;
#endif
	//============================================================================= //POLYMOST ENDS

	if (cx1 < 0)
		cx1 = 0;

	if (cy1 < 0)
		cy1 = 0;

	if (cx2 > xres - 1)
		cx2 = xres - 1;

	if (cy2 > yres - 1)
		cy2 = yres - 1;

	const int xsiz = tilesizx[picnum];
	const int ysiz = tilesizy[picnum];

	int xoff{0};
	int yoff{0};

	if ((dastat & 16) == 0) {
		xoff = (int)((signed char)((picanm[picnum]>>8)&255))+(xsiz>>1);
		yoff = (int)((signed char)((picanm[picnum]>>16)&255))+(ysiz>>1);
	}

	if (dastat & 4)
		yoff = ysiz - yoff;

	const int cosang = sintable[(a+512)&2047];
	const int sinang = sintable[a&2047];

	if ((dastat & 2) != 0)  //Auto window size scaling
	{
		if ((dastat&8) == 0)
		{
			if (widescreen) {
				x = ydimenscale;   //= scale(xdimen,yxaspect,320);
				sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),ydimen<<16,200*pixelaspect);
			} else {
				x = xdimenscale;   //= scale(xdimen,yxaspect,320);
				sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),xdimen,320);
			}
			sy = ((cy1+cy2+2)<<15)+mulscalen<16>(sy-(200<<15),x);
		}
		else
		{
			  //If not clipping to startmosts, & auto-scaling on, as a
			  //hard-coded bonus, scale to full screen instead
			if (widescreen) {
				x = scale(ydim<<16,yxaspect,200*pixelaspect);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),ydim<<16,200*pixelaspect);
			} else {
				x = scale(xdim,yxaspect,320);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),xdim,320);
			}
			sy = (ydim<<15)+32768+mulscalen<16>(sy-(200<<15),x);
		}
		z = mulscalen<16>(z,x);
	}

	int xv = mulscalen<14>(cosang,z);
	int yv = mulscalen<14>(sinang,z);
	
	int xv2{0};
	int yv2{0};

	if (((dastat & 2) != 0) || ((dastat & 8) == 0)) //Don't aspect unscaled perms
	{
		xv2 = mulscalen<16>(xv, xyaspect);
		yv2 = mulscalen<16>(yv, xyaspect);
	}
	else
	{
		xv2 = xv;
		yv2 = yv;
	}

	nry1[0] = sy - (yv * xoff + xv * yoff);
	nry1[1] = nry1[0] + yv * xsiz;
	nry1[3] = nry1[0] + xv * ysiz;
	nry1[2] = nry1[1] + nry1[3] - nry1[0];
	i = (cy1<<16);
	
	if ((nry1[0]<i) && (nry1[1]<i) && (nry1[2]<i) && (nry1[3]<i))
		return;
	
	i = (cy2<<16);
	
	if ((nry1[0]>i) && (nry1[1]>i) && (nry1[2]>i) && (nry1[3]>i))
		return;

	nrx1[0] = sx - (xv2*xoff - yv2*yoff);
	nrx1[1] = nrx1[0] + xv2*xsiz;
	nrx1[3] = nrx1[0] - yv2*ysiz;
	nrx1[2] = nrx1[1] + nrx1[3] - nrx1[0];
	
	i = (cx1<<16);
	
	if ((nrx1[0]<i) && (nrx1[1]<i) && (nrx1[2]<i) && (nrx1[3]<i))
		return;
	
	i = (cx2<<16);

	if ((nrx1[0]>i) && (nrx1[1]>i) && (nrx1[2]>i) && (nrx1[3]>i))
		return;

	const int gx1 = nrx1[0];
	const int gy1 = nry1[0];   //back up these before clipping
	
	int npoints{0};
	if ((npoints = clippoly4(cx1<<16,cy1<<16,(cx2+1)<<16,(cy2+1)<<16)) < 3)
		return;

	int lx = nrx1[0];
	int rx = nrx1[0];

	for(int nextv{0}, v = npoints - 1; v >= 0; --v)
	{
		x1 = nrx1[v];
		x2 = nrx1[nextv];
		const int dax1 = (x1>>16);
		if (x1 < lx)
			lx = x1;

		const int dax2 = (x2 >> 16);

		if (x1 > rx)
			rx = x1;

		if (dax1 != dax2)
		{
			y1 = nry1[v];
			y2 = nry1[nextv];
			const int yinc = divscalen<16>(y2 - y1, x2 - x1);

			if (dax2 > dax1)
			{
				const int yplc = y1 + mulscalen<16>((dax1<<16)+65535-x1,yinc);
				qinterpolatedown16short(&uplc[dax1],dax2-dax1,yplc,yinc);
			}
			else
			{
				const int yplc = y2 + mulscalen<16>((dax2<<16)+65535-x2,yinc);
				qinterpolatedown16short(&dplc[dax2],dax1-dax2,yplc,yinc);
			}
		}

		nextv = v;
	}

	if (waloff[picnum] == 0) loadtile(picnum);
	setgotpic(picnum);
	intptr_t bufplc = waloff[picnum];

	auto palookupoffs = (intptr_t) palookup[dapalnum].data() + (getpalookup(0L, (int)dashade) << 8);

	const int iv = divscalen<32>(1L,z);
	xv = mulscalen<14>(sinang, iv);
	yv = mulscalen<14>(cosang, iv);
	if (((dastat&2) != 0) || ((dastat&8) == 0)) //Don't aspect unscaled perms
	{
		yv2 = mulscalen<16>(-xv,yxaspect);
		xv2 = mulscalen<16>(yv,yxaspect);
	}
	else
	{
		yv2 = -xv;
		xv2 = yv;
	}

	x1 = (lx>>16);
	x2 = (rx>>16);

	int oy{0};
	x = (x1 << 16) - 1 - gx1;
	y = (oy << 16) + 65535 - gy1;
	int bx = dmulscalen<16>(x,xv2,y,xv);
	int by = dmulscalen<16>(x,yv2,y,yv);
	
	if (dastat&4) {
		yv = -yv;
		yv2 = -yv2;
		by = (ysiz << 16) - 1 - by;
	}

#ifndef USING_A_C
	int ny1;
	int ny2;
	int xx;
	int xend;
	bool qlinemode{false};
	std::array<int, 4> y1ve;
	std::array<int, 4> y2ve;
	int u4;
	int d4;
	char bad;

	if ((dastat&1) == 0)
	{
		if (((a&1023) == 0) && (ysiz <= 256))  //vlineasm4 has 256 high limit!
		{
			if (dastat&64)
				setupvlineasm(24L);
			else
				setupmvlineasm(24L);

			by <<= 8;
			yv <<= 8;
			yv2 <<= 8;

			std::ranges::fill(palookupoffse, palookupoffs);
			std::ranges::fill(vince, yv);

			for(x=x1;x<x2;x+=4)
			{
				bad = 15;
				xend = std::min(x2 - x, 4);
				for(xx=0;xx<xend;xx++)
				{
					bx += xv2;

					y1 = uplc[x + xx];
					y2 = dplc[x + xx];

					if ((dastat & 8) == 0)
					{
						if (startumost[x + xx] > y1)
							y1 = startumost[x + xx];
						
						if (startdmost[x + xx] < y2)
							y2 = startdmost[x + xx];
					}

					if (y2 <= y1)
						continue;

					by += yv * (y1 - oy);
					oy = y1;

					bufplce[xx] = (bx >> 16) * ysiz + bufplc;
					vplce[xx] = by;
					y1ve[xx] = y1;
					y2ve[xx] = y2 - 1;
					bad &= ~pow2char[xx];
				}

				p = x + frameplace;

				u4 = std::max(std::max(y1ve[0], y1ve[1]), std::max(y1ve[2], y1ve[3]));
				d4 = std::min(std::min(y2ve[0], y2ve[1]), std::min(y2ve[2], y2ve[3]));

				if (dastat & 64)
				{
					if ((bad != 0) || (u4 >= d4))
					{
						if (!(bad & 1)) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
						if (!(bad & 2)) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
						if (!(bad & 4)) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
						if (!(bad & 8)) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));
						continue;
					}

					if (u4 > y1ve[0]) vplce[0] = prevlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
					if (u4 > y1ve[1]) vplce[1] = prevlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
					if (u4 > y1ve[2]) vplce[2] = prevlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
					if (u4 > y1ve[3]) vplce[3] = prevlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));

					if (d4 >= u4)
						vlineasm4(d4 - u4 + 1, (void *)(ylookup[u4] + p));

					i = p + ylookup[d4 + 1];
					if (y2ve[0] > d4) prevlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
					if (y2ve[1] > d4) prevlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
					if (y2ve[2] > d4) prevlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
					if (y2ve[3] > d4) prevlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
				}
				else
				{
					if ((bad != 0) || (u4 >= d4))
					{
						if (!(bad&1)) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-y1ve[0],vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
						if (!(bad&2)) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-y1ve[1],vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
						if (!(bad&4)) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-y1ve[2],vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
						if (!(bad&8)) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-y1ve[3],vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));
						continue;
					}

					if (u4 > y1ve[0]) vplce[0] = mvlineasm1(vince[0],(void *)palookupoffse[0],u4-y1ve[0]-1,vplce[0],(void *)bufplce[0],(void *)(ylookup[y1ve[0]]+p+0));
					if (u4 > y1ve[1]) vplce[1] = mvlineasm1(vince[1],(void *)palookupoffse[1],u4-y1ve[1]-1,vplce[1],(void *)bufplce[1],(void *)(ylookup[y1ve[1]]+p+1));
					if (u4 > y1ve[2]) vplce[2] = mvlineasm1(vince[2],(void *)palookupoffse[2],u4-y1ve[2]-1,vplce[2],(void *)bufplce[2],(void *)(ylookup[y1ve[2]]+p+2));
					if (u4 > y1ve[3]) vplce[3] = mvlineasm1(vince[3],(void *)palookupoffse[3],u4-y1ve[3]-1,vplce[3],(void *)bufplce[3],(void *)(ylookup[y1ve[3]]+p+3));

					if (d4 >= u4)
						mvlineasm4(d4 - u4 + 1, (void *)(ylookup[u4] + p));

					i = p + ylookup[d4 + 1];
					if (y2ve[0] > d4) mvlineasm1(vince[0],(void *)palookupoffse[0],y2ve[0]-d4-1,vplce[0],(void *)bufplce[0],(void *)(i+0));
					if (y2ve[1] > d4) mvlineasm1(vince[1],(void *)palookupoffse[1],y2ve[1]-d4-1,vplce[1],(void *)bufplce[1],(void *)(i+1));
					if (y2ve[2] > d4) mvlineasm1(vince[2],(void *)palookupoffse[2],y2ve[2]-d4-1,vplce[2],(void *)bufplce[2],(void *)(i+2));
					if (y2ve[3] > d4) mvlineasm1(vince[3],(void *)palookupoffse[3],y2ve[3]-d4-1,vplce[3],(void *)bufplce[3],(void *)(i+3));
				}

				faketimerhandler();
			}
		}
		else
		{
			if (dastat & 64)
			{
				if ((xv2 & 0x0000ffff) == 0)
				{
					qlinemode = true;
					setupqrhlineasm4(0L,yv2<<16,(xv2>>16)*ysiz+(yv2>>16),(void *)palookupoffs,0L,0L);
				}
				else
				{
					qlinemode = false;
					setuprhlineasm4(xv2<<16,yv2<<16,(xv2>>16)*ysiz+(yv2>>16),(void *)palookupoffs,ysiz,0L);
				}
			}
			else
				setuprmhlineasm4(xv2<<16,yv2<<16,(xv2>>16)*ysiz+(yv2>>16),(void *)palookupoffs,ysiz,0L);

			y1 = uplc[x1];

			if (((dastat&8) == 0) && (startumost[x1] > y1))
				y1 = startumost[x1];

			y2 = y1;

			for(x=x1;x<x2;x++)
			{
				ny1 = uplc[x] - 1;
				ny2 = dplc[x];
				
				if ((dastat&8) == 0)
				{
					if (startumost[x]-1 > ny1)
						ny1 = startumost[x]-1;

					if (startdmost[x] < ny2)
						ny2 = startdmost[x];
				}

				if (ny1 < ny2-1)
				{
					if (ny1 >= y2)
					{
						while (y1 < y2-1)
						{
							++y1;
							if ((y1 & 31) == 0)
								faketimerhandler();

								//x,y1
							bx += xv * (y1 - oy);
							by += yv * (y1 - oy);
							oy = y1;

							if (dastat & 64) {
								if (qlinemode)
									qrhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y1]+x+frameplace));
								else
									rhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
							}
							else
								rmhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
						}

						y1 = ny1;
					}
					else
					{
						while (y1 < ny1)
						{
							++y1;
							
							if ((y1 & 31) == 0)
								faketimerhandler();

								//x,y1
							bx += xv * (y1 - oy);
							by += yv * (y1 - oy);
							oy = y1;
							
							if (dastat & 64) {
								if (qlinemode)
									qrhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y1]+x+frameplace));
								else
									rhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
							}
							else
								rmhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
						}
						
						while (y1 > ny1) {
							lastx[y1--] = x;
						}
					}

					while (y2 > ny2)
					{
						--y2;
						
						if ((y2&31) == 0)
							faketimerhandler();

							//x,y2
						bx += xv * (y2 - oy);
						by += yv * (y2 - oy);
						oy = y2;
						if (dastat&64) {
							if (qlinemode) qrhlineasm4(x-lastx[y2],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y2]+x+frameplace));
							else rhlineasm4(x-lastx[y2],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y2]+x+frameplace));
						} else rmhlineasm4(x-lastx[y2],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y2]+x+frameplace));
					}

					while (y2 < ny2) {
						lastx[y2++] = x;
					}
				}
				else
				{
					while (y1 < y2-1)
					{
						y1++; if ((y1&31) == 0) faketimerhandler();

							//x,y1
						bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1;
						if (dastat&64) {
							if (qlinemode) qrhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L    ,by<<16,(void *)(ylookup[y1]+x+frameplace));
							else rhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
						} else rmhlineasm4(x-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x+frameplace));
					}
					
					if (x == x2-1) {
						bx += xv2;
						by += yv2;
						break;
					}

					y1 = uplc[x+1];
					if (((dastat&8) == 0) && (startumost[x+1] > y1)) y1 = startumost[x+1];
					y2 = y1;
				}

				bx += xv2;
				by += yv2;
			}

			while (y1 < y2 - 1)
			{
				++y1;
				
				if ((y1 & 31) == 0)
					faketimerhandler();

					//x2,y1
				bx += xv * (y1 - oy);
				by += yv * (y1 - oy);
				oy = y1;

				if (dastat & 64) {
					if (qlinemode) qrhlineasm4(x2-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,0L,by<<16,(void *)(ylookup[y1]+x2+frameplace));
					else rhlineasm4(x2-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x2+frameplace));
				} else rmhlineasm4(x2-lastx[y1],(void *)((bx>>16)*ysiz+(by>>16)+bufplc),0L,bx<<16,by<<16,(void *)(ylookup[y1]+x2+frameplace));
			}
		}
	}
	else
	{
		if ((dastat&1) == 0)
		{
			if (dastat&64)
				setupspritevline((void *)palookupoffs,(xv>>16)*ysiz,xv<<16,ysiz,yv,0L);
			else
				msetupspritevline((void *)palookupoffs,(xv>>16)*ysiz,xv<<16,ysiz,yv,0L);
		}
		else
		{
			tsetupspritevline((void *)palookupoffs,(xv>>16)*ysiz,xv<<16,ysiz,yv,0L);
			
			if (dastat&32)
				settransreverse();
			else
				settransnormal();
		}
		for(x=x1;x<x2;x++)
		{
			bx += xv2;
			by += yv2;

			y1 = uplc[x];
			y2 = dplc[x];
			if ((dastat & 8) == 0)
			{
				if (startumost[x] > y1)
					y1 = startumost[x];

				if (startdmost[x] < y2)
					y2 = startdmost[x];
			}

			if (y2 <= y1)
				continue;

			switch(y1 - oy)
			{
				case -1: bx -= xv; by -= yv; oy = y1; break;
				case 0: break;
				case 1: bx += xv; by += yv; oy = y1; break;
				default: bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1; break;
			}

			p = ylookup[y1]+x+frameplace;

			if ((dastat&1) == 0)
			{
				if (dastat&64)
					spritevline(0L,by<<16,y2-y1+1,bx<<16,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
				else
					mspritevline(0L,by<<16,y2-y1+1,bx<<16,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			}
			else
			{
				tspritevline(0L,by<<16,y2-y1+1,bx<<16,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			}
			
			faketimerhandler();
		}
	}

#else	// USING_A_C

	if ((dastat&1) == 0)
	{
		if (dastat&64)
			setupspritevline((void *)palookupoffs,xv,yv,ysiz);
		else
			msetupspritevline((void *)palookupoffs,xv,yv,ysiz);
	}
	else
	{
		tsetupspritevline((void *)palookupoffs,xv,yv,ysiz);
		if (dastat&32) settransreverse(); else settransnormal();
	}
	for(x=x1;x<x2;x++)
	{
		bx += xv2;
		by += yv2;

		y1 = uplc[x];
		y2 = dplc[x];

		if ((dastat&8) == 0) {
			if (startumost[x] > y1)
				y1 = startumost[x];
			
			if (startdmost[x] < y2)
				y2 = startdmost[x];
		}

		if (y2 <= y1)
			continue;

		switch(y1-oy)
		{
			case -1: bx -= xv; by -= yv; oy = y1; break;
			case 0: break;
			case 1: bx += xv; by += yv; oy = y1; break;
			default: bx += xv*(y1-oy); by += yv*(y1-oy); oy = y1; break;
		}

		p = ylookup[y1]+x+frameplace;

		if ((dastat&1) == 0)
		{
			if (dastat&64)
				spritevline(bx&65535,by&65535,y2-y1+1,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			else
				mspritevline(bx&65535,by&65535,y2-y1+1,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
		}
		else
		{
			tspritevline(bx&65535,by&65535,y2-y1+1,(void *)((bx>>16)*ysiz+(by>>16)+bufplc),(void *)p);
			//transarea += (y2-y1);
		}
		faketimerhandler();
	}

#endif
}

//
// dosetaspect
//
void dosetaspect()
{
	if (xyaspect != oxyaspect)
	{
		oxyaspect = xyaspect;
		const int j = xyaspect*320;
		horizlookup2[horizycent-1] = divscalen<26>(131072, j);
		for(int i = ydim * 4 - 1; i >= 0; --i) {
			if (i != (horizycent-1))
			{
				horizlookup[i] = divscalen<28>(1, i - (horizycent - 1));
				horizlookup2[i] = divscalen<14>(std::abs(horizlookup[i]), j);
			}
		}
	}

	if ((xdimen != oxdimen) || (viewingrange != oviewingrange))
	{
		oxdimen = xdimen;
		oviewingrange = viewingrange;
		const int xinc = mulscalen<32>(viewingrange*320,xdimenrecip);
		int x = (640 << 16) - mulscalen<1>(xinc, xdimen);
		
		for(int i{0}; i < xdimen; ++i)
		{
			int j = (x & 65535);
			const int k = (x >> 16);
			x += xinc;
			
			if (j != 0)
				j = mulscalen<16>((int)radarang[k + 1] - (int)radarang[k], j);

			radarang2[i] = (short)((static_cast<int>(radarang[k]) + j) >> 6);
		}

		std::ranges::generate(std::ranges::next(distrecip.begin(), 1), distrecip.end(), [i = 1]() mutable {
			return divscalen<20>(xdimen, (i++));
		});

		nytooclose = xdimen * 2100;
	}
}


//
// loadtables (internal)
//
void calcbritable()
{
	for (int i{0}; i < 16; ++i) {
		const double a = 8.0 / (static_cast<double>(i) + 8.0);
		const double b = 255.0 / std::pow(255.0, a);

		for (int j{0}; j < 256; ++j) // JBF 20040207: full 8bit precision
			britable[i][j] = static_cast<unsigned char>(std::pow(static_cast<double>(j), a) * b);
	}
}

bool loadtables()
{
	std::ranges::generate(sintable, [n = 0]() mutable {
        	return static_cast<short>(16384 * std::sin(static_cast<double>(n++) * std::numbers::pi_v<double> / 1024));
		});

	// TODO: Make this table as a constexpr array.
    for(int i{0}; i < 640; i++) {
        radarang[i] = (short)(atan(((double)i-639.5)/160)*64*1024/std::numbers::pi_v<double>);
        radarang[1279 - i] = -radarang[i];
    }
	calcbritable();

    if (crc32once((unsigned char *)&sintable[0], sizeof(sintable)) != 0xee1e7aba) {
        engineerrstr = "Calculation of sintable yielded unexpected results.";
        return false;
    }
    if (crc32once((unsigned char *)&radarang[0], sizeof(radarang) / 2) != 0xee893d92) {
        engineerrstr = "Calculation of radarang yielded unexpected results.";
        return false;
    }

	return true;
}


//
// initfastcolorlookup (internal)
//
void initfastcolorlookup(int rscale, int gscale, int bscale)
{
	for(int i{64}, j{0}; i >= 0; i--)
	{
		//j = (i-64)*(i-64);
		rdist[i] = rdist[128 - i] = j * rscale;
		gdist[i] = gdist[128 - i] = j * gscale;
		bdist[i] = bdist[128 - i] = j * bscale;
		j += 129 - (i << 1);
	}

	std::ranges::fill(colhere, 0);
	std::ranges::fill(colhead, 0);

	unsigned char* pal1 = &palette[768-3];
	for(int i{255}; i >= 0; --i, pal1 -= 3)
	{
		const int j = (pal1[0]>>3)*FASTPALGRIDSIZ*FASTPALGRIDSIZ+(pal1[1]>>3)*FASTPALGRIDSIZ+(pal1[2]>>3)+FASTPALGRIDSIZ*FASTPALGRIDSIZ+FASTPALGRIDSIZ+1;
		
		if (colhere[j >> 3] & pow2char[j & 7])
			colnext[i] = colhead[j];
		else
			colnext[i] = -1;

		colhead[j] = i;
		colhere[j >> 3] |= pow2char[j & 7];
	}

	int i{0};
	for(int x = -FASTPALGRIDSIZ * FASTPALGRIDSIZ; x <= FASTPALGRIDSIZ * FASTPALGRIDSIZ; x += FASTPALGRIDSIZ * FASTPALGRIDSIZ) {
		for(int y =- FASTPALGRIDSIZ; y <= FASTPALGRIDSIZ; y += FASTPALGRIDSIZ) {
			for(int z{-1}; z <= 1; ++z) {
				colscan[i++] = x + y + z;
			}
		}
	}

	std::swap(colscan[13], colscan[26]);
}


//
// loadpalette (internal)
//
bool loadpalette()
{
	int fil{-1};

	off_t flen{0};

	const auto badpalette = [](const auto& fil) {
		engineerrstr = "Failed to load PALETTE.DAT!";
		if (fil >= 0)
			kclose(fil);

		return false;
	};

	if ((fil = kopen4load("palette.dat",0)) < 0)
		return badpalette(fil);
	
	flen = kfilelength(fil);

	if (kread(fil, &palette[0], 768) != 768) {
		buildputs("loadpalette: truncated palette\n");
		return badpalette(fil);
	}

	if ((flen - 65536 - 2 - 768) % 256 == 0) {
		// Map format 7+ palette.
		if (kread(fil,&numpalookups,2) != 2) {
			buildputs("loadpalette: read error\n");
			return badpalette(fil);
		}
	}
	else if ((flen - 32640 - 768) % 256 == 0) {
		// Old format palette.
		numpalookups = (flen - 32640 - 768) >> 8;
		buildprintf("loadpalette: old format palette ({} shades)\n",
			numpalookups);
		return badpalette(fil);
	} else {
		buildprintf("loadpalette: damaged palette\n");
		return badpalette(fil);
	}

	palookup[0].resize(numpalookups << 8);

	globalpalwritten = palookup[0].data();
	globalpal = 0;
	setpalookupaddress(globalpalwritten);

	fixtransluscence(transluc.data());

	kread(fil, palookup[globalpal].data(), numpalookups << 8);
	kread(fil, transluc.data(), 65536);
	kclose(fil);

	// FIXME: Make constexpr initialization.
	initfastcolorlookup(30L,59L,11L);

	return true;	
}

} // namespace

//
// getclosestcol
//
int getclosestcol(int r, int g, int b)
{
	const int j = (r>>3) * FASTPALGRIDSIZ * FASTPALGRIDSIZ +
	              (g>>3) * FASTPALGRIDSIZ +
				  (b>>3) + FASTPALGRIDSIZ * FASTPALGRIDSIZ + FASTPALGRIDSIZ + 1;
	int mindist = std::min(rdist[coldist[r & 7] + 64 + 8], gdist[coldist[g & 7] + 64 + 8]);
	mindist = std::min(mindist, bdist[coldist[b & 7] + 64 + 8]);
	++mindist;

	r = 64 - r;
	g = 64 - g;
	b = 64 - b;

	int retcol{-1};

	for(int k{26}; k >= 0; --k)
	{
		int i = colscan[k] + j;
		if ((colhere[i >> 3] & pow2char[i & 7]) == 0)
			continue;
		i = colhead[i];
		
		do
		{
			auto* pal1 = &palette[i * 3];
			int dist = gdist[pal1[1] + g];
			if (dist < mindist)
			{
				dist += rdist[pal1[0] + r];
				if (dist < mindist)
				{
					dist += bdist[pal1[2]+b];
					if (dist < mindist) {
						mindist = dist;
						retcol = i;
					}
				}
			}

			i = colnext[i];
		} while (i >= 0);
	}
	
	if (retcol >= 0)
		return retcol;

	mindist = 0x7fffffff;
	auto* pal1 = &palette[768-3];
	
	for(int i{255}; i >= 0; --i, pal1 -= 3)
	{
		int dist = gdist[pal1[1]  + g];
		if (dist >= mindist)
			continue;
		dist += rdist[pal1[0] + r];
		if (dist >= mindist)
			continue;
		dist += bdist[pal1[2] + b];
		if (dist >= mindist)
			continue;
		mindist = dist;
		retcol = i;
	}

	return retcol;
}

namespace {

//
// insertspritesect (internal)
//
int insertspritesect(short sectnum)
{
	if ((sectnum >= MAXSECTORS) || (headspritesect[MAXSECTORS] == -1))
		return -1;  //list full

	const short blanktouse = headspritesect[MAXSECTORS];

	headspritesect[MAXSECTORS] = nextspritesect[blanktouse];
	if (headspritesect[MAXSECTORS] >= 0)
		prevspritesect[headspritesect[MAXSECTORS]] = -1;

	prevspritesect[blanktouse] = -1;
	nextspritesect[blanktouse] = headspritesect[sectnum];
	if (headspritesect[sectnum] >= 0)
		prevspritesect[headspritesect[sectnum]] = blanktouse;
	headspritesect[sectnum] = blanktouse;

	sprite[blanktouse].sectnum = sectnum;

	return blanktouse;
}


//
// insertspritestat (internal)
//
int insertspritestat(short statnum)
{
	if ((statnum >= MAXSTATUS) || (headspritestat[MAXSTATUS] == -1))
		return -1;  //list full

	const short blanktouse = headspritestat[MAXSTATUS];

	headspritestat[MAXSTATUS] = nextspritestat[blanktouse];
	if (headspritestat[MAXSTATUS] >= 0)
		prevspritestat[headspritestat[MAXSTATUS]] = -1;

	prevspritestat[blanktouse] = -1;
	nextspritestat[blanktouse] = headspritestat[statnum];
	if (headspritestat[statnum] >= 0)
		prevspritestat[headspritestat[statnum]] = blanktouse;
	headspritestat[statnum] = blanktouse;

	sprite[blanktouse].statnum = statnum;

	return(blanktouse);
}


//
// deletespritesect (internal)
//
int deletespritesect(short deleteme)
{
	if (sprite[deleteme].sectnum == MAXSECTORS)
		return(-1);

	if (headspritesect[sprite[deleteme].sectnum] == deleteme)
		headspritesect[sprite[deleteme].sectnum] = nextspritesect[deleteme];

	if (prevspritesect[deleteme] >= 0) nextspritesect[prevspritesect[deleteme]] = nextspritesect[deleteme];
	if (nextspritesect[deleteme] >= 0) prevspritesect[nextspritesect[deleteme]] = prevspritesect[deleteme];

	if (headspritesect[MAXSECTORS] >= 0) prevspritesect[headspritesect[MAXSECTORS]] = deleteme;
	prevspritesect[deleteme] = -1;
	nextspritesect[deleteme] = headspritesect[MAXSECTORS];
	headspritesect[MAXSECTORS] = deleteme;

	sprite[deleteme].sectnum = MAXSECTORS;
	return(0);
}


//
// deletespritestat (internal)
//
int deletespritestat(short deleteme)
{
	if (sprite[deleteme].statnum == MAXSTATUS)
		return -1;

	if (headspritestat[sprite[deleteme].statnum] == deleteme)
		headspritestat[sprite[deleteme].statnum] = nextspritestat[deleteme];

	if (prevspritestat[deleteme] >= 0) nextspritestat[prevspritestat[deleteme]] = nextspritestat[deleteme];
	if (nextspritestat[deleteme] >= 0) prevspritestat[nextspritestat[deleteme]] = prevspritestat[deleteme];

	if (headspritestat[MAXSTATUS] >= 0) prevspritestat[headspritestat[MAXSTATUS]] = deleteme;
	prevspritestat[deleteme] = -1;
	nextspritestat[deleteme] = headspritestat[MAXSTATUS];
	headspritestat[MAXSTATUS] = deleteme;

	sprite[deleteme].statnum = MAXSTATUS;
	return 0;
}


//
// lintersect (internal)
//
bool lintersect(int x1, int y1, int z1, int x2, int y2, int z2, int x3,
		  int y3, int x4, int y4, int *intx, int *inty, int *intz)
{
	//p1 to p2 is a line segment
	const int x21 = x2 - x1;
	const int x34 = x3 - x4;
	const int y21 = y2 - y1;
	const int y34 = y3 - y4;
	const int bot = x21 * y34 - y21 * x34;

	int topt{0};

	if (bot >= 0)
	{
		if (bot == 0) {
			return false;
		}

		const int x31 = x3 - x1;
		const int y31 = y3 - y1;

		topt = x31 * y34 - y31 * x34;

		if ((topt < 0) || (topt >= bot)) {
			return false;
		}

		const int topu = x21 * y31 - y21 * x31;

		if ((topu < 0) || (topu >= bot)) {
			return false;
		}
	}
	else
	{
		const int x31 = x3 - x1;
		const int y31 = y3 - y1;
		topt = x31 * y34 - y31 * x34;

		if ((topt > 0) || (topt <= bot)) {
			return false;
		}
		
		const int topu = x21 * y31 - y21 * x31;
		
		if ((topu > 0) || (topu <= bot)) {
			return false;
		}
	}

	const int t = divscalen<24>(topt, bot);

	*intx = x1 + mulscalen<24>(x21, t);
	*inty = y1 + mulscalen<24>(y21, t);
	*intz = z1 + mulscalen<24>(z2 - z1, t);
	
	return true;
}


//
// rintersect (internal)
//
bool rintersect(int x1, int y1, int z1, int vx, int vy, int vz, int x3,
		  int y3, int x4, int y4, int *intx, int *inty, int *intz)
{     //p1 towards p2 is a ray
	const int x34 = x3 - x4;
	const int y34 = y3 - y4;
	const int bot = vx * y34 - vy * x34;

	int topt{0};

	if (bot >= 0)
	{
		if (bot == 0)
			return false;
		
		const int x31 = x3 - x1;
		const int y31 = y3 - y1;
		topt = x31 * y34 - y31 * x34;
		
		if (topt < 0)
			return false;

		const int topu = vx * y31 - vy * x31;
		
		if ((topu < 0) || (topu >= bot))
			return false;
	}
	else
	{
		const int x31 = x3 - x1;
		const int y31 = y3 - y1;
		topt = x31 * y34 - y31 * x34;
		
		if (topt > 0)
			return false;

		const int topu = vx * y31 - vy * x31;
		
		if ((topu > 0) || (topu <= bot))
			return false;
	}

	const int t = divscalen<16>(topt, bot);
	*intx = x1 + mulscalen<16>(vx, t);
	*inty = y1 + mulscalen<16>(vy, t);
	*intz = z1 + mulscalen<16>(vz, t);

	return true;
}


//
// keepaway (internal)
//
void keepaway (int *x, int *y, int w)
{
	const int x1 = clipit[w].x1;
	const int dx = clipit[w].x2 - x1;
	const int y1 = clipit[w].y1;
	const int dy = clipit[w].y2 - y1;
	const int ox = ksgn(-dy);
	const int oy = ksgn(dx);
	char first = (std::abs(dx) <= std::abs(dy));

	while (1)
	{
		if (dx * (*y - y1) > (*x - x1) * dy)
			return;

		if (first == 0)
			*x += ox;
		else
			*y += oy;

		first ^= 1;
	}
}


//
// raytrace (internal)
//
int raytrace(int x3, int y3, int *x4, int *y4)
{
	int x1;
	int y1;
	int x2;
	int y2;
	int bot;
	int topu;
	int nintx;
	int ninty;
	int cnt;
	int z;
	int x21;
	int y21;
	int x43;
	int y43;

	int hitwall{ -1 };

	for(z=clipnum-1;z>=0;z--)
	{
		x1 = clipit[z].x1;
		x2 = clipit[z].x2;
		x21 = x2 - x1;
		y1 = clipit[z].y1;
		y2 = clipit[z].y2;
		y21 = y2 - y1;

		topu = x21*(y3-y1) - (x3-x1)*y21; if (topu <= 0) continue;
		if (x21*(*y4-y1) > (*x4-x1)*y21) continue;
		x43 = *x4-x3; y43 = *y4-y3;
		if (x43*(y1-y3) > (x1-x3)*y43) continue;
		if (x43*(y2-y3) <= (x2-x3)*y43) continue;
		bot = x43*y21 - x21*y43; if (bot == 0) continue;

		cnt = 256;
		do
		{
			cnt--; if (cnt < 0) { *x4 = x3; *y4 = y3; return(z); }
			nintx = x3 + scale(x43,topu,bot);
			ninty = y3 + scale(y43,topu,bot);
			topu--;
		} while (x21*(ninty-y1) <= (nintx-x1)*y21);

		if (std::abs(x3-nintx)+std::abs(y3-ninty) < std::abs(x3-*x4)+std::abs(y3-*y4)) {
			*x4 = nintx;
			*y4 = ninty;
			hitwall = z;
		}
	}
	return hitwall;
}



//
// Exported Engine Functions
//

#if !defined _WIN32 && defined DEBUGGINGAIDS
void sighandler(int sig, siginfo_t *info, void *ctx)
{
	const char *s;

	(void)ctx;

	switch (sig) {
		case SIGFPE:
			switch (info->si_code) {
				case FPE_INTDIV: s = "FPE_INTDIV (integer divide by zero)"; break;
				case FPE_INTOVF: s = "FPE_INTOVF (integer overflow)"; break;
				case FPE_FLTDIV: s = "FPE_FLTDIV (floating-point divide by zero)"; break;
				case FPE_FLTOVF: s = "FPE_FLTOVF (floating-point overflow)"; break;
				case FPE_FLTUND: s = "FPE_FLTUND (floating-point underflow)"; break;
				case FPE_FLTRES: s = "FPE_FLTRES (floating-point inexact result)"; break;
				case FPE_FLTINV: s = "FPE_FLTINV (floating-point invalid operation)"; break;
				case FPE_FLTSUB: s = "FPE_FLTSUB (floating-point subscript out of range)"; break;
				default: s = "?! (unknown)"; break;
			}
			fmt::print(stderr, "Caught SIGFPE at address {}, code {}. Aborting.\n", info->si_addr, s);
			break;
		default: break;
	}
	abort();
}
#endif

//
// preinitengine
//
int preinitcalled = 0;

} // namespace

int preinitengine()
{
#if defined(_MSC_VER)
	auto compstr = fmt::format("MS Visual C++ {}.{:02d}", _MSC_VER / 100, _MSC_VER % 100);
#elif defined(__clang__)
	auto compstr = fmt::format("Clang {}.{}", __clang_major__, __clang_minor__);
#elif defined(__GNUC__)
	auto compstr = fmt::format("GCC {}.{}", __GNUC__, __GNUC_MINOR__);
#else // Unidentified compiler
	auto compstr = "an unidentified compiler"
#endif

	buildprintf("\nBUILD engine by Ken Silverman (http://www.advsys.net/ken)\n"
	       "Additional improvements by Jonathon Fowler (http://www.jonof.id.au)\n"
	       "and other contributors. See BUILDLIC.TXT for terms.\n\n"
	       "Version {}.\nBuilt {} {} using {}.\n{}-bit word size.\n\n",
	       build_version, build_date, build_time, compstr, (int)(sizeof(intptr_t)<<3));

	// Detect anomalous structure packing.
	assert(sizeof(sectortype) == 40);
	assert((intptr_t)&g_sector[1] - (intptr_t)&g_sector[0] == sizeof(sectortype));
	assert(sizeof(walltype) == 32);
	assert((intptr_t)&wall[1] - (intptr_t)&wall[0] == sizeof(walltype));
	assert(sizeof(spritetype) == 44);
	assert((intptr_t)&sprite[1] - (intptr_t)&sprite[0] == sizeof(spritetype));
	assert(sizeof(spriteexttype) == 12);
	assert((intptr_t)&spriteext[1] - (intptr_t)&spriteext[0] == sizeof(spriteexttype));

	if (initsystem())
		std::exit(1);

#ifndef USING_A_C
	makeasmwriteable();

	if (std::getenv("BUILD_NOP6")) {
		buildprintf("Disabling P6 optimizations.\n");
		dommxoverlay = false;
	}
	if (dommxoverlay)
		mmxoverlay();
#endif

	getvalidmodes();

	preinitcalled = 1;
	return 0;
}


//
// initengine
//
bool initengine()
{
#if !defined _WIN32 && defined DEBUGGINGAIDS
	struct sigaction sigact, oldact;
	std::memset(&sigact, 0, sizeof(sigact));
	sigact.sa_sigaction = sighandler;
	sigact.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sigact, &oldact);
#endif
	if (!preinitcalled) {
		const int i = preinitengine();

		if (i != 0)
			return false;
	}

	pskyoff[0] = 0;

	std::ranges::fill(&voxlock[0][0], &voxlock[0][0] + sizeof(voxlock) / sizeof(voxlock[0][0]), 200);
	std::ranges::fill(tiletovox, -1);
	std::ranges::fill(voxscale, 65536L);

	visibility = 512;

	captureformat = 2;  // PNG

	if (!loadtables())
		return false;

	if (!loadpalette())
		return false;

#if USE_POLYMOST
	polymost_initosdfuncs();
#endif
#if USE_POLYMOST && USE_OPENGL
	if (!hicfirstinit)
		hicinit();
	
	if (!mdinited)
		mdinit();
	
	PTCacheLoadIndex();
#endif

	return true;
}


//
// uninitengine
//
void uninitengine()
{
	//buildprintf("cacheresets = {}, cacheinvalidates = {}\n", cacheresets, cacheinvalidates);

#if USE_POLYMOST && USE_OPENGL
	polymost_glreset();
	PTClear();
	PTCacheUnloadIndex();
	hicinit();
	freeallmodels();
#endif

	uninitsystem();

	if (logfile) {
		std::fclose(logfile);
	}

	logfile = nullptr;

	if (artfil != -1) {
		kclose(artfil);
	}

	if (pic != nullptr) {
		std::free(pic);
		pic = nullptr;
	}
}


//
// initspritelists
//
void initspritelists()
{
	//Init doubly-linked sprite g_sector lists
	// FIXME: Initialize with constexpr.
	std::ranges::fill(headspritesect, -1);
	headspritesect[MAXSECTORS] = 0;

	// FIXME: Initialize with constexpr.
	std::iota(prevspritesect.begin(), prevspritesect.end(), -1);
	std::iota(nextspritesect.begin(), nextspritesect.end(), 1);

	for(auto& aSprite : sprite) {
		aSprite.sectnum = MAXSECTORS;
	}

	prevspritesect[0] = -1;
	nextspritesect[MAXSPRITES-1] = -1;

	std::ranges::fill(headspritestat, -1);  //Init doubly-linked sprite status lists
	headspritestat[MAXSTATUS] = 0;

	std::iota(prevspritestat.begin(), prevspritestat.end(), -1);
	std::iota(nextspritestat.begin(), nextspritestat.end(),  1);
	
	for(auto& aSprite : sprite) {
		aSprite.statnum = MAXSTATUS;
	}

	prevspritestat[0] = -1;
	nextspritestat[MAXSPRITES-1] = -1;
}


//
// drawrooms
//
void drawrooms(int daposx, int daposy, int daposz,
		 short daang, int dahoriz, short dacursectnum)
{
	int i;
	int j;
	int z;
	int cz;
	int fz;
	int closest;
	short *shortptr1, *shortptr2;

#if defined(DEBUGGINGAIDS)
	if (numscans > MAXWALLSB)
		debugprintf("damage report: numscans {} exceeded {}\n", numscans, MAXWALLSB);
	if (numbunches > MAXWALLSB)
		debugprintf("damage report: numbunches {} exceeded {}\n", numbunches, MAXWALLSB);
	if (maskwallcnt > MAXWALLSB)
		debugprintf("damage report: maskwallcnt {} exceeded {}\n", maskwallcnt, MAXWALLSB);
	if (smostwallcnt > MAXWALLSB)
		debugprintf("damage report: smostwallcnt {} exceeded {}\n", smostwallcnt, MAXWALLSB);
#endif

	beforedrawrooms = false;

	globalpos.x = daposx;
	globalpos.y = daposy;
	globalpos.z = daposz;
	globalang = (daang&2047);

	globalhoriz = mulscalen<16>(dahoriz-100,xdimenscale)+(ydimen>>1);
	globaluclip = (0-globalhoriz)*xdimscale;
	globaldclip = (ydimen-globalhoriz)*xdimscale;

	i = mulscalen<16>(xdimenscale,viewingrangerecip);
	globalpisibility = mulscalen<16>(parallaxvisibility,i);
	globalvisibility = mulscalen<16>(visibility,i);
	globalhisibility = mulscalen<16>(globalvisibility,xyaspect);
	globalcisibility = mulscalen<8>(globalhisibility,320);

	globalcursectnum = dacursectnum;
	totalclocklock = totalclock;

	cosglobalang = sintable[(globalang+512)&2047];
	singlobalang = sintable[globalang&2047];
	cosviewingrangeglobalang = mulscalen<16>(cosglobalang,viewingrange);
	sinviewingrangeglobalang = mulscalen<16>(singlobalang,viewingrange);

	if ((xyaspect != oxyaspect) || (xdimen != oxdimen) || (viewingrange != oviewingrange))
		dosetaspect();

	//clearbufbyte(&gotsector[0],(int)((numsectors+7)>>3),0L);
	std::memset(&gotsector[0],0,(int)((numsectors+7)>>3));

	shortptr1 = (short *)&startumost[windowx1];
	shortptr2 = (short *)&startdmost[windowx1];
	i = xdimen-1;
	do
	{
		umost[i] = shortptr1[i]-windowy1;
		dmost[i] = shortptr2[i]-windowy1;
		i--;
	} while (i != 0);
	umost[0] = shortptr1[0]-windowy1;
	dmost[0] = shortptr2[0]-windowy1;

	//============================================================================= //POLYMOST BEGINS
#if USE_POLYMOST
	polymost_drawrooms();
	if (rendmode != rendmode_t::Classic) {
		return;
	}
#endif
	//============================================================================= //POLYMOST ENDS

	frameoffset = frameplace + windowy1 * bytesperline + windowx1;

	numhits = xdimen;
	numscans = 0;
	numbunches = 0;
	maskwallcnt = 0;
	smostwallcnt = 0;
	smostcnt = 0;
	spritesortcnt = 0;

	if (globalcursectnum >= MAXSECTORS) {
		globalcursectnum -= MAXSECTORS;
	}
	else {
		i = globalcursectnum;
		updatesector(globalpos.x,globalpos.y,&globalcursectnum);
		if (globalcursectnum < 0) globalcursectnum = i;
	}

	globparaceilclip = true;
	globparaflorclip = true;

	auto cfz = getzsofslope(globalcursectnum,globalpos.x,globalpos.y);
	if (globalpos.z < cfz.ceilz)
		globparaceilclip = false;

	if (globalpos.z > cfz.floorz)
		globparaflorclip = false;

	scansector(globalcursectnum);

	if (inpreparemirror)
	{
		inpreparemirror = false;
		mirrorsx1 = xdimen-1;
		mirrorsx2 = 0;

		for(i=numscans-1;i>=0;i--)
		{
			if (wall[thewall[i]].nextsector < 0)
				continue;
			
			if (xb1[i] < mirrorsx1)
				mirrorsx1 = xb1[i];
			
			if (xb2[i] > mirrorsx2)
				mirrorsx2 = xb2[i];
		}

		for(i=0;i<mirrorsx1;i++)
			if (umost[i] <= dmost[i])
				{ umost[i] = 1; dmost[i] = 0; numhits--; }
		for(i=mirrorsx2+1;i<xdimen;i++)
			if (umost[i] <= dmost[i])
				{ umost[i] = 1; dmost[i] = 0; numhits--; }

		drawalls(0L);
		numbunches--;
		bunchfirst[0] = bunchfirst[numbunches];
		bunchlast[0] = bunchlast[numbunches];

		mirrorsy1 = std::min(umost[mirrorsx1], umost[mirrorsx2]);
		mirrorsy2 = std::max(dmost[mirrorsx1], dmost[mirrorsx2]);
	}

	while ((numbunches > 0) && (numhits > 0))
	{
		clearbuf(&tempbuf[0], (int)((numbunches+3)>>2),0L);
		tempbuf[0] = 1;

		closest = 0;              //Almost works, but not quite :(
		for(i=1;i<numbunches;i++)
		{
			if ((j = bunchfront(i,closest)) < 0) continue;
			tempbuf[i] = 1;
			if (j == 0) tempbuf[closest] = 1, closest = i;
		}
		for(i=0;i<numbunches;i++) //Double-check
		{
			if (tempbuf[i]) continue;
			if ((j = bunchfront(i,closest)) < 0) continue;
			tempbuf[i] = 1;
			if (j == 0) tempbuf[closest] = 1, closest = i, i = 0;
		}

		drawalls(closest);

		if (automapping)
		{
			for(z=bunchfirst[closest];z>=0;z=p2[z])
				show2dwall[thewall[z]>>3] |= pow2char[thewall[z] & 7];
		}

		numbunches--;
		bunchfirst[closest] = bunchfirst[numbunches];
		bunchlast[closest] = bunchlast[numbunches];
	}
}


//
// drawmasks
//
void drawmasks()
{
	int i;
	int j;
	int k;
	int l;
	int gap;
	int xs;
	int ys;
	int xp;
	int yp;
	int yoff;
	int yspan;

	for(i=spritesortcnt-1;i>=0;i--)
		tspriteptr[i] = &tsprite[i];
	
	for(i=spritesortcnt-1;i>=0;i--) {
		xs = tspriteptr[i]->x-globalpos.x;
		ys = tspriteptr[i]->y-globalpos.y;
		yp = dmulscalen<6>(xs,cosviewingrangeglobalang,ys,sinviewingrangeglobalang);
		
		if (yp > (4<<8)) {
			xp = dmulscalen<6>(ys,cosglobalang,-xs,singlobalang);
			if (mulscalen<24>(abs(xp+yp),xdimen) >= yp) goto killsprite;
			spritesx[i] = scale(xp+yp,xdimen<<7,yp);
		}
		else if ((tspriteptr[i]->cstat&48) == 0) {
killsprite:
			spritesortcnt--;  //Delete face sprite if on wrong side!
			if (i != spritesortcnt)
			{
				tspriteptr[i] = tspriteptr[spritesortcnt];
				spritesx[i] = spritesx[spritesortcnt];
				spritesy[i] = spritesy[spritesortcnt];
			}
			continue;
		}
		spritesy[i] = yp;
	}

	gap = 1; while (gap < spritesortcnt) gap = (gap<<1)+1;
	for(gap>>=1;gap>0;gap>>=1)      //Sort sprite list
		for(i=0;i<spritesortcnt-gap;i++)
			for(l=i;l>=0;l-=gap)
			{
				if (spritesy[l] <= spritesy[l+gap])
					break;
				
				std::swap(tspriteptr[l], tspriteptr[l + gap]);
				std::swap(spritesx[l], spritesx[l + gap]);
				std::swap(spritesy[l], spritesy[l + gap]);
			}

	if (spritesortcnt > 0)
		spritesy[spritesortcnt] = (spritesy[spritesortcnt-1]^1);

	ys = spritesy[0]; i = 0;
	for(j=1;j<=spritesortcnt;j++)
	{
		if (spritesy[j] == ys)
			continue;
		
		ys = spritesy[j];
		
		if (j > i+1) {
			for(k=i;k<j;k++) {
				spritesz[k] = tspriteptr[k]->z;
				if ((tspriteptr[k]->cstat&48) != 32)
				{
					yoff = (int)((signed char)((picanm[tspriteptr[k]->picnum]>>16)&255))+((int)tspriteptr[k]->yoffset);
					spritesz[k] -= ((yoff*tspriteptr[k]->yrepeat)<<2);
					yspan = (tilesizy[tspriteptr[k]->picnum]*tspriteptr[k]->yrepeat<<2);
					if (!(tspriteptr[k]->cstat&128)) spritesz[k] -= (yspan>>1);
					if (std::abs(spritesz[k]-globalpos.z) < (yspan>>1)) spritesz[k] = globalpos.z;
				}
			}
			for(k=i+1;k<j;k++)
				for(l=i;l<k;l++)
					if (std::abs(spritesz[k]-globalpos.z) < std::abs(spritesz[l]-globalpos.z))
					{
						std::swap(tspriteptr[k], tspriteptr[l]);
						std::swap(spritesx[k], spritesx[l]);
						std::swap(spritesy[k], spritesy[l]);
						std::swap(spritesz[k], spritesz[l]);
					}
			for(k=i+1;k<j;k++)
				for(l=i;l<k;l++)
					if (tspriteptr[k]->statnum < tspriteptr[l]->statnum)
					{
						std::swap(tspriteptr[k], tspriteptr[l]);
						std::swap(spritesx[k], spritesx[l]);
						std::swap(spritesy[k], spritesy[l]);
					}
		}
		i = j;
	}

	/*for(i=spritesortcnt-1;i>=0;i--)
	{
		xs = tspriteptr[i].x-globalpos.x;
		ys = tspriteptr[i].y-globalpos.y;
		zs = tspriteptr[i].z-globalpos.z;

		xp = ys*cosglobalang-xs*singlobalang;
		yp = (zs<<1);
		zp = xs*cosglobalang+ys*singlobalang;

		xs = scale(xp,halfxdimen<<12,zp)+((halfxdimen+windowx1)<<12);
		ys = scale(yp,xdimenscale<<12,zp)+((globalhoriz+windowy1)<<12);

		drawline256(xs-65536,ys-65536,xs+65536,ys+65536,31);
		drawline256(xs+65536,ys-65536,xs-65536,ys+65536,31);
	}*/

#if USE_POLYMOST
		//Hack to make it draw all opaque quads first. This should reduce the chances of
		//bad sorting causing transparent quads knocking out opaque quads behind it.
		//
		//Need to store alpha flag with all textures before this works right!
	if (rendmode != rendmode_t::Classic)
	{
		for(i=spritesortcnt-1;i>=0;i--)
			if ((!(tspriteptr[i]->cstat&2))
#if USE_OPENGL
			    && (!polymost_texmayhavealpha(tspriteptr[i]->picnum,tspriteptr[i]->pal))
#endif
			   )
				{ drawsprite(i); tspriteptr[i] = nullptr; } //draw only if it is fully opaque
		for(i=j=0;i<spritesortcnt;i++)
		{
			if (!tspriteptr[i])
				continue;
			
			tspriteptr[j] = tspriteptr[i];
			spritesx[j] = spritesx[i];
			spritesy[j] = spritesy[i];
			j++;
		}

		spritesortcnt = j;

		for(i=maskwallcnt-1;i>=0;i--)
		{
			k = thewall[maskwall[i]];
			if ((!(wall[k].cstat&128))
#if USE_OPENGL
			    && (!polymost_texmayhavealpha(wall[k].overpicnum,wall[k].pal))
#endif
			   )
				{ drawmaskwall(i); maskwall[i] = -1; } //draw only if it is fully opaque
		}
		for(i=j=0;i<maskwallcnt;i++)
		{
			if (maskwall[i] < 0) continue;
			maskwall[j++] = maskwall[i];
		}
		maskwallcnt = j;
	}
#endif

	while ((spritesortcnt > 0) && (maskwallcnt > 0))  //While BOTH > 0
	{
		j = maskwall[maskwallcnt-1];
		if (spritewallfront(tspriteptr[spritesortcnt - 1], (int) thewall[j]) == 0)
			drawsprite(--spritesortcnt);
		else
		{
				//Check to see if any sprites behind the masked wall...
			k = -1;
			gap = 0;
			for(i=spritesortcnt-2;i>=0;i--)
			{
#if USE_POLYMOST
				if (rendmode != rendmode_t::Classic)
					l = dxb1[j] <= (double)spritesx[i]/256.0 && (double)spritesx[i]/256.0 <= dxb2[j];
				else
#endif
					l = xb1[j] <= (spritesx[i]>>8) && (spritesx[i]>>8) <= xb2[j];
				if (l && spritewallfront(tspriteptr[i], (int) thewall[j]) == 0)
				{
					drawsprite(i);
					tspriteptr[i]->owner = -1;
					k = i;
					gap++;
				}
			}
			if (k >= 0)       //remove holes in sprite list
			{
				for(i=k;i<spritesortcnt;i++)
					if (tspriteptr[i]->owner >= 0)
					{
						if (i > k)
						{
							tspriteptr[k] = tspriteptr[i];
							spritesx[k] = spritesx[i];
							spritesy[k] = spritesy[i];
						}
						k++;
					}
				spritesortcnt -= gap;
			}

				//finally safe to draw the masked wall
			drawmaskwall(--maskwallcnt);
		}
	}
	while (spritesortcnt > 0) drawsprite(--spritesortcnt);
	while (maskwallcnt > 0) drawmaskwall(--maskwallcnt);
}


//
// drawmapview
//
void drawmapview(int dax, int day, int zoome, short ang)
{
	walltype *wal;
	sectortype *sec;
	spritetype *spr;
	int tilenum;
	int xoff;
	int yoff;
	int i;
	int j;
	int k;
	int l;
	int cosang;
	int sinang;
	int xspan;
	int yspan;
	int xrepeat;
	int yrepeat;
	int x;
	int y;
	int x1;
	int y1;
	int x2;
	int y2;
	int x3;
	int y3;
	int x4;
	int y4;
	int bakx1;
	int baky1;
	int s;
	int w;
	int ox;
	int oy;
	int startwall;
	int npoints;
	int daslope;

	beforedrawrooms = false;

	clearbuf(&gotsector[0],(int)((numsectors+31)>>5),0L);

	const int cx1 = windowx1 << 12;
	const int cy1 = windowy1 << 12;
	const int cx2 = ((windowx2 + 1) << 12) - 1;
	const int cy2 = ((windowy2 + 1) << 12) - 1;

	zoome <<= 8;

	const int bakgxvect = divscalen<28>(sintable[(1536-ang)&2047],zoome);
	const int bakgyvect = divscalen<28>(sintable[(2048-ang)&2047],zoome);
	const int xvect = mulscalen<8>(sintable[(2048-ang)&2047],zoome);
	const int yvect = mulscalen<8>(sintable[(1536-ang)&2047],zoome);
	const int xvect2 = mulscalen<16>(xvect,yxaspect);
	const int yvect2 = mulscalen<16>(yvect,yxaspect);

	int sortnum{ 0 };

	for(s=0,sec=&g_sector[s];s<numsectors;s++,sec++)
		if (show2dsector[s>>3] & pow2char[s & 7])
		{
			npoints = 0; i = 0;
			startwall = sec->wallptr;
#if 0
			for(w=sec->wallnum,wal=&wall[startwall];w>0;w--,wal++)
			{
				ox = wal->pt.x - dax; oy = wal->pt.y - day;
				x = dmulscalen<16>(ox,xvect,-oy,yvect) + (xdim<<11);
				y = dmulscalen<16>(oy,xvect2,ox,yvect2) + (ydim<<11);
				i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
				rx1[npoints] = x;
				ry1[npoints] = y;
				xb1[npoints] = wal->point2 - startwall;
				npoints++;
			}
#else
			j = startwall;
			l = 0;
			
			for(w=sec->wallnum,wal=&wall[startwall];w>0;w--,wal++,j++)
			{
				k = lastwall(j);
				if ((k > j) && (npoints > 0)) { xb1[npoints-1] = l; l = npoints; } //overwrite point2
					//wall[k].x wal->pt.x wall[wal->point2].x
					//wall[k].y wal->pt.y wall[wal->point2].y
				if (!dmulscalen<1>(wal->pt.x - wall[k].pt.x, wall[wal->point2].pt.y - wal->pt.y, -(wal->pt.y - wall[k].pt.y),wall[wal->point2].pt.x - wal->pt.x)) continue;
				ox = wal->pt.x - dax;
				oy = wal->pt.y - day;
				x = dmulscalen<16>(ox,xvect,-oy,yvect) + (xdim<<11);
				y = dmulscalen<16>(oy,xvect2,ox,yvect2) + (ydim<<11);
				i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
				rx1[npoints] = x;
				ry1[npoints] = y;
				xb1[npoints] = npoints+1;
				npoints++;
			}
			if (npoints > 0) xb1[npoints-1] = l; //overwrite point2
#endif
			if ((i & 0xf0) != 0xf0) {
				continue;
			}

			bakx1 = rx1[0];
			baky1 = mulscalen<16>(ry1[0] - (ydim << 11), xyaspect) + (ydim << 11);

			if (i & 0x0f)
			{
				npoints = clippoly(npoints, i);

				if (npoints < 3) {
					continue;
				}
			}

				//Collect floor sprites to draw
			for(i=headspritesect[s];i>=0;i=nextspritesect[i])
				if ((sprite[i].cstat&48) == 32)
				{
					if ((sprite[i].cstat&(64+8)) == (64+8)) continue;
					tsprite[sortnum++].owner = i;
				}

			gotsector[s >> 3] |= pow2char[s & 7];

			globalorientation = (int)sec->floorstat;
			if ((globalorientation&1) != 0) continue;

			globalpal = sec->floorpal;
			if (palookup[sec->floorpal].data() != globalpalwritten)
			{
				globalpalwritten = palookup[sec->floorpal].data();
				if (!globalpalwritten) globalpalwritten = palookup[0].data();	// JBF: fixes null-pointer crash
				setpalookupaddress(globalpalwritten);
			}
			globalpicnum = sec->floorpicnum;
			if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
			setgotpic(globalpicnum);
			if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0)) continue;
			if ((picanm[globalpicnum]&192) != 0) globalpicnum += animateoffs((short)globalpicnum,s);
			if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
			globalbufplc = waloff[globalpicnum];
			globalshade = std::max(std::min(static_cast<int>(sec->floorshade), static_cast<int>(numpalookups) - 1), 0);
			globvis = globalhisibility;
			if (sec->visibility != 0) globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));
			globalpolytype = 0;
			if ((globalorientation&64) == 0)
			{
				globalpos.x = dax;
				g_pt1 = {bakgxvect, bakgyvect};
				globalpos.y = day;
				g_pt2.x = bakgxvect;
				g_pt2.y = bakgyvect;
			}
			else
			{
				ox = wall[wall[startwall].point2].pt.x - wall[startwall].pt.x;
				oy = wall[wall[startwall].point2].pt.y - wall[startwall].pt.y;
				i = static_cast<int>(std::hypot(ox, oy)); 
				
				if (i == 0)
					continue;

				i = 1048576/i;
				g_pt1.x = mulscalen<10>(dmulscalen<10>(ox, bakgxvect, oy, bakgyvect), i);
				g_pt1.y = mulscalen<10>(dmulscalen<10>(ox, bakgyvect, -oy, bakgxvect), i);
				ox = (bakx1 >> 4) - (xdim << 7);
				oy = (baky1 >> 4) - (ydim << 7);
				globalpos.x = dmulscalen<28>(-oy,g_pt1.x,-ox,g_pt1.y);
				globalpos.y = dmulscalen<28>(-ox,g_pt1.x,oy,g_pt1.y);
				g_pt2 = -g_pt1;

				daslope = g_sector[s].floorheinum;
				i = static_cast<int>(std::sqrt(daslope * daslope+ 16777216)); // FIXME: Magic number.
				globalpos.y = mulscalen<12>(globalpos.y,i);
				g_pt2 = mulscalen<12>(g_pt2, i);
			}
			globalxshift = (8-(picsiz[globalpicnum]&15));
			globalyshift = (8-(picsiz[globalpicnum]>>4));
			if (globalorientation&8) {globalxshift++; globalyshift++; }

			sethlinesizes(picsiz[globalpicnum]&15,picsiz[globalpicnum]>>4,(void *)globalbufplc);

			if ((globalorientation&0x4) > 0)
			{
				i = globalpos.x;
				globalpos.x = -globalpos.y;
				globalpos.y = -i;
				std::swap(g_pt2.x, g_pt1.y);

				i = g_pt1.x;
				g_pt1.x = -g_pt2.y;
				g_pt2.y = -i;
			}
			if ((globalorientation&0x10) > 0) {
				g_pt1 = -g_pt1;
				globalpos.x = -globalpos.x;
			}

			if ((globalorientation&0x20) > 0) {
				g_pt2 = -g_pt2;
				globalpos.y = -globalpos.y;
			}

			asm1 = (g_pt1.y<<globalxshift);
			asm2 = (g_pt2.x<<globalyshift);
			g_pt1.x <<= globalxshift;
			g_pt2.y <<= globalyshift;
			globalpos.x = (globalpos.x<<(20+globalxshift))+(((int)sec->floorxpanning)<<24);
			globalpos.y = (globalpos.y<<(20+globalyshift))-(((int)sec->floorypanning)<<24);

			fillpolygon(npoints);
		}

		//Sort sprite list
	int gap{ 1 };

	while (gap < sortnum) {
		gap = (gap << 1) + 1;
	}

	for(gap>>=1;gap>0;gap>>=1) {
		for(i=0;i<sortnum-gap;i++) {
			for(j=i;j>=0;j-=gap) {
				if (sprite[tsprite[j].owner].z <= sprite[tsprite[j+gap].owner].z)
					break;
				
				std::swap(tsprite[j].owner, tsprite[j + gap].owner);
			}
		}
	}

	for(s=sortnum-1;s>=0;s--)
	{
		spr = &sprite[tsprite[s].owner];
		if ((spr->cstat&48) == 32)
		{
			npoints = 0;

			tilenum = spr->picnum;
			xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
			yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
			if ((spr->cstat&4) > 0) xoff = -xoff;
			if ((spr->cstat&8) > 0) yoff = -yoff;

			k = spr->ang;
			cosang = sintable[(k+512)&2047]; sinang = sintable[k];
			xspan = tilesizx[tilenum]; xrepeat = spr->xrepeat;
			yspan = tilesizy[tilenum]; yrepeat = spr->yrepeat;

			ox = ((xspan>>1)+xoff)*xrepeat; oy = ((yspan>>1)+yoff)*yrepeat;
			x1 = spr->x + mulscalen<16>(sinang,ox) + mulscalen<16>(cosang,oy);
			y1 = spr->y + mulscalen<16>(sinang,oy) - mulscalen<16>(cosang,ox);
			l = xspan*xrepeat;
			x2 = x1 - mulscalen<16>(sinang,l);
			y2 = y1 + mulscalen<16>(cosang,l);
			l = yspan*yrepeat;
			k = -mulscalen<16>(cosang,l);
			x3 = x2+k;
			x4 = x1+k;
			k = -mulscalen<16>(sinang,l);
			y3 = y2+k;
			y4 = y1+k;

			xb1[0] = 1;
			xb1[1] = 2;
			xb1[2] = 3;
			xb1[3] = 0;
			npoints = 4;

			i = 0;

			ox = x1 - dax;
			oy = y1 - day;
			x = dmulscalen<16>(ox,xvect,-oy,yvect) + (xdim<<11);
			y = dmulscalen<16>(oy,xvect2,ox,yvect2) + (ydim<<11);
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[0] = x; ry1[0] = y;

			ox = x2 - dax; oy = y2 - day;
			x = dmulscalen<16>(ox,xvect,-oy,yvect) + (xdim<<11);
			y = dmulscalen<16>(oy,xvect2,ox,yvect2) + (ydim<<11);
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[1] = x; ry1[1] = y;

			ox = x3 - dax; oy = y3 - day;
			x = dmulscalen<16>(ox,xvect,-oy,yvect) + (xdim<<11);
			y = dmulscalen<16>(oy,xvect2,ox,yvect2) + (ydim<<11);
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[2] = x;
			ry1[2] = y;

			x = rx1[0]+rx1[2]-rx1[1];
			y = ry1[0]+ry1[2]-ry1[1];
			i |= getclipmask(x-cx1,cx2-x,y-cy1,cy2-y);
			rx1[3] = x;
			ry1[3] = y;

			if ((i&0xf0) != 0xf0) continue;
			bakx1 = rx1[0];
			baky1 = mulscalen<16>(ry1[0]-(ydim<<11),xyaspect)+(ydim<<11);
			
			if (i&0x0f)
			{
				npoints = clippoly(npoints,i);
				
				if (npoints < 3)
					continue;
			}

			globalpicnum = spr->picnum;
			if ((unsigned)globalpicnum >= (unsigned)MAXTILES) globalpicnum = 0;
			setgotpic(globalpicnum);
			if ((tilesizx[globalpicnum] <= 0) || (tilesizy[globalpicnum] <= 0))
				continue;

			if ((picanm[globalpicnum]&192) != 0)
				globalpicnum += animateoffs((short)globalpicnum,s);

			if (waloff[globalpicnum] == 0)
				loadtile(globalpicnum);

			globalbufplc = waloff[globalpicnum];

			if ((g_sector[spr->sectnum].ceilingstat&1) > 0)
				globalshade = ((int)g_sector[spr->sectnum].ceilingshade);
			else
				globalshade = ((int)g_sector[spr->sectnum].floorshade);

			globalshade = std::max(std::min(globalshade + spr->shade + 6, static_cast<int>(numpalookups) - 1), 0);
			asm3 = (intptr_t)palookup[spr->pal].data() + (globalshade<<8);
			globvis = globalhisibility;

			if (sec->visibility != 0)
				globvis = mulscalen<4>(globvis,(int)((unsigned char)(sec->visibility+16)));

			globalpolytype = ((spr->cstat&2)>>1)+1;

				//relative alignment stuff
			ox = x2 - x1;
			oy = y2 - y1;
			i = ox*ox+oy*oy;
			
			if (i == 0)
				continue;
			
			i = (65536*16384)/i;

			g_pt1.x = mulscalen<10>(dmulscalen<10>(ox,bakgxvect,oy,bakgyvect),i);
			g_pt1.y = mulscalen<10>(dmulscalen<10>(ox,bakgyvect,-oy,bakgxvect),i);
			ox = y1-y4; oy = x4-x1;
			i = ox*ox+oy*oy; if (i == 0) continue; i = (65536*16384)/i;
			g_pt2.x = mulscalen<10>(dmulscalen<10>(ox,bakgxvect,oy,bakgyvect),i);
			g_pt2.y = mulscalen<10>(dmulscalen<10>(ox,bakgyvect,-oy,bakgxvect),i);

			ox = picsiz[globalpicnum];
			oy = ((ox>>4)&15);
			ox &= 15;
			
			if (pow2long[ox] != xspan)
			{
				ox++;
				g_pt1.x = mulscale(g_pt1.x,xspan,ox);
				g_pt1.y = mulscale(g_pt1.y,xspan,ox);
			}

			bakx1 = (bakx1>>4)-(xdim<<7); baky1 = (baky1>>4)-(ydim<<7);
			globalpos.x = dmulscalen<28>(-baky1,g_pt1.x,-bakx1,g_pt1.y);
			globalpos.y = dmulscalen<28>(bakx1,g_pt2.x,-baky1,g_pt2.y);

			if ((spr->cstat&2) == 0)
				msethlineshift(ox,oy);
			else {
				if (spr->cstat&512) settransreverse(); else settransnormal();
				tsethlineshift(ox,oy);
			}

			if ((spr->cstat&0x4) > 0) {
				g_pt1 = -g_pt1;
				globalpos.x = -globalpos.x;
			}

			asm1 = (g_pt1.y<<2);
			g_pt1.x <<= 2;
			globalpos.x <<= (20+2);
			asm2 = (g_pt2.x<<2);
			g_pt2.y <<= 2;
			globalpos.y <<= (20+2);

			globalorientation = ((spr->cstat&2)<<7) | ((spr->cstat&512)>>2);	// so polymost can get the translucency. ignored in software mode.
			fillpolygon(npoints);
		}
	}
}


//
// loadboard
//
int loadboard(const std::string& filename, char fromwhere, int *daposx, int *daposy, int *daposz,
			 short *daang, short *dacursectnum)
{
	short fil;
	short numsprites;
	short maxsectors;
	short maxwalls;
	short maxsprites;

	if ((fil = kopen4load(filename.c_str(), fromwhere)) == -1) {
		mapversion = 7L;
		return -1;
	}

	kread(fil,&mapversion,4);

	if (mapversion == 7) {
		maxsectors = MAXSECTORSV7;
		maxwalls = MAXWALLSV7;
		maxsprites = MAXSPRITESV7;
	}
	else if (mapversion == 8) {
		maxsectors = MAXSECTORSV8;
		maxwalls = MAXWALLSV8;
		maxsprites = MAXSPRITESV8;
	}
	else {
		kclose(fil);

		return -2;
	}

	/*
	// Enable this for doing map checksum tests
	clearbufbyte(&wall,   sizeof(wall),   0);
	clearbufbyte(&g_sector, sizeof(g_sector), 0);
	clearbufbyte(&sprite, sizeof(sprite), 0);
	*/

	initspritelists();

	std::ranges::fill(show2dsector, 0);
	std::ranges::fill(show2dsprite, 0);
	std::ranges::fill(show2dwall,   0);

	kread(fil,daposx,4);
	kread(fil,daposy,4);
	kread(fil,daposz,4);
	kread(fil,daang,2); 
	kread(fil,dacursectnum,2);
	kread(fil, &numsectors,2);

	if (numsectors > maxsectors) {
		kclose(fil);
		return -2;
	}

	kread(fil, &g_sector[0], sizeof(sectortype) * numsectors);

	kread(fil,&numwalls,2);
	if (numwalls > maxwalls) { kclose(fil); return(-2); }
	kread(fil,&wall[0],sizeof(walltype)*numwalls);

	kread(fil,&numsprites,2);

	if (numsprites > maxsprites) {
		kclose(fil);
		return -2;
	}

	kread(fil, &sprite[0], sizeof(spritetype) * numsprites);

	for(int i{0}; i < numsprites; ++i) {
		if ((sprite[i].cstat & 48) == 48)
			sprite[i].cstat &= ~48;

		insertsprite(sprite[i].sectnum, sprite[i].statnum);
	}

		//Must be after loading sectors, etc!
	updatesector(*daposx, *daposy, dacursectnum);

	kclose(fil);

#if USE_POLYMOST && USE_OPENGL
	std::ranges::fill(spriteext, spriteexttype{});
#endif
	guniqhudid = 0;

	return 0;
}

namespace {

//
// loadboardv5/6
//
struct sectortypev5
{
	unsigned short wallptr;
	unsigned short wallnum;
	short ceilingpicnum;
	short floorpicnum;
	short ceilingheinum;
	short floorheinum;
	int ceilingz;
	int floorz;
	signed char ceilingshade;
	signed char floorshade;
	unsigned char ceilingxpanning;
	unsigned char floorxpanning;
	unsigned char ceilingypanning;
	unsigned char floorypanning;
	unsigned char ceilingstat;
	unsigned char floorstat;
	unsigned char ceilingpal;
	unsigned char floorpal;
	unsigned char visibility;
	short lotag;
	short hitag;
	short extra;
};

struct walltypev5
{
	int x;
	int y;
	short point2;
	short picnum;
	short overpicnum;
	signed char shade;
	short cstat;
	unsigned char xrepeat;
	unsigned char yrepeat;
	unsigned char xpanning;
	unsigned char ypanning;
	short nextsector1;
	short nextwall1;
	short nextsector2;
	short nextwall2;
	short lotag;
	short hitag;
	short extra;
};

struct spritetypev5
{
	int x;
	int y;
	int z;
	unsigned char cstat;
	signed char shade;
	unsigned char xrepeat;
	unsigned char yrepeat;
	short picnum;
	short ang;
	short xvel;
	short yvel;
	short zvel;
	short owner;
	short sectnum;
	short statnum;
	short lotag;
	short hitag;
	short extra;
};

struct sectortypev6
{
	unsigned short wallptr;
	unsigned short wallnum;
	short ceilingpicnum;
	short floorpicnum;
	short ceilingheinum;
	short floorheinum;
	int ceilingz;
	int floorz;
	signed char ceilingshade;
	signed char floorshade;
	unsigned char ceilingxpanning;
	unsigned char floorxpanning;
	unsigned char ceilingypanning;
	unsigned char floorypanning;
	unsigned char ceilingstat;
	unsigned char floorstat;
	unsigned char ceilingpal;
	unsigned char floorpal;
	unsigned char visibility;
	short lotag;
	short hitag;
	short extra;
};

struct walltypev6
{
	int x;
	int y;
	short point2;
	short nextsector;
	short nextwall;
	short picnum;
	short overpicnum;
	signed char shade;
	unsigned char pal;
	short cstat;
	unsigned char xrepeat;
	unsigned char yrepeat;
	unsigned char xpanning;
	unsigned char ypanning;
	short lotag;
	short hitag;
	short extra;
};

struct spritetypev6
{
	int x;
	int y;
	int z;
	short cstat;
	signed char shade;
	unsigned char pal;
	unsigned char clipdist;
	unsigned char xrepeat;
	unsigned char yrepeat;
	signed char xoffset;
	signed char yoffset;
	short picnum;
	short ang;
	short xvel;
	short yvel;
	short zvel;
	short owner;
	short sectnum;
	short statnum;
	short lotag;
	short hitag;
	short extra;
};

short sectorofwallv5(short theline)
{
	short i;
	short startwall;
	short endwall;

	short sucksect{ -1 };

	for(i=0;i<numsectors;i++)
	{
		startwall = g_sector[i].wallptr;
		endwall = startwall + g_sector[i].wallnum - 1;
		if ((theline >= startwall) && (theline <= endwall))
		{
			sucksect = i;
			break;
		}
	}

	return sucksect;
}

int readv5sect(int fil, struct sectortypev5 *sect)
{
	if (kread(fil, &sect->wallptr, 2) != 2) return -1;
	if (kread(fil, &sect->wallnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->floorpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingheinum, 2) != 2) return -1;
	if (kread(fil, &sect->floorheinum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingz, 4) != 4) return -1;
	if (kread(fil, &sect->floorz, 4) != 4) return -1;
	if (kread(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (kread(fil, &sect->floorshade, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorypanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (kread(fil, &sect->floorstat, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (kread(fil, &sect->floorpal, 1) != 1) return -1;
	if (kread(fil, &sect->visibility, 1) != 1) return -1;
	if (kread(fil, &sect->lotag, 2) != 2) return -1;
	if (kread(fil, &sect->hitag, 2) != 2) return -1;
	if (kread(fil, &sect->extra, 2) != 2) return -1;

	return 0;
}

int writev5sect(int fil, struct sectortypev5 const *sect)
{
	uint32_t tl;
	uint16_t ts;

	ts = sect->wallptr; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->wallnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->ceilingpicnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->floorpicnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->ceilingheinum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->floorheinum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	tl = sect->ceilingz; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = sect->floorz; if (Bwrite(fil, &tl, 4) != 4) return -1;
	if (Bwrite(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->visibility, 1) != 1) return -1;
	ts = sect->lotag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->hitag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->extra; if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

void convertv5sectv6(struct sectortypev5 const *from, struct sectortypev6 *to)
{
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->floorpicnum = from->floorpicnum;
	to->ceilingheinum = from->ceilingheinum;
	to->floorheinum = from->floorheinum;
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->ceilingshade = from->ceilingshade;
	to->floorshade = from->floorshade;
	to->ceilingxpanning = from->ceilingxpanning;
	to->floorxpanning = from->floorxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	to->floorstat = from->floorstat;
	to->ceilingpal = from->ceilingpal;
	to->floorpal = from->floorpal;
	to->visibility = from->visibility;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

void convertv6sectv5(struct sectortypev6 const *from, struct sectortypev5 *to)
{
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->floorpicnum = from->floorpicnum;
	to->ceilingheinum = from->ceilingheinum;
	to->floorheinum = from->floorheinum;
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->ceilingshade = from->ceilingshade;
	to->floorshade = from->floorshade;
	to->ceilingxpanning = from->ceilingxpanning;
	to->floorxpanning = from->floorxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	to->floorstat = from->floorstat;
	to->ceilingpal = from->ceilingpal;
	to->floorpal = from->floorpal;
	to->visibility = from->visibility;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

int readv5wall(int fil, struct walltypev5 *wall)
{
	if (kread(fil, &wall->x, 4) != 4) return -1;
	if (kread(fil, &wall->y, 4) != 4) return -1;
	if (kread(fil, &wall->point2, 2) != 2) return -1;
	if (kread(fil, &wall->picnum, 2) != 2) return -1;
	if (kread(fil, &wall->overpicnum, 2) != 2) return -1;
	if (kread(fil, &wall->shade, 1) != 1) return -1;
	if (kread(fil, &wall->cstat, 2) != 2) return -1;
	if (kread(fil, &wall->xrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->yrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->xpanning, 1) != 1) return -1;
	if (kread(fil, &wall->ypanning, 1) != 1) return -1;
	if (kread(fil, &wall->nextsector1, 2) != 2) return -1;
	if (kread(fil, &wall->nextwall1, 2) != 2) return -1;
	if (kread(fil, &wall->nextsector2, 2) != 2) return -1;
	if (kread(fil, &wall->nextwall2, 2) != 2) return -1;
	if (kread(fil, &wall->lotag, 2) != 2) return -1;
	if (kread(fil, &wall->hitag, 2) != 2) return -1;
	if (kread(fil, &wall->extra, 2) != 2) return -1;

	return 0;
}

int writev5wall(int fil, struct walltypev5 const *wall)
{
	uint32_t tl;
	uint16_t ts;

	tl = wall->x; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = wall->y; if (Bwrite(fil, &tl, 4) != 4) return -1;
	ts = wall->point2; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->picnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->overpicnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->shade, 1) != 1) return -1;
	ts = wall->cstat; if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->yrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->xpanning, 1) != 1) return -1;
	if (Bwrite(fil, &wall->ypanning, 1) != 1) return -1;
	ts = wall->nextsector1; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->nextwall1; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->nextsector2; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->nextwall2; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->lotag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->hitag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->extra; if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

void convertv5wallv6(struct walltypev5 const *from, struct walltypev6 *to, int i)
{
	to->x = from->x;
	to->y = from->y;
	to->point2 = from->point2;
	to->nextsector = from->nextsector1;
	to->nextwall = from->nextwall1;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->pal = g_sector[sectorofwallv5((short)i)].floorpal;
	to->cstat = from->cstat;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

void convertv6wallv5(struct walltypev6 const *from, struct walltypev5 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->point2 = from->point2;
	to->nextsector1 = from->nextsector;
	to->nextwall1 = from->nextwall;
	to->nextsector2 = -1;
	to->nextwall2 = -1;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->cstat = from->cstat;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

int readv5sprite(int fil, struct spritetypev5 *spr)
{
	if (kread(fil, &spr->x, 4) != 4) return -1;
	if (kread(fil, &spr->y, 4) != 4) return -1;
	if (kread(fil, &spr->z, 4) != 4) return -1;
	if (kread(fil, &spr->cstat, 1) != 1) return -1;
	if (kread(fil, &spr->shade, 1) != 1) return -1;
	if (kread(fil, &spr->xrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->yrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->picnum, 2) != 2) return -1;
	if (kread(fil, &spr->ang, 2) != 2) return -1;
	if (kread(fil, &spr->xvel, 2) != 2) return -1;
	if (kread(fil, &spr->yvel, 2) != 2) return -1;
	if (kread(fil, &spr->zvel, 2) != 2) return -1;
	if (kread(fil, &spr->owner, 2) != 2) return -1;
	if (kread(fil, &spr->sectnum, 2) != 2) return -1;
	if (kread(fil, &spr->statnum, 2) != 2) return -1;
	if (kread(fil, &spr->lotag, 2) != 2) return -1;
	if (kread(fil, &spr->hitag, 2) != 2) return -1;
	if (kread(fil, &spr->extra, 2) != 2) return -1;

	return 0;
}

int writev5sprite(int fil, struct spritetypev5 const *spr)
{
	uint32_t tl;
	uint16_t ts;

	tl = spr->x; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = spr->y; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = spr->z; if (Bwrite(fil, &tl, 4) != 4) return -1;
	if (Bwrite(fil, &spr->cstat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->shade, 1) != 1) return -1;
	if (Bwrite(fil, &spr->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->yrepeat, 1) != 1) return -1;
	ts = spr->picnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->ang; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->xvel; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->yvel; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->zvel; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->owner; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->sectnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->statnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->lotag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->hitag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->extra; if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

void convertv5sprv6(struct spritetypev5 const *from, struct spritetypev6 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = from->cstat;
	to->shade = from->shade;

	const short j = from->sectnum;
	if ((g_sector[j].ceilingstat&1) > 0)
		to->pal = g_sector[j].ceilingpal;
	else
		to->pal = g_sector[j].floorpal;

	to->clipdist = 32;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xoffset = 0;
	to->yoffset = 0;
	to->picnum = from->picnum;
	to->ang = from->ang;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->owner = from->owner;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

void convertv6sprv5(struct spritetypev6 const *from, struct spritetypev5 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = (unsigned char)from->cstat;
	to->shade = from->shade;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->picnum = from->picnum;
	to->ang = from->ang;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->owner = from->owner;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

int readv6sect(int fil, struct sectortypev6 *sect)
{
	if (kread(fil, &sect->wallptr, 2) != 2) return -1;
	if (kread(fil, &sect->wallnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->floorpicnum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingheinum, 2) != 2) return -1;
	if (kread(fil, &sect->floorheinum, 2) != 2) return -1;
	if (kread(fil, &sect->ceilingz, 4) != 4) return -1;
	if (kread(fil, &sect->floorz, 4) != 4) return -1;
	if (kread(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (kread(fil, &sect->floorshade, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (kread(fil, &sect->floorypanning, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (kread(fil, &sect->floorstat, 1) != 1) return -1;
	if (kread(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (kread(fil, &sect->floorpal, 1) != 1) return -1;
	if (kread(fil, &sect->visibility, 1) != 1) return -1;
	if (kread(fil, &sect->lotag, 2) != 2) return -1;
	if (kread(fil, &sect->hitag, 2) != 2) return -1;
	if (kread(fil, &sect->extra, 2) != 2) return -1;

	return 0;
}

int writev6sect(int fil, struct sectortypev6 const *sect)
{
	uint32_t tl;
	uint16_t ts;

	ts = sect->wallptr; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->wallnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->ceilingpicnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->floorpicnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->ceilingheinum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->floorheinum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	tl = sect->ceilingz; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = sect->floorz; if (Bwrite(fil, &tl, 4) != 4) return -1;
	if (Bwrite(fil, &sect->ceilingshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorshade, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorxpanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorypanning, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorstat, 1) != 1) return -1;
	if (Bwrite(fil, &sect->ceilingpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->floorpal, 1) != 1) return -1;
	if (Bwrite(fil, &sect->visibility, 1) != 1) return -1;
	ts = sect->lotag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->hitag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = sect->extra; if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

void convertv6sectv7(struct sectortypev6 const *from, sectortype *to)
{
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->ceilingheinum = std::max(std::min(static_cast<int>(from->ceilingheinum) << 5, 32767), -32768);
	if ((from->ceilingstat&2) == 0) to->ceilingheinum = 0;
	to->ceilingshade = from->ceilingshade;
	to->ceilingpal = from->ceilingpal;
	to->ceilingxpanning = from->ceilingxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorpicnum = from->floorpicnum;
	to->floorheinum = std::max(std::min(static_cast<int>(from->floorheinum) << 5, 32767), -32768);
	if ((from->floorstat&2) == 0) to->floorheinum = 0;
	to->floorshade = from->floorshade;
	to->floorpal = from->floorpal;
	to->floorxpanning = from->floorxpanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	to->floorstat = from->floorstat;
	to->visibility = from->visibility;
	to->filler = 0;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

void convertv7sectv6(const sectortype *from, struct sectortypev6 *to)
{
	to->ceilingz = from->ceilingz;
	to->floorz = from->floorz;
	to->wallptr = from->wallptr;
	to->wallnum = from->wallnum;
	to->ceilingpicnum = from->ceilingpicnum;
	to->ceilingheinum = (((int)from->ceilingheinum)>>5);
	to->ceilingshade = from->ceilingshade;
	to->ceilingpal = from->ceilingpal;
	to->ceilingxpanning = from->ceilingxpanning;
	to->ceilingypanning = from->ceilingypanning;
	to->floorpicnum = from->floorpicnum;
	to->floorheinum = (((int)from->floorheinum)>>5);
	to->floorshade = from->floorshade;
	to->floorpal = from->floorpal;
	to->floorxpanning = from->floorxpanning;
	to->floorypanning = from->floorypanning;
	to->ceilingstat = from->ceilingstat;
	if (to->ceilingheinum == 0) to->ceilingstat &= ~2;
	to->floorstat = from->floorstat;
	if (to->floorheinum == 0) to->floorstat &= ~2;
	to->visibility = from->visibility;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

int readv6wall(int fil, struct walltypev6 *wall)
{
	if (kread(fil, &wall->x, 4) != 4) return -1;
	if (kread(fil, &wall->y, 4) != 4) return -1;
	if (kread(fil, &wall->point2, 2) != 2) return -1;
	if (kread(fil, &wall->nextsector, 2) != 2) return -1;
	if (kread(fil, &wall->nextwall, 2) != 2) return -1;
	if (kread(fil, &wall->picnum, 2) != 2) return -1;
	if (kread(fil, &wall->overpicnum, 2) != 2) return -1;
	if (kread(fil, &wall->shade, 1) != 1) return -1;
	if (kread(fil, &wall->pal, 1) != 1) return -1;
	if (kread(fil, &wall->cstat, 2) != 2) return -1;
	if (kread(fil, &wall->xrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->yrepeat, 1) != 1) return -1;
	if (kread(fil, &wall->xpanning, 1) != 1) return -1;
	if (kread(fil, &wall->ypanning, 1) != 1) return -1;
	if (kread(fil, &wall->lotag, 2) != 2) return -1;
	if (kread(fil, &wall->hitag, 2) != 2) return -1;
	if (kread(fil, &wall->extra, 2) != 2) return -1;

	return 0;
}

int writev6wall(int fil, struct walltypev6 const *wall)
{
	uint32_t tl;
	uint16_t ts;

	tl = wall->x; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = wall->y; if (Bwrite(fil, &tl, 4) != 4) return -1;
	ts = wall->point2; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->nextsector; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->nextwall; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->picnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->overpicnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->shade, 1) != 1) return -1;
	if (Bwrite(fil, &wall->pal, 1) != 1) return -1;
	ts = wall->cstat; if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &wall->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->yrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &wall->xpanning, 1) != 1) return -1;
	if (Bwrite(fil, &wall->ypanning, 1) != 1) return -1;
	ts = wall->lotag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->hitag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = wall->extra; if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

void convertv6wallv7(struct walltypev6 const *from, walltype *to)
{
	to->pt.x = from->x;
	to->pt.y = from->y;
	to->point2 = from->point2;
	to->nextwall = from->nextwall;
	to->nextsector = from->nextsector;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

void convertv7wallv6(const walltype *from, struct walltypev6 *to)
{
	to->x = from->pt.x;
	to->y = from->pt.y;
	to->point2 = from->point2;
	to->nextwall = from->nextwall;
	to->nextsector = from->nextsector;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->overpicnum = from->overpicnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xpanning = from->xpanning;
	to->ypanning = from->ypanning;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

int readv6sprite(int fil, struct spritetypev6 *spr)
{
	if (kread(fil, &spr->x, 4) != 4) return -1;
	if (kread(fil, &spr->y, 4) != 4) return -1;
	if (kread(fil, &spr->z, 4) != 4) return -1;
	if (kread(fil, &spr->cstat, 2) != 2) return -1;
	if (kread(fil, &spr->shade, 1) != 1) return -1;
	if (kread(fil, &spr->pal, 1) != 1) return -1;
	if (kread(fil, &spr->clipdist, 1) != 1) return -1;
	if (kread(fil, &spr->xrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->yrepeat, 1) != 1) return -1;
	if (kread(fil, &spr->xoffset, 1) != 1) return -1;
	if (kread(fil, &spr->yoffset, 1) != 1) return -1;
	if (kread(fil, &spr->picnum, 2) != 2) return -1;
	if (kread(fil, &spr->ang, 2) != 2) return -1;
	if (kread(fil, &spr->xvel, 2) != 2) return -1;
	if (kread(fil, &spr->yvel, 2) != 2) return -1;
	if (kread(fil, &spr->zvel, 2) != 2) return -1;
	if (kread(fil, &spr->owner, 2) != 2) return -1;
	if (kread(fil, &spr->sectnum, 2) != 2) return -1;
	if (kread(fil, &spr->statnum, 2) != 2) return -1;
	if (kread(fil, &spr->lotag, 2) != 2) return -1;
	if (kread(fil, &spr->hitag, 2) != 2) return -1;
	if (kread(fil, &spr->extra, 2) != 2) return -1;

	return 0;
}

int writev6sprite(int fil, struct spritetypev6 const *spr)
{
	uint32_t tl;
	uint16_t ts;

	tl = spr->x; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = spr->y; if (Bwrite(fil, &tl, 4) != 4) return -1;
	tl = spr->z; if (Bwrite(fil, &tl, 4) != 4) return -1;
	ts = spr->cstat; if (Bwrite(fil, &ts, 2) != 2) return -1;
	if (Bwrite(fil, &spr->shade, 1) != 1) return -1;
	if (Bwrite(fil, &spr->pal, 1) != 1) return -1;
	if (Bwrite(fil, &spr->clipdist, 1) != 1) return -1;
	if (Bwrite(fil, &spr->xrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->yrepeat, 1) != 1) return -1;
	if (Bwrite(fil, &spr->xoffset, 1) != 1) return -1;
	if (Bwrite(fil, &spr->yoffset, 1) != 1) return -1;
	ts = spr->picnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->ang; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->xvel; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->yvel; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->zvel; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->owner; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->sectnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->statnum; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->lotag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->hitag; if (Bwrite(fil, &ts, 2) != 2) return -1;
	ts = spr->extra; if (Bwrite(fil, &ts, 2) != 2) return -1;

	return 0;
}

void convertv6sprv7(struct spritetypev6 const *from, spritetype *to)
{
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->clipdist = from->clipdist;
	to->filler = 0;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xoffset = from->xoffset;
	to->yoffset = from->yoffset;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->ang = from->ang;
	to->owner = from->owner;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

void convertv7sprv6(const spritetype *from, struct spritetypev6 *to)
{
	to->x = from->x;
	to->y = from->y;
	to->z = from->z;
	to->cstat = from->cstat;
	to->picnum = from->picnum;
	to->shade = from->shade;
	to->pal = from->pal;
	to->clipdist = from->clipdist;
	to->xrepeat = from->xrepeat;
	to->yrepeat = from->yrepeat;
	to->xoffset = from->xoffset;
	to->yoffset = from->yoffset;
	to->sectnum = from->sectnum;
	to->statnum = from->statnum;
	to->ang = from->ang;
	to->owner = from->owner;
	to->xvel = from->xvel;
	to->yvel = from->yvel;
	to->zvel = from->zvel;
	to->lotag = from->lotag;
	to->hitag = from->hitag;
	to->extra = from->extra;
}

} // namespace

// Powerslave uses v6
// Witchaven 1 and TekWar use v5
int loadoldboard(const std::string& filename, char fromwhere, int *daposx, int *daposy, int *daposz,
			 short *daang, short *dacursectnum)
{
	short fil;
	short numsprites;
	struct sectortypev5 v5sect;
	struct walltypev5   v5wall;
	struct spritetypev5 v5spr;
	struct sectortypev6 v6sect;
	struct walltypev6   v6wall;
	struct spritetypev6 v6spr;

	const auto readerror = [](const auto& fil){
		kclose(fil);
		return -3;
	};

	if ((fil = kopen4load(filename.c_str(), fromwhere)) == -1) {
		mapversion = 5L;
		return -1;
	}

	if (kread(fil,&mapversion,4) != 4)
		readerror(fil);
	
	if (mapversion != 5L && mapversion != 6L) {
		kclose(fil);
		return -2;
	}

	initspritelists();

	clearbuf(&show2dsector[0],(int)((MAXSECTORS+3)>>5),0L);
	clearbuf(&show2dsprite[0],(int)((MAXSPRITES+3)>>5),0L);
	clearbuf(&show2dwall[0],(int)((MAXWALLS+3)>>5),0L);

	if (kread(fil,daposx,4) != 4) return readerror(fil);
	if (kread(fil,daposy,4) != 4) return readerror(fil);
	if (kread(fil,daposz,4) != 4) return readerror(fil);
	if (kread(fil,daang,2) != 2) return readerror(fil);
	if (kread(fil,dacursectnum,2) != 2) return readerror(fil);
	if (kread(fil,&numsectors,2) != 2) return readerror(fil);

	if (numsectors > MAXSECTORS) {
		kclose(fil);
		return(-1);
	}

	for (int i{0}; i < numsectors; ++i) {
		switch (mapversion) {
			case 5:
				if (readv5sect(fil,&v5sect)) return readerror(fil);
				convertv5sectv6(&v5sect,&v6sect);
				convertv6sectv7(&v6sect,&g_sector[i]);
				break;
			case 6:
				if (readv6sect(fil,&v6sect)) return readerror(fil);
				convertv6sectv7(&v6sect,&g_sector[i]);
				break;
		}
	}

	if (kread(fil,&numwalls,2) != 2) return readerror(fil);
	if (numwalls > MAXWALLS) {
		kclose(fil);
		return(-1);
	}

	for (int i{0}; i < numwalls; ++i) {
		switch (mapversion) {
			case 5:
				if (readv5wall(fil,&v5wall)) return readerror(fil);
				convertv5wallv6(&v5wall,&v6wall,i);
				convertv6wallv7(&v6wall,&wall[i]);
				break;
			case 6:
				if (readv6wall(fil,&v6wall)) return readerror(fil);
				convertv6wallv7(&v6wall,&wall[i]);
				break;
		}
	}

	if (kread(fil,&numsprites,2) != 2) return readerror(fil);
	if (numsprites > MAXSPRITES) {
		kclose(fil);
		return(-1);
	}

	for (int i{0}; i < numsprites; ++i) {
		switch (mapversion) {
			case 5:
				if (readv5sprite(fil,&v5spr)) readerror(fil);
				convertv5sprv6(&v5spr,&v6spr);
				convertv6sprv7(&v6spr,&sprite[i]);
				break;
			case 6:
				if (readv6sprite(fil,&v6spr)) readerror(fil);
				convertv6sprv7(&v6spr,&sprite[i]);
				break;
		}
	}

	for(int i{0}; i < numsprites; ++i) {
		if ((sprite[i].cstat & 48) == 48) sprite[i].cstat &= ~48;
		insertsprite(sprite[i].sectnum,sprite[i].statnum);
	}

		//Must be after loading sectors, etc!
	updatesector(*daposx,*daposy,dacursectnum);

	kclose(fil);

#if USE_POLYMOST && USE_OPENGL
	std::ranges::fill(spriteext, spriteexttype{});
#endif
	guniqhudid = 0;

	return 0;
}


//
// loadmaphack
//
#include "scriptfile.hpp"
int loadmaphack(const std::string& filename)
{
#if USE_POLYMOST && USE_OPENGL
	static constexpr struct {
		const char *text;
		int tokenid;
	} legaltokens[] = {
		{ "sprite", 0 },
		{ "angleoff", 1 },
		{ "angoff", 1 },
		{ "notmd2", 2 },
		{ "notmd3", 2 },
		{ "notmd", 2 },
		{ "nomd2anim", 3 },
		{ "nomd3anim", 3 },
		{ "nomdanim", 3 },
		{ nullptr, -1 }
	};

	std::optional<int> whichsprite{std::nullopt};

	auto script = scriptfile_fromfile(filename);
	
	if (!script) {
		return -1;
	}

	std::ranges::fill(spriteext, spriteexttype{});

	while (1) {
		const auto tok = scriptfile_gettoken(script.get());

		if (!tok.has_value())
			break;
		
		int i{0};

		for (; legaltokens[i].text; ++i)
			if (IsSameAsNoCase(tok.value(), legaltokens[i].text))
				break;

		char* cmdtokptr = script->ltextptr;

		switch (legaltokens[i].tokenid) {
			case 0:		// sprite <xx>
				whichsprite = scriptfile_getnumber(script.get());
				if (!whichsprite.has_value()) break;

				if ((unsigned)whichsprite.value() >= (unsigned)MAXSPRITES) {
					// sprite number out of range
					buildprintf("Sprite number out of range 0-{} on line {}:{}\n",
							MAXSPRITES-1,script->filename, scriptfile_getlinum(script.get(), cmdtokptr));
					whichsprite = std::nullopt;
					break;
				}

				break;
			case 1:		// angoff <xx>
				{
					auto ang = scriptfile_getnumber(script.get());
					if (!ang.has_value()) break;

					if (!whichsprite.has_value()) {
						// no sprite directive preceeding
						buildprintf("Ignoring angle offset directive because of absent/invalid sprite number on line {}:{}\n",
							script->filename, scriptfile_getlinum(script.get(), cmdtokptr));
						break;
					}
					spriteext[whichsprite.value()].angoff = (short) ang.value();
				}
				break;
			case 2:      // notmd
				if (!whichsprite.has_value()) {
					// no sprite directive preceeding
					buildprintf("Ignoring not-MD2/MD3 directive because of absent/invalid sprite number on line {}:{}\n",
							script->filename, scriptfile_getlinum(script.get(), cmdtokptr));
					break;
				}
				spriteext[whichsprite.value()].flags |= SPREXT_NOTMD;
				break;
			case 3:      // nomdanim
				if (!whichsprite.has_value()) {
					// no sprite directive preceeding
					buildprintf("Ignoring no-MD2/MD3-anim directive because of absent/invalid sprite number on line {}:{}\n",
							script->filename, scriptfile_getlinum(script.get(), cmdtokptr));
					break;
				}
				spriteext[whichsprite.value()].flags |= SPREXT_NOMDANIM;
				break;
			default:
				// unrecognised token
				break;
		}
	}

#else
	(void)filename;
#endif //USE_POLYMOST && USE_OPENGL

	return 0;
}


//
// saveboard
//
int saveboard(const std::string& filename, const int *daposx, const int *daposy, const int *daposz,
			 const short *daang, const short *dacursectnum)
{
	short i;
	short j;

	short fil{};
	if ((fil = Bopen(filename.c_str(), BO_BINARY|BO_TRUNC|BO_CREAT|BO_WRONLY,BS_IREAD|BS_IWRITE)) == -1)
		return(-1);

	short numsprites{0};

	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			numsprites++;
			i = nextspritestat[i];
		}
	}

	if (numsectors > MAXSECTORSV7 || numwalls > MAXWALLSV7 || numsprites > MAXSPRITESV7)
		mapversion = 8;
	else
		mapversion = 7;
	
	auto tl = mapversion;
	int ts{0};

	const auto writeerror = [](const auto& fil){
		Bclose(fil);
		return -1;
	};

	if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);

	tl = *daposx;       if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);
	tl = *daposy;       if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);
	tl = *daposz;       if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);
	ts = *daang;        if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);
	ts = *dacursectnum; if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);

	ts = numsectors;    if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);
	for (const auto& tsect : g_sector) {
		if (Bwrite(fil, &tsect, sizeof(sectortype)) != sizeof(sectortype))
			return writeerror(fil);
	}

	ts = numwalls;
	if (Bwrite(fil,&ts,2) != 2)
		return writeerror(fil);

	for (const auto& twall : wall) {
		if (Bwrite(fil, &twall, sizeof(walltype)) != sizeof(walltype))
			return writeerror(fil);
	}

	ts = numsprites;
	
	if (Bwrite(fil,&ts,2) != 2)
		return writeerror(fil);

	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			const auto& tspri = sprite[i];
			if (Bwrite(fil,&tspri,sizeof(spritetype)) != sizeof(spritetype))
				return writeerror(fil);
			i = nextspritestat[i];
		}
	}

	Bclose(fil);
	return 0;
}


int saveoldboard(const char *filename, const int *daposx, const int *daposy, const int *daposz,
			 const short *daang, const short *dacursectnum)
{
	short fil;
	short i;
	short j;
	short ts;
	int tl;
	struct sectortypev5 v5sect;
	struct walltypev5   v5wall;
	struct spritetypev5 v5spr;
	struct sectortypev6 v6sect;
	struct walltypev6   v6wall;
	struct spritetypev6 v6spr;

	if (mapversion != 5 && mapversion != 6) {
		buildputs("saveoldboard: map version not 5 or 6\n");
		return -2;
	}

	short numsprites{0};

	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			numsprites++;
			i = nextspritestat[i];
		}
	}

	switch (mapversion) {
		case 5:
			if (numsectors > MAXSECTORSV5 || numwalls > MAXWALLSV5 || numsprites > MAXSPRITESV5) {
				buildprintf("saveoldboard: too many sectors/walls/sprites for map version 5 ({}/{}, {}/{}, {}/{})\n",
					numsectors, MAXSECTORSV5, numwalls, MAXWALLSV5, numsprites, MAXSPRITESV5);
				return -2;
			}
			break;
		case 6:
			if (numsectors > MAXSECTORSV6 || numwalls > MAXWALLSV6 || numsprites > MAXSPRITESV6) {
				buildprintf("saveoldboard: too many sectors/walls/sprites for map version 6 ({}/{}, {}/{}, {}/{})\n",
					numsectors, MAXSECTORSV6, numwalls, MAXWALLSV6, numsprites, MAXSPRITESV6);
				return -2;
			}
			break;
	}

	if ((fil = Bopen(filename,BO_BINARY|BO_TRUNC|BO_CREAT|BO_WRONLY,BS_IREAD|BS_IWRITE)) == -1)
		return(-1);

	const auto writeerror = [](const auto& fil){
		Bclose(fil);
		return -1;
	};

	tl = mapversion;
	if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);

	tl = *daposx;
	if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);
	tl = *daposy;
	if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);
	tl = *daposz;
	if (Bwrite(fil,&tl,4) != 4) return writeerror(fil);
	ts = *daang;
	if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);
	ts = *dacursectnum;
	if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);

	ts = numsectors;
	if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);
	for (i=0; i<numsectors; i++) {
		switch (mapversion) {
			case 6:
				convertv7sectv6(&g_sector[i], &v6sect);
				if (writev6sect(fil, &v6sect)) return writeerror(fil);
				break;
			case 5:
				convertv7sectv6(&g_sector[i], &v6sect);
				convertv6sectv5(&v6sect, &v5sect);
				if (writev5sect(fil, &v5sect)) return writeerror(fil);
				break;
		}
	}

	ts = numwalls;
	if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);
	for (i=0; i<numwalls; i++) {
		switch (mapversion) {
			case 6:
				convertv7wallv6(&wall[i], &v6wall);
				if (writev6wall(fil, &v6wall)) return writeerror(fil);
				break;
			case 5:
				convertv7wallv6(&wall[i], &v6wall);
				convertv6wallv5(&v6wall, &v5wall);
				if (writev5wall(fil, &v5wall)) return writeerror(fil);
				break;
		}
	}

	ts = numsprites;
	if (Bwrite(fil,&ts,2) != 2) return writeerror(fil);
	for(j=0;j<MAXSTATUS;j++)
	{
		i = headspritestat[j];
		while (i != -1)
		{
			switch (mapversion) {
				case 6:
					convertv7sprv6(&sprite[i], &v6spr);
					if (writev6sprite(fil, &v6spr)) return writeerror(fil);
					break;
				case 5:
					convertv7sprv6(&sprite[i], &v6spr);
					convertv6sprv5(&v6spr, &v5spr);
					if (writev5sprite(fil, &v5spr)) return writeerror(fil);
					break;
			}
			i = nextspritestat[i];
		}
	}

	Bclose(fil);
	return 0;
}


//
// setgamemode
//
// JBF: davidoption now functions as a windowed-mode flag (0 == windowed, true == fullscreen)
int setgamemode(bool davidoption, int daxdim, int daydim, int dabpp)
{
	int i;

	if ((qsetmode == 200) && !videomodereset &&
	    (davidoption == fullscreen) && (xdim == daxdim) && (ydim == daydim) && (bpp == dabpp))
		return(0);

	std::strcpy(&kensmessage[0],"!!!! BUILD engine&tools programmed by Ken Silverman of E.G. RI.  (c) Copyright 1995 Ken Silverman.  Summary:  BUILD = Ken. !!!!");

#if USE_POLYMOST && USE_OPENGL
	const int oldbpp{ bpp };
#endif
	//bytesperline is set in this function
	if (setvideomode(daxdim, daydim, dabpp, davidoption) < 0)
		return -1;

#ifndef USING_A_C
	// it's possible the previous call protected our code sections again
	makeasmwriteable();
#endif

#if USE_POLYMOST && USE_OPENGL
	if (dabpp > 8)
		rendmode = rendmode_t::OpenGL;	// GL renderer
	else if (dabpp == 8 && oldbpp != 8)
		rendmode = rendmode_t::Classic;	// going from GL to software activates classic
#endif

	xdim = xres;
	ydim = yres;

	// determine the corrective factor for pixel-squareness. Build
	// is built around the non-square pixels of Mode 13h, so to get
	// things back square on VGA screens, things need to be "compressed"
	// vertically a little.
	widescreen = false;
	tallscreen = false;

	if ((xdim == 320 && ydim == 200) || (xdim == 640 && ydim == 400)) {
		pixelaspect = 65536;
	}
	else {
		const int ratio = divscalen<16>(ydim * 320, xdim * 240);
		pixelaspect = divscalen<16>(240 * 320L, 320 * 200L);

		if (ratio < 65536) {
			widescreen = true;
		}
		else if (ratio > 65536) {
			tallscreen = true;

			// let tall screens (eg. 1280x1024) stretch the 2D elements
			// vertically a little until something better is thought of
			pixelaspect = divscalen<16>(ydim*320L,xdim*200L);
		}
	}

	int looktable_size = ydim * 4 * sizeof(int);  // Leave room for horizlookup and horizlookup2

	lookups.clear();
	lookups.resize(looktable_size << 1);

	horizlookup = lookups.data();
	horizlookup2 = (int *)((intptr_t)lookups.data() + looktable_size);
	horizycent = ((ydim * 4) >> 1);

	//Force drawrooms to call dosetaspect & recalculate stuff
	oxyaspect = -1;
	oxdimen = -1;
	oviewingrange = -1;

	setvlinebpl(bytesperline);

	std::for_each(ylookup.begin(), std::next(ylookup.begin(), ydim + 1),
		[j = 0](auto& ylook) mutable {
			ylook = j;
			j += bytesperline;
		});

	setview(0L, 0L, xdim - 1, ydim - 1);

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == rendmode_t::OpenGL) {
		polymost_glreset();
		polymost_glinit();
	}
#endif

	setbrightness(curbrightness, palette, 0);
	clearallviews(0L);

	if (searchx < 0) {
		searchx = halfxdimen;
		searchy = ydimen >> 1;
	}

	qsetmode = 200;

	//std::memset(ratelimitlast,0,sizeof(ratelimitlast));
	//ratelimitn = 0;

	return 0;
}


//
// nextpage
//
void nextpage()
{
	//char snotbuf[32];
	//j = 0; k = 0;
	//for(i=0;i<4096;i++)
	//   if (waloff[i] != 0)
	//   {
	//      std::sprintf(snotbuf,"%ld-%ld",i,tilesizx[i]*tilesizy[i]);
	//      printext256((j>>5)*40+32,(j&31)*6,walock[i]>>3,-1,snotbuf,1);
	//      k += tilesizx[i]*tilesizy[i];
	//      j++;
	//   }
	//std::sprintf(snotbuf,"Total: %ld",k);
	//printext256((j>>5)*40+32,(j&31)*6,31,-1,snotbuf,1);

	switch(qsetmode)
	{
		case 200:
			for(int i{permtail}; i != permhead; i = ((i + 1) & (MAXPERMS - 1)))
			{
				auto* per = &permfifo[i];
				if ((per->pagesleft > 0) && (per->pagesleft <= numpages))
					dorotatesprite(per->sx,per->sy,per->z,per->a,per->picnum,
							per->dashade,per->dapalnum,per->dastat,
							per->cx1,per->cy1,per->cx2,per->cy2,per->uniqid);
			}

			OSD_Draw();
#if USE_POLYMOST
			polymost_nextpage();
#endif

			if (captureatnextpage) {
				screencapture("", captureatnextpage); // FIXME: Correct behavior?
				captureatnextpage = false;
			}

			showframe();
#if USE_POLYMOST && USE_OPENGL
			polymost_aftershowframe();
#endif

			/*
			if (ratelimit > 0) {
				int delaytime;
				unsigned int thisticks, thist;

				ratelimitlast[ ratelimitn++ & 31 ] = thist = getusecticks();
				delaytime = 0;
				if (ratelimitn >= 32) {
					for (i=1;i<32;i++) delaytime += ratelimitlast[i] - ratelimitlast[i-1];
					delaytime = (1000000/ratelimit) - (delaytime/31);
				}
#ifdef _WIN32
				while (delaytime > 0) {
					Sleep(1);
					thisticks = getusecticks();
					delaytime -= (thisticks - thist);
					thist = thisticks;
				}
#endif
			}
			*/

			for(int i{permtail}; i != permhead; i = ((i + 1) & (MAXPERMS - 1)))
			{
				auto* per = &permfifo[i];
				if (per->pagesleft >= 130)
					dorotatesprite(per->sx,per->sy,per->z,per->a,per->picnum,
										per->dashade,per->dapalnum,per->dastat,
										per->cx1,per->cy1,per->cx2,per->cy2,per->uniqid);

				if ((per->pagesleft&127) && (numpages < 127)) per->pagesleft--;
				if (((per->pagesleft&127) == 0) && (i == permtail))
					permtail = ((permtail+1)&(MAXPERMS-1));
			}
			break;

		case 480:
			break;
	}

	faketimerhandler();

	if ((totalclock >= lastageclock+8) || (totalclock < lastageclock)) {
		lastageclock = totalclock;
		agecache();
	}

#if USE_POLYMOST && USE_OPENGL
	omdtims = mdtims;
	mdtims = getticks();

	if (((unsigned int)(mdtims-omdtims)) > 10000)
		omdtims = mdtims;
#endif

	beforedrawrooms = true;
	++numframes;
}


//
// loadpics
//
// TODO: Maybe consider a strict file format type for filename.
int loadpics(const std::string& filename, int askedsize)
{
	std::ranges::copy(filename, &artfilename[0]);

	std::ranges::fill(tilesizx, 0);
	std::ranges::fill(tilesizy, 0);
	std::ranges::fill(picanm, 0);

	artsize = 0L;

	numtilefiles = 0;

	int k{0};

	do
	{
		k = numtilefiles;

		artfilename[7] = (k%10)+48;
		artfilename[6] = ((k/10)%10)+48;
		artfilename[5] = ((k/100)%10)+48;

		short fil{0};

		if ((fil = kopen4load(artfilename.data(), 0)) != -1)
		{
			int localtilestart{0};
			int localtileend{0};
			kread(fil,&artversion,4);
			
			if (artversion != 1) {
				buildprintf("loadpics(): Invalid art file version in {}\n", artfilename.data());
				return(-1);
			}
			
			kread(fil,&numtiles,4);
			kread(fil,&localtilestart,4);
			kread(fil,&localtileend,4);
			kread(fil,&tilesizx[localtilestart],(localtileend-localtilestart+1)<<1);
			kread(fil,&tilesizy[localtilestart],(localtileend-localtilestart+1)<<1);
			kread(fil,&picanm[localtilestart],(localtileend-localtilestart+1)<<2);

			int offscount = 4 + 4 + 4 + 4 + ((localtileend-localtilestart+1)<<3);
			
			for(int i{localtilestart}; i <= localtileend; ++i)
			{
				tilefilenum[i] = k;
				tilefileoffs[i] = offscount;
				const int dasiz = (int)(tilesizx[i] * tilesizy[i]);
				offscount += dasiz;
				artsize += ((dasiz + 15) & 0xfffffff0);
			}

			kclose(fil);

			++numtilefiles;
		}
	} while (k != numtilefiles);

	clearbuf(&gotpic[0],(int)((MAXTILES+31)>>5),0L);

	//try dpmi_DETERMINEMAXREALALLOC!

	//cachesize = std::min((int)((Bgetsysmemsize()/100)*60), std::max(artsize, askedsize));
	if (Bgetsysmemsize() <= (unsigned int)askedsize)
		cachesize = (Bgetsysmemsize() / 100) * 60;
	else
		cachesize = askedsize;

	while ((pic = std::malloc(cachesize)) == nullptr)
	{
		cachesize -= 65536L;
	
		if (cachesize < 65536)
			return -1;
	}

	initcache(pic, cachesize);

	for(int i{0}; i < MAXTILES; ++i)
	{
		int j{15};

		while ((j > 1) && (pow2long[j] > tilesizx[i]))
			--j;

		picsiz[i] = ((unsigned char)j);
		
		j = 15;
		while ((j > 1) && (pow2long[j] > tilesizy[i]))
			--j;
		
		picsiz[i] += ((unsigned char)(j << 4));
	}

	artfil = -1;
	artfilnum = -1;
	artfilplc = 0L;

	return 0;
}


//
// loadtile
//
char cachedebug = 0;
void loadtile(short tilenume)
{
	if ((unsigned)tilenume >= (unsigned)MAXTILES)
		return;

	const int dasiz = tilesizx[tilenume] * tilesizy[tilenume];

	if (dasiz <= 0)
		return;

	const int i = tilefilenum[tilenume];
	
	if (i != artfilnum)
	{
		if (artfil != -1)
			kclose(artfil);

		artfilnum = i;
		artfilplc = 0L;

		artfilename[7] = (i % 10) + 48;
		artfilename[6] = ((i / 10) % 10) + 48;
		artfilename[5] = ((i / 100) % 10) + 48;
		artfil = kopen4load(artfilename.data(), 0);

		faketimerhandler();
	}

	if (cachedebug)
		buildprintf("Tile:{}\n", tilenume);

	if (waloff[tilenume] == 0)
	{
		walock[tilenume] = 199;
		allocache((void **)&waloff[tilenume], dasiz,&walock[tilenume]);
	}

	if (artfilplc != tilefileoffs[tilenume])
	{
		klseek(artfil, tilefileoffs[tilenume] - artfilplc, BSEEK_CUR);
		faketimerhandler();
	}

	auto* ptr = (char *)waloff[tilenume];

	kread(artfil, ptr, dasiz);
	faketimerhandler();
	
	artfilplc = tilefileoffs[tilenume] + dasiz;
}


//
// allocatepermanenttile
//
intptr_t allocatepermanenttile(short tilenume, int xsiz, int ysiz)
{
	if ((xsiz <= 0) || (ysiz <= 0) || ((unsigned)tilenume >= (unsigned)MAXTILES))
		return 0;

	const int dasiz = xsiz*ysiz;

	walock[tilenume] = 255;
	allocache((void **)&waloff[tilenume],dasiz,&walock[tilenume]);

	tilesizx[tilenume] = xsiz;
	tilesizy[tilenume] = ysiz;
	picanm[tilenume] = 0;

	int j{15};
	while ((j > 1) && (pow2long[j] > xsiz))
		--j;

	picsiz[tilenume] = ((unsigned char)j);

	j = 15;
	while ((j > 1) && (pow2long[j] > ysiz))
		--j;

	picsiz[tilenume] += ((unsigned char)(j<<4));

	return waloff[tilenume];
}


//
// copytilepiece
//
void copytilepiece(int tilenume1, int sx1, int sy1, int xsiz, int ysiz,
		  int tilenume2, int sx2, int sy2)
{
	const int xsiz1 = tilesizx[tilenume1];
	const int ysiz1 = tilesizy[tilenume1];
	const int xsiz2 = tilesizx[tilenume2];
	const int ysiz2 = tilesizy[tilenume2];
	
	if ((xsiz1 > 0) && (ysiz1 > 0) && (xsiz2 > 0) && (ysiz2 > 0))
	{
		if (waloff[tilenume1] == 0) {
			loadtile(tilenume1);
		}

		if (waloff[tilenume2] == 0) {
			loadtile(tilenume2);
		}

		int x1{sx1};

		for(int i{0}; i < xsiz; ++i)
		{
			int y1{sy1};
			
			for(int j{0}; j < ysiz; ++j)
			{
				const int x2 = sx2 + i;
				const int y2 = sy2 + j;

				if ((x2 >= 0) && (y2 >= 0) && (x2 < xsiz2) && (y2 < ysiz2))
				{
					auto* ptr1 = (unsigned char *)(waloff[tilenume1] + x1 * ysiz1 + y1);
					auto* ptr2 = (unsigned char *)(waloff[tilenume2] + x2 * ysiz2 + y2);
					const auto dat = *ptr1;
					
					if (dat != 255)
						*ptr2 = *ptr1;
				}

				y1++;
				
				if (y1 >= ysiz1)
					y1 = 0;
			}
			
			++x1;
			
			if (x1 >= xsiz1)
				x1 = 0;
		}
	}
}


//
// qloadkvx
//
int qloadkvx(int voxindex, const std::string& filename)
{
	int fil{0};

	if ((fil = kopen4load(filename.c_str(), 0)) == -1) {
		return -1;
	}

	int lengcnt{0};
	const int lengtot = kfilelength(fil);

	for(int i{0}; i < MAXVOXMIPS; ++i)
	{
		int dasiz{0};
		kread(fil, &dasiz, 4);
			//Must store filenames to use cacheing system :(
		voxlock[voxindex][i] = 200;
		allocache((void **)&voxoff[voxindex][i],dasiz,&voxlock[voxindex][i]);
		auto* ptr = (unsigned char *)voxoff[voxindex][i];
		kread(fil, ptr, dasiz);

		lengcnt += dasiz + 4;
		
		if (lengcnt >= lengtot - 768) {
			break;
		}
	}

	kclose(fil);

#if USE_POLYMOST && USE_OPENGL
	if (voxmodels[voxindex]) {
		voxfree(voxmodels[voxindex].get());
		voxmodels[voxindex].reset();
	}
	voxmodels[voxindex] = voxload(filename.c_str());
#endif
	return 0;
}


//
// clipinsidebox
//
int clipinsidebox(int x, int y, short wallnum, int walldist)
{
	const int r = walldist << 1;
	
	walltype* wal = &wall[wallnum];

	const int x1 = wal->pt.x + walldist - x;
	const int y1 = wal->pt.y + walldist - y;

	wal = &wall[wal->point2];

	int x2 = wal->pt.x+walldist - x;
	int y2 = wal->pt.y+walldist - y;

	if (((x1 < 0)  && (x2 < 0))  || 
		((y1 < 0)  && (y2 < 0))  ||
	    ((x1 >= r) && (x2 >= r)) ||
		((y1 >= r) && (y2 >= r)))
		return 0;

	x2 -= x1;
	y2 -= y1;

	if (x2 * (walldist - y1) >= y2 * (walldist - x1))  //Front
	{
		if (x2 > 0) {
			x2 *= (0 - y1);
		}
		else {
			x2 *= (r - y1);
		}

		if (y2 > 0) {
			y2 *= (r - x1);
		}
		else {
			y2 *= (0 - x1);
		}

		return x2 < y2;
	}

	if (x2 > 0) {
		x2 *= (r - y1);
	}
	else {
		x2 *= (0 - y1);
	}

	if (y2 > 0) {
		y2 *= (0 - x1);
	}
	else {
		y2 *= (r - x1);
	}

	return (x2 >= y2) << 1;
}


//
// clipinsideboxline
//
int clipinsideboxline(int x, int y, int x1, int y1, int x2, int y2, int walldist)
{
	const int r = walldist << 1;

	x1 += walldist-x;
	x2 += walldist-x;

	if ((x1 < 0) && (x2 < 0)) {
		return 0;
	}

	if ((x1 >= r) && (x2 >= r)) {
		return 0;
	}

	y1 += walldist - y;
	y2 += walldist - y;

	if ((y1 < 0) && (y2 < 0)) {
		return 0;
	}

	if ((y1 >= r) && (y2 >= r)) {
		return 0;
	}

	x2 -= x1;
	y2 -= y1;

	if (x2 * (walldist - y1) >= y2 * (walldist - x1))  //Front
	{
		if (x2 > 0) {
			x2 *= (0 - y1);
		}
		else {
			x2 *= (r - y1);
		}

		if (y2 > 0) {
			y2 *= (r - x1);
		}
		else {
			y2 *= (0 - x1);
		}

		return x2 < y2;
	}

	if (x2 > 0) {
		x2 *= (r - y1);
	}
	else {
		x2 *= (0 - y1);
	}

	if (y2 > 0) {
		y2 *= (0 - x1);
	}
	else {
		y2 *= (r - x1);
	}

	return (x2 >= y2) << 1;
}


//
// inside
//
int inside(int x, int y, short sectnum)
{
	if ((sectnum < 0) || (sectnum >= numsectors)) {
		return -1;
	}

	unsigned int cnt{ 0 };

	walltype* wal = &wall[g_sector[sectnum].wallptr];

	int i = g_sector[sectnum].wallnum;

	do
	{
		const int y1 = wal->pt.y - y;
		const int y2 = wall[wal->point2].pt.y - y;

		if ((y1 ^ y2) < 0)
		{
			const int x1 = wal->pt.x - x;
			const int x2 = wall[wal->point2].pt.x - x;

			if ((x1 ^ x2) >= 0) {
				cnt ^= x1;
			}
			else {
				cnt ^= (x1 * y2 - x2 * y1) ^ y2;
			}
		}

		++wal;
		--i;
	} while (i);
	
	return cnt >> 31;
}


//
// getangle
//
 // FIXME: Shifting on boolean results
int getangle(int xvect, int yvect)
{
	if ((xvect|yvect) == 0)
		return 0;

	if (xvect == 0)
		return 512 + ((yvect < 0) << 10);

	if (yvect == 0)
		return (xvect < 0) << 10;

	if (xvect == yvect)
		return 256 + ((xvect < 0) << 10);

	if (xvect == -yvect)
		return 768 + ((xvect > 0) << 10);

	if (std::abs(xvect) > std::abs(yvect))
		return ((radarang[640 + scale(160, yvect, xvect)] >> 6) + ((xvect < 0) << 10)) & 2047;

	return ((radarang[640 - scale(160, xvect, yvect)] >> 6) + 512 + ((yvect < 0) << 10)) & 2047;
}

//
// krecip
//
int krecip(int num)
{
	return krecipasm(num);
}


//
// setsprite
//
int setsprite(short spritenum, int newx, int newy, int newz)
{
	sprite[spritenum].x = newx;
	sprite[spritenum].y = newy;
	sprite[spritenum].z = newz;

	short tempsectnum = sprite[spritenum].sectnum;

	updatesector(newx, newy, &tempsectnum);
	
	if (tempsectnum < 0) {
		return -1;
	}

	if (tempsectnum != sprite[spritenum].sectnum) {
		changespritesect(spritenum, tempsectnum);
	}

	return 0;
}

//
// setspritez
//
int setspritez(short spritenum, int newx, int newy, int newz)
{
	sprite[spritenum].x = newx;
	sprite[spritenum].y = newy;
	sprite[spritenum].z = newz;

	short tempsectnum = sprite[spritenum].sectnum;

	updatesectorz(newx, newy, newz, &tempsectnum);

	if (tempsectnum < 0) {
		return -1;
	}

	if (tempsectnum != sprite[spritenum].sectnum) {
		changespritesect(spritenum, tempsectnum);
	}

	return 0;
}


//
// insertsprite
//
int insertsprite(short sectnum, short statnum)
{
	insertspritestat(statnum);
	
	return insertspritesect(sectnum);
}


//
// deletesprite
//
int deletesprite(short spritenum)
{
	deletespritestat(spritenum);

	return deletespritesect(spritenum);
}


//
// changespritesect
//
int changespritesect(short spritenum, short newsectnum)
{
	if ((newsectnum < 0) || (newsectnum > MAXSECTORS)) {
		return -1;
	}

	if (sprite[spritenum].sectnum == newsectnum) {
		return 0;
	}

	if (sprite[spritenum].sectnum == MAXSECTORS) {
		return -1;
	}

	if (deletespritesect(spritenum) < 0) {
		return -1;
	}

	insertspritesect(newsectnum);

	return 0;
}


//
// changespritestat
//
int changespritestat(short spritenum, short newstatnum)
{
	if ((newstatnum < 0) || (newstatnum > MAXSTATUS)) {
		return -1;
	}

	if (sprite[spritenum].statnum == newstatnum) {
		return 0;
	}

	if (sprite[spritenum].statnum == MAXSTATUS) {
		return -1;
	}

	if (deletespritestat(spritenum) < 0) {
		return -1;
	}

	insertspritestat(newstatnum);

	return 0;
}


//
// nextsectorneighborz
//
int nextsectorneighborz(short sectnum, int thez, short topbottom, short direction)
{
	int nextz = 0x80000000; // FIXME: Is overflow correct?

	if (direction == 1) {
		nextz = 0x7fffffff;
	}

	short sectortouse{ -1 };

	std::ranges::subrange wallrange{&wall[g_sector[sectnum].wallptr],
								    &wall[g_sector[sectnum].wallptr + g_sector[sectnum].wallnum]};

	for(const auto& wal : wallrange)
	{
		if (wal.nextsector >= 0)
		{
			if (topbottom == 1)
			{
				const int testz = g_sector[wal.nextsector].floorz;

				if (direction == 1)
				{
					if ((testz > thez) && (testz < nextz))
					{
						nextz = testz;
						sectortouse = wal.nextsector;
					}
				}
				else
				{
					if ((testz < thez) && (testz > nextz))
					{
						nextz = testz;
						sectortouse = wal.nextsector;
					}
				}
			}
			else
			{
				const int testz = g_sector[wal.nextsector].ceilingz;
				if (direction == 1)
				{
					if ((testz > thez) && (testz < nextz))
					{
						nextz = testz;
						sectortouse = wal.nextsector;
					}
				}
				else
				{
					if ((testz < thez) && (testz > nextz))
					{
						nextz = testz;
						sectortouse = wal.nextsector;
					}
				}
			}
		}
	}

	return sectortouse;
}


//
// cansee
//
bool cansee(int x1, int y1, int z1, short sect1, int x2, int y2, int z2, short sect2)
{
	if ((x1 == x2) && (y1 == y2)) {
		return sect1 == sect2;
	}

	const int x21 = x2 - x1;
	const int y21 = y2 - y1;
	const int z21 = z2 - z1;

	clipsectorlist[0] = sect1;
	int danum{1};

	for(int dacnt{0}; dacnt < danum; ++dacnt)
	{
		const int dasectnum = clipsectorlist[dacnt];
		const auto* sec = &g_sector[dasectnum];

		int cnt = sec->wallnum;

		for(auto* wal = &wall[sec->wallptr]; cnt > 0; --cnt, ++wal)
		{
			auto* wal2 = &wall[wal->point2];
			const int x31 = wal->pt.x-x1;
			const int x34 = wal->pt.x-wal2->pt.x;
			const int y31 = wal->pt.y-y1;
			const int y34 = wal->pt.y-wal2->pt.y;

			const int bot = y21 * x34 - x21 * y34;
			
			if (bot <= 0)
				continue;

			int t = y21 * x31 - x21 * y31;
			
			if ((unsigned)t >= (unsigned)bot)
				continue;
			
			t = y31 * x34 - x31 * y34;

			if ((unsigned)t >= (unsigned)bot)
				continue;

			const int nexts = wal->nextsector;
			
			if ((nexts < 0) || (wal->cstat & 32))
				return 0;

			t = divscalen<24>(t,bot);
			const int x = x1 + mulscalen<24>(x21, t);
			const int y = y1 + mulscalen<24>(y21, t);
			const int z = z1 + mulscalen<24>(z21, t);

			auto cfz = getzsofslope((short)dasectnum, x, y);

			if ((z <= cfz.ceilz) || (z >= cfz.floorz))
				return false;
			
			auto nextcfz = getzsofslope((short)nexts, x, y);
			
			if ((z <= nextcfz.ceilz) || (z >= nextcfz.floorz))
				return false;

			int i{0};

			for(i=danum-1;i>=0;i--) {
				if (clipsectorlist[i] == nexts)
					break;
			}

			if (i < 0)
				clipsectorlist[danum++] = nexts;
		}
	}

	for (int i = danum - 1; i >= 0; i--) {
		if (clipsectorlist[i] == sect2) {
			return true;
		}
	}

	return false;
}


//
// hitscan
//
int hitscan(int xs, int ys, int zs, short sectnum, int vx, int vy, int vz,
	short *hitsect, short *hitwall, short *hitsprite,
	int *hitx, int *hity, int *hitz, unsigned int cliptype)
{
	static constexpr int hitscangoalx = (1 << 29) - 1;
	static constexpr int hitscangoaly = (1 << 29) - 1;

	*hitsect = -1;
	*hitwall = -1;
	*hitsprite = -1;

	if (sectnum < 0)
		return -1;

	*hitx = hitscangoalx;
	*hity = hitscangoaly;

	const auto dawalclipmask = cliptype & 65535;
	const auto dasprclipmask = cliptype >> 16;

	clipsectorlist[0] = sectnum;
	short tempshortcnt{0};
	short tempshortnum{1};
	
	int y1{0};
	int z1{0};
	
	do
	{
		const auto dasector = clipsectorlist[tempshortcnt];
		auto* sec = &g_sector[dasector];

		int x1 = 0x7fffffff;

		if (sec->ceilingstat&2)
		{
			const auto* wal = &wall[sec->wallptr];
			const auto* wal2 = &wall[wal->point2];
			auto dax = wal2->pt.x-wal->pt.x;
			auto day = wal2->pt.y-wal->pt.y;
			int i = static_cast<int>(std::hypot(dax, day)); 
			
			if (i == 0) {
				continue;
			}
			
			i = divscalen<15>(sec->ceilingheinum, i);

			dax *= i;
			day *= i;

			const int j = (vz << 8) - dmulscalen<15>(dax, vy, -day, vx);
			
			if (j != 0)
			{
				i = ((sec->ceilingz-zs)<<8)+dmulscalen<15>(dax,ys-wal->pt.y,-day,xs-wal->pt.x);
				if (((i^j) >= 0) && ((std::abs(i)>>1) < std::abs(j))) {
					i = divscalen<30>(i,j);
					x1 = xs + mulscalen<30>(vx,i);
					y1 = ys + mulscalen<30>(vy,i);
					z1 = zs + mulscalen<30>(vz,i);
				}
			}
		}
		else if ((vz < 0) && (zs >= sec->ceilingz))
		{
			z1 = sec->ceilingz;
			int i = z1 - zs;
			
			if ((std::abs(i)>>1) < -vz) {
				i = divscalen<30>(i,vz);
				x1 = xs + mulscalen<30>(vx,i);
				y1 = ys + mulscalen<30>(vy,i);
			}
		}

		if ((x1 != 0x7fffffff) && (std::abs(x1-xs)+std::abs(y1-ys) < std::abs((*hitx)-xs)+std::abs((*hity)-ys)))
			if (inside(x1,y1,dasector) != 0) {
				*hitsect = dasector;
				*hitwall = -1;
				*hitsprite = -1;
				*hitx = x1;
				*hity = y1;
				*hitz = z1;
			}

		x1 = 0x7fffffff;
		
		if (sec->floorstat & 2)
		{
			const auto* wal = &wall[sec->wallptr];
			const auto* wal2 = &wall[wal->point2];
			int dax = wal2->pt.x-wal->pt.x;
			int day = wal2->pt.y-wal->pt.y;
			int i = static_cast<int>(std::hypot(dax, day));
			
			if (i == 0) {
				continue;
			}
			
			i = divscalen<15>(sec->floorheinum,i);
			dax *= i;
			day *= i;

			const int j = (vz<<8) - dmulscalen<15>(dax, vy, -day, vx);
			if (j != 0)
			{
				i = ((sec->floorz-zs)<<8)+dmulscalen<15>(dax,ys-wal->pt.y,-day,xs-wal->pt.x);
				if (((i^j) >= 0) && ((std::abs(i)>>1) < std::abs(j)))
				{
					i = divscalen<30>(i,j);
					x1 = xs + mulscalen<30>(vx,i);
					y1 = ys + mulscalen<30>(vy,i);
					z1 = zs + mulscalen<30>(vz,i);
				}
			}
		}
		else if ((vz > 0) && (zs <= sec->floorz))
		{
			z1 = sec->floorz;
			int i = z1 - zs;

			if ((std::abs(i) >> 1) < vz)
			{
				i = divscalen<30>(i, vz);
				x1 = xs + mulscalen<30>(vx, i);
				y1 = ys + mulscalen<30>(vy, i);
			}
		}

		if ((x1 != 0x7fffffff) && (std::abs(x1-xs)+std::abs(y1-ys) < std::abs((*hitx)-xs)+std::abs((*hity)-ys)))
			if (inside(x1,y1,dasector) != 0)
			{
				*hitsect = dasector;
				*hitwall = -1;
				*hitsprite = -1;
				*hitx = x1;
				*hity = y1;
				*hitz = z1;
			}

		const auto startwall = sec->wallptr;
		const auto endwall = startwall + sec->wallnum;

		const auto* wal = &wall[startwall];
		for(int z{startwall}; z < endwall; ++z, ++wal)
		{
			const auto* wal2 = &wall[wal->point2];
			x1 = wal->pt.x;
			y1 = wal->pt.y;
			const int x2 = wal2->pt.x;
			const int y2 = wal2->pt.y;

			if ((x1-xs)*(y2-ys) < (x2-xs)*(y1-ys)) continue;

			int intx{0};
			int inty{0};
			int intz{0};
			if (!rintersect(xs,ys,zs,vx,vy,vz,x1,y1,x2,y2,&intx,&inty,&intz))
				continue;

			if (std::abs(intx-xs)+std::abs(inty-ys) >= std::abs((*hitx)-xs)+std::abs((*hity)-ys)) continue;

			const auto nextsector = wal->nextsector;
			
			if ((nextsector < 0) || (wal->cstat&dawalclipmask))
			{
				*hitsect = dasector;
				*hitwall = z;
				*hitsprite = -1;
				*hitx = intx;
				*hity = inty;
				*hitz = intz;
				continue;
			}
			
			auto dacfz = getzsofslope(nextsector, intx, inty);
			
			if ((intz <= dacfz.ceilz) || (intz >= dacfz.floorz))
			{
				*hitsect = dasector;
				*hitwall = z;
				*hitsprite = -1;
				*hitx = intx;
				*hity = inty;
				*hitz = intz;
				continue;
			}

			int zz{0};
			for(zz = tempshortnum - 1; zz >= 0; --zz) {
				if (clipsectorlist[zz] == nextsector)
					break;
			}

			if (zz < 0)
				clipsectorlist[tempshortnum++] = nextsector;
		}

		for(auto z = headspritesect[dasector]; z >= 0; z = nextspritesect[z])
		{
			const auto* spr = &sprite[z];
			const auto cstat = spr->cstat;
#if USE_POLYMOST
			// TODO: Is the scope of this the entirety of what's below it?
			if (!hitallsprites)
#endif
			if ((cstat&dasprclipmask) == 0)
				continue;

			x1 = spr->x;
			y1 = spr->y;
			z1 = spr->z;
			
			switch(cstat & 48)
			{
				case 0: {
					const int topt = vx*(x1-xs) + vy*(y1-ys);
					
					if (topt <= 0)
						continue;
					
					const int bot = vx*vx + vy*vy;
					
					if (bot == 0)
						continue;

					const int intz = zs+scale(vz, topt, bot);

					int i = (tilesizy[spr->picnum]*spr->yrepeat<<2);
					
					if (cstat & 128)
						z1 += (i>>1);

					if (picanm[spr->picnum]&0x00ff0000)
						z1 -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
					
					if ((intz > z1) || (intz < z1-i))
						continue;
					
					const int topu = vx*(y1-ys) - vy*(x1-xs);

					const int offx = scale(vx, topu, bot);
					const int offy = scale(vy, topu, bot);
					const int dist = offx * offx + offy * offy;
					i = tilesizx[spr->picnum]*spr->xrepeat;
					i *= i;
					
					if (dist > (i>>7))
						continue;

					const int intx = xs + scale(vx,topt,bot);
					const int inty = ys + scale(vy,topt,bot);

					if (std::abs(intx - xs) + std::abs(inty - ys) > std::abs((*hitx) - xs) + std::abs((*hity) - ys)) continue;

					*hitsect = dasector;
					*hitwall = -1;
					*hitsprite = z;
					*hitx = intx;
					*hity = inty;
					*hitz = intz;
					break;
				}
				case 16: {
						//These lines get the 2 points of the rotated sprite
						//Given: (x1, y1) starts out as the center point
					const auto tilenum = spr->picnum;

					int xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);

					if ((cstat&4) > 0)
						xoff = -xoff;

					int k = spr->ang;
					int l = spr->xrepeat;
					const int dax = sintable[k & 2047] * l;
					const int day = sintable[(k + 1536) & 2047] * l;
					l = tilesizx[tilenum];
					k = (l>>1)+xoff;
					x1 -= mulscalen<16>(dax, k);
					const int x2 = x1 + mulscalen<16>(dax, l);
					y1 -= mulscalen<16>(day, k);
					const int y2 = y1 + mulscalen<16>(day, l);

					if ((cstat&64) != 0)   //back side of 1-way sprite
						if ((x1-xs)*(y2-ys) < (x2-xs)*(y1-ys))
							continue;

					int intx{0};
					int inty{0};
					int intz{0};
					if (!rintersect(xs,ys,zs,vx,vy,vz,x1,y1,x2,y2,&intx,&inty,&intz))
						continue;

					if (std::abs(intx-xs)+std::abs(inty-ys) > std::abs((*hitx)-xs)+std::abs((*hity)-ys))
						continue;

					k = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);

					int daz{0};
					if (cstat&128)
						daz = spr->z+(k>>1);
					else
						daz = spr->z;

					if (picanm[spr->picnum]&0x00ff0000)
						daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);

					if ((intz < daz) && (intz > daz-k))
					{
						*hitsect = dasector;
						*hitwall = -1;
						*hitsprite = z;
						*hitx = intx;
						*hity = inty;
						*hitz = intz;
					}

					break;
				}
				case 32: {
					if (vz == 0)
						continue;
					
					const int intz{z1};
					
					if (((intz-zs)^vz) < 0)
						continue;

					if ((cstat&64) != 0)
						if ((zs > intz) == ((cstat&8)==0))
							continue;

					const int intx = xs+scale(intz - zs, vx, vz);
					const int inty = ys+scale(intz - zs, vy, vz);

					if (std::abs(intx-xs)+std::abs(inty-ys) > std::abs((*hitx)-xs)+std::abs((*hity)-ys))
						continue;

					const auto tilenum = spr->picnum;
					int xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
					int yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
					
					if ((cstat&4) > 0)
						xoff = -xoff;

					if ((cstat&8) > 0)
						yoff = -yoff;

					const auto ang = spr->ang;
					const auto cosang = sintable[(ang+512)&2047];
					const auto sinang = sintable[ang];
					const auto xspan = tilesizx[tilenum];
					const auto xrepeat = spr->xrepeat;
					const auto yspan = tilesizy[tilenum];
					const auto yrepeat = spr->yrepeat;

					const int dax = ((xspan>>1)+xoff) * xrepeat;
					const int day = ((yspan>>1)+yoff) * yrepeat;
					x1 += dmulscalen<16>(sinang, dax, cosang, day)-intx;
					y1 += dmulscalen<16>(sinang, day, -cosang, dax)-inty;
					int l = xspan * xrepeat;
					const int x2 = x1 - mulscalen<16>(sinang,l);
					const int y2 = y1 + mulscalen<16>(cosang,l);
					l = yspan * yrepeat;
					int k = -mulscalen<16>(cosang,l);
					const int x3 = x2 + k;
					const int x4 = x1 + k;
					k = -mulscalen<16>(sinang, l);
					const int y3 = y2 + k;
					const int y4 = y1 + k;

					unsigned char clipyou{0}; // TODO: Switch to bool
					if ((y1^y2) < 0)
					{
						if ((x1^x2) < 0) clipyou ^= (x1*y2<x2*y1)^(y1<y2);
						else if (x1 >= 0) clipyou ^= 1;
					}
					if ((y2^y3) < 0)
					{
						if ((x2^x3) < 0) clipyou ^= (x2*y3<x3*y2)^(y2<y3);
						else if (x2 >= 0) clipyou ^= 1;
					}
					if ((y3^y4) < 0)
					{
						if ((x3^x4) < 0) clipyou ^= (x3*y4<x4*y3)^(y3<y4);
						else if (x3 >= 0) clipyou ^= 1;
					}
					if ((y4^y1) < 0)
					{
						if ((x4^x1) < 0) clipyou ^= (x4*y1<x1*y4)^(y4<y1);
						else if (x4 >= 0) clipyou ^= 1;
					}

					if (clipyou != 0)
					{
						*hitsect = dasector;
						*hitwall = -1;
						*hitsprite = z;
						*hitx = intx;
						*hity = inty;
						*hitz = intz;
					}

					break;
				}
			}
		}

		tempshortcnt++;
	} while (tempshortcnt < tempshortnum);

	return 0;
}


//
// neartag
//
int neartag(int xs, int ys, int zs, short sectnum, short ange, short *neartagsector, short *neartagwall,
	short *neartagsprite, int *neartaghitdist, int neartagrange, unsigned char tagsearch)
{
	walltype* wal;
	walltype* wal2;
	spritetype* spr;
	int i;
	int z;
	int zz;
	int x1;
	int y1;
	int z1;
	int x2;
	int y2;
	int intx;
	int inty;
	int intz;
	int topt;
	int topu;
	int bot;
	int dist;
	int offx;
	int offy;
	short dasector;
	short startwall;
	short endwall;
	short nextsector;
	short good;

	*neartagsector = -1;
	*neartagwall = -1;
	*neartagsprite = -1;
	*neartaghitdist = 0;

	if (sectnum < 0)
		return 0;

	if ((tagsearch < 1) || (tagsearch > 3))
		return 0;

	const int vx = mulscalen<14>(sintable[(ange + 2560) & 2047], neartagrange);
	int xe = xs + vx;
	const int vy = mulscalen<14>(sintable[(ange + 2048) & 2047], neartagrange);
	int ye = ys + vy;
	const int vz{0};
	int ze{0};

	clipsectorlist[0] = sectnum;

	short tempshortcnt{0};
	short tempshortnum{1};

	do
	{
		dasector = clipsectorlist[tempshortcnt];

		startwall = g_sector[dasector].wallptr;
		endwall = startwall + g_sector[dasector].wallnum - 1;

		for(z=startwall,wal=&wall[startwall];z<=endwall;z++,wal++)
		{
			wal2 = &wall[wal->point2];
			x1 = wal->pt.x;
			y1 = wal->pt.y;
			x2 = wal2->pt.x;
			y2 = wal2->pt.y;

			nextsector = wal->nextsector;

			good = 0;
			if (nextsector >= 0)
			{
				if ((tagsearch&1) && g_sector[nextsector].lotag) good |= 1;
				if ((tagsearch&2) && g_sector[nextsector].hitag) good |= 1;
			}

			if ((tagsearch&1) && wal->lotag)
				good |= 2;

			if ((tagsearch&2) && wal->hitag)
				good |= 2;

			if ((good == 0) && (nextsector < 0))
				continue;

			if ((x1-xs)*(y2-ys) < (x2-xs)*(y1-ys))
				continue;

			if (lintersect(xs,ys,zs,xe,ye,ze,x1,y1,x2,y2,&intx,&inty,&intz))
			{
				if (good != 0)
				{
					if (good & 1)
						*neartagsector = nextsector;
					
					if (good&2)
						*neartagwall = z;

					*neartaghitdist = dmulscalen<14>(intx - xs,sintable[(ange + 2560) & 2047], inty - ys,sintable[(ange + 2048) & 2047]);
					
					xe = intx;
					ye = inty;
					ze = intz;
				}

				if (nextsector >= 0)
				{
					for(zz=tempshortnum-1;zz>=0;zz--)
						if (clipsectorlist[zz] == nextsector)
							break;

					if (zz < 0)
						clipsectorlist[tempshortnum++] = nextsector;
				}
			}
		}

		for(z=headspritesect[dasector];z>=0;z=nextspritesect[z])
		{
			spr = &sprite[z];

			good = 0;
			if ((tagsearch&1) && spr->lotag) good |= 1;
			if ((tagsearch&2) && spr->hitag) good |= 1;
			if (good != 0)
			{
				x1 = spr->x;
				y1 = spr->y;
				z1 = spr->z;

				topt = vx*(x1-xs) + vy*(y1-ys);
				if (topt > 0)
				{
					bot = vx*vx + vy*vy;
					if (bot != 0)
					{
						intz = zs+scale(vz,topt,bot);
						i = tilesizy[spr->picnum]*spr->yrepeat;
						if (spr->cstat&128) z1 += (i<<1);
						if (picanm[spr->picnum]&0x00ff0000) z1 -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
						if ((intz <= z1) && (intz >= z1-(i<<2)))
						{
							topu = vx*(y1-ys) - vy*(x1-xs);

							offx = scale(vx,topu,bot);
							offy = scale(vy,topu,bot);
							dist = offx*offx + offy*offy;
							i = (tilesizx[spr->picnum]*spr->xrepeat); i *= i;
							if (dist <= (i>>7))
							{
								intx = xs + scale(vx,topt,bot);
								inty = ys + scale(vy,topt,bot);
								if (std::abs(intx-xs)+std::abs(inty-ys) < std::abs(xe-xs)+std::abs(ye-ys))
								{
									*neartagsprite = z;
									*neartaghitdist = dmulscalen<14>(intx-xs,sintable[(ange+2560)&2047],inty-ys,sintable[(ange+2048)&2047]);
									xe = intx;
									ye = inty;
									ze = intz;
								}
							}
						}
					}
				}
			}
		}

		tempshortcnt++;
	} while (tempshortcnt < tempshortnum);
	
	return 0;
}


//
// dragpoint
//
void dragpoint(short pt_highlight, int dax, int day)
{
	wall[pt_highlight].pt.x = dax;
	wall[pt_highlight].pt.y = day;

	short cnt{MAXWALLS};
	short tempshort{pt_highlight};    //search points CCW
	
	do
	{
		if (wall[tempshort].nextwall >= 0)
		{
			tempshort = wall[wall[tempshort].nextwall].point2;
			wall[tempshort].pt.x = dax;
			wall[tempshort].pt.y = day;
		}
		else
		{
			tempshort = pt_highlight;    //search points CW if not searched all the way around
			do
			{
				if (wall[lastwall(tempshort)].nextwall >= 0)
				{
					tempshort = wall[lastwall(tempshort)].nextwall;
					wall[tempshort].pt.x = dax;
					wall[tempshort].pt.y = day;
				}
				else
				{
					break;
				}
				cnt--;
			} while ((tempshort != pt_highlight) && (cnt > 0));
			
			break;
		}

		cnt--;
	} while ((tempshort != pt_highlight) && (cnt > 0));
}


//
// lastwall
//
int lastwall(short point)
{
	if ((point > 0) && (wall[point - 1].point2 == point))
		return point - 1;

	int i{point};

	int cnt{MAXWALLS};

	do
	{
		const int j = wall[i].point2;

		if (j == point)
			return i;

		i = j;
		cnt--;
	} while (cnt > 0);

	return point;
}



#define addclipline(dax1, day1, dax2, day2, daoval)      \
{                                                        \
	if (clipnum < MAXCLIPNUM) { \
	clipit[clipnum].x1 = dax1; clipit[clipnum].y1 = day1; \
	clipit[clipnum].x2 = dax2; clipit[clipnum].y2 = day2; \
	clipobjectval[clipnum] = daoval;                      \
	clipnum++;                                            \
	}                           \
}                                                        \

int clipmoveboxtracenum = 3;

//
// clipmove
//
int clipmove (int *x, int *y, const int *z, short *sectnum,
		 int xvect, int yvect,
		 int walldist, int ceildist, int flordist, unsigned int cliptype)
{
	if (((xvect | yvect) == 0) || (*sectnum < 0)) {
		return 0;
	}

	int retval{0};

	const int oxvect{ xvect };
	const int oyvect{ yvect };

	int goalx = (*x) + (xvect >> 14);
	int goaly = (*y) + (yvect >> 14);

	clipnum = 0;

	const int cx = ((*x) + goalx) >> 1;
	const int cy = ((*y) + goaly) >> 1;
		//Extra walldist for sprites on sector lines
	const int gx = goalx - (*x);
	const int gy = goaly - (*y);
	const int rad = static_cast<int>(std::hypot(gx, gy)) + MAXCLIPDIST + walldist + 8;
	const int xmin = cx - rad;
	const int ymin = cy - rad;
	const int xmax = cx + rad;
	const int ymax = cy + rad;

	const int dawalclipmask = cliptype & 65535;        //CLIPMASK0 = 0x00010001
	const int dasprclipmask = cliptype >> 16;          //CLIPMASK1 = 0x01000040

	clipsectorlist[0] = (*sectnum);

	int clipsectcnt{0};
	clipsectnum = 1;

	do
	{
		const int dasect = clipsectorlist[clipsectcnt++];
		const sectortype* sec = &g_sector[dasect];
		const int startwall = sec->wallptr;
		const int endwall = startwall + sec->wallnum;
		auto* wal = &wall[startwall];
		
		for(int j{startwall}; j < endwall; ++j, ++wal)
		{
			auto* wal2 = &wall[wal->point2];

			if ((wal->pt.x < xmin) && (wal2->pt.x < xmin))
				continue;
			if ((wal->pt.x > xmax) && (wal2->pt.x > xmax))
				continue;
			if ((wal->pt.y < ymin) && (wal2->pt.y < ymin))
				continue;
			if ((wal->pt.y > ymax) && (wal2->pt.y > ymax))
				continue;

			const int x1 = wal->pt.x;
			const int y1 = wal->pt.y;
			const int x2 = wal2->pt.x;
			const int y2 = wal2->pt.y;

			const int dx = x2-x1;
			const int dy = y2-y1;

			if (dx*((*y)-y1) < ((*x)-x1)*dy) continue;  //If wall's not facing you

			int dax{0};
			if (dx > 0)
				dax = dx*(ymin-y1);
			else
				dax = dx*(ymax-y1);

			int day{0};
			if (dy > 0)
				day = dy*(xmax-x1);
			else
				day = dy*(xmin-x1);
			
			if (dax >= day)
				continue;

			int clipyou{0};
			if ((wal->nextsector < 0) || (wal->cstat & dawalclipmask))
				clipyou = 1;
			else if (!editstatus)
			{
				int daz{0};

				if (!rintersect(*x,*y,0,gx,gy,0,x1,y1,x2,y2,&dax,&day,&daz)) {
					dax = *x;
					day = *y;
				}

				daz = getflorzofslope((short)dasect,dax,day);
				int daz2 = getflorzofslope(wal->nextsector,dax,day);

				auto* sec2 = &g_sector[wal->nextsector];
				if (daz2 < daz-(1<<8))
					if ((sec2->floorstat&1) == 0)
						if ((*z) >= daz2-(flordist-1)) clipyou = 1;
				if (clipyou == 0)
				{
					daz = getceilzofslope((short)dasect,dax,day);
					daz2 = getceilzofslope(wal->nextsector,dax,day);
					if (daz2 > daz+(1<<8))
						if ((sec2->ceilingstat&1) == 0)
							if ((*z) <= daz2+(ceildist-1)) clipyou = 1;
				}
			}

			if (clipyou)
			{
					//Add 2 boxes at endpoints
				int bsz = walldist; if (gx < 0) bsz = -bsz;
				addclipline(x1-bsz,y1-bsz,x1-bsz,y1+bsz,(short)j+32768);
				addclipline(x2-bsz,y2-bsz,x2-bsz,y2+bsz,(short)j+32768);
				bsz = walldist; if (gy < 0) bsz = -bsz;
				addclipline(x1+bsz,y1-bsz,x1-bsz,y1-bsz,(short)j+32768);
				addclipline(x2+bsz,y2-bsz,x2-bsz,y2-bsz,(short)j+32768);

				dax = walldist; if (dy > 0) dax = -dax;
				day = walldist; if (dx < 0) day = -day;
				addclipline(x1+dax,y1+day,x2+dax,y2+day,(short)j+32768);
			}
			else
			{
				int i = clipsectnum - 1;
				for(; i >= 0; --i) {
					if (wal->nextsector == clipsectorlist[i])
						break;
				}

				if (i < 0)
					clipsectorlist[clipsectnum++] = wal->nextsector;
			}
		}

		for(int j = headspritesect[dasect]; j >= 0; j = nextspritesect[j])
		{
			auto* spr = &sprite[j];
			const short cstat = spr->cstat;

			if ((cstat&dasprclipmask) == 0)
				continue;

			int daz{0};
			int l{0};

			switch(cstat & 48) // FIXME: Cryptic
			{
				case 0: {
					const int x1 = spr->x;
					const int y1 = spr->y;
					if ((x1 >= xmin) && (x1 <= xmax) && (y1 >= ymin) && (y1 <= ymax))
					{
						const int k = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);
						if (cstat&128) daz = spr->z+(k>>1); else daz = spr->z;
						if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
						if (((*z) < daz+ceildist) && ((*z) > daz-k-flordist))
						{
							int bsz = (spr->clipdist<<2)+walldist;
							if (gx < 0)
								bsz = -bsz;
							addclipline(x1-bsz,y1-bsz,x1-bsz,y1+bsz,(short)j+49152);

							bsz = (spr->clipdist<<2)+walldist;
							if (gy < 0)
								bsz = -bsz;
							addclipline(x1+bsz,y1-bsz,x1-bsz,y1-bsz,(short)j+49152);
						}
					}
					break;
				}
				case 16: {
					int k = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);

					if (cstat&128)
						daz = spr->z+(k>>1);
					else
						daz = spr->z;

					if (picanm[spr->picnum]&0x00ff0000)
						daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
					int daz2 = daz - k;
					daz += ceildist;
					daz2 -= flordist;
					
					if (((*z) < daz) && ((*z) > daz2))
					{
							//These lines get the 2 points of the rotated sprite
							//Given: (x1, y1) starts out as the center point
						const int tilenum = spr->picnum;
						int xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						if ((cstat&4) > 0)
							xoff = -xoff;
						k = spr->ang;
						l = spr->xrepeat;
						int dax = sintable[k&2047]*l;
						int day = sintable[(k+1536)&2047]*l;
						l = tilesizx[tilenum];
						k = (l>>1)+xoff;
						int x1 = spr->x;
						int y1 = spr->y;
						x1 -= mulscalen<16>(dax,k);
						int x2 = x1+mulscalen<16>(dax,l);
						y1 -= mulscalen<16>(day,k);
						int y2 = y1+mulscalen<16>(day,l);
						
						if (clipinsideboxline(cx,cy,x1,y1,x2,y2,rad) != 0)
						{
							dax = mulscalen<14>(sintable[(spr->ang+256+512)&2047],walldist);
							day = mulscalen<14>(sintable[(spr->ang+256)&2047],walldist);

							if ((x1-(*x))*(y2-(*y)) >= (x2-(*x))*(y1-(*y)))   //Front
							{
								addclipline(x1+dax,y1+day,x2+day,y2-dax,(short)j+49152);
							}
							else
							{
								if ((cstat&64) != 0) continue;
								addclipline(x2-dax,y2-day,x1-day,y1+dax,(short)j+49152);
							}

								//Side blocker
							if ((x2-x1)*((*x)-x1) + (y2-y1)*((*y)-y1) < 0)
								{ addclipline(x1-day,y1+dax,x1+dax,y1+day,(short)j+49152); }
							else if ((x1-x2)*((*x)-x2) + (y1-y2)*((*y)-y2) < 0)
								{ addclipline(x2+day,y2-dax,x2-dax,y2-day,(short)j+49152); }
						}
					}
					break;
				}
				case 32: {
					const int x1 = spr->x;
					const int y1 = spr->y;
					daz = spr->z+ceildist;
					const int daz2 = spr->z-flordist;

					if (((*z) < daz) && ((*z) > daz2))
					{
						if ((cstat&64) != 0)
							if (((*z) > spr->z) == ((cstat&8)==0)) continue;

						int tilenum = spr->picnum;
						int xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						int yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
						
						if ((cstat&4) > 0)
							xoff = -xoff;

						if ((cstat&8) > 0)
							yoff = -yoff;

						int k = spr->ang;
						const int cosang = sintable[(k+512)&2047];
						const int sinang = sintable[k];
						const int xspan = tilesizx[tilenum];
						const int xrepeat = spr->xrepeat;
						const int yspan = tilesizy[tilenum];
						const int yrepeat = spr->yrepeat;

						int dax = ((xspan>>1)+xoff) * xrepeat;
						int day = ((yspan>>1)+yoff)*yrepeat;
						rxi[0] = x1 + dmulscalen<16>(sinang,dax,cosang,day);
						ryi[0] = y1 + dmulscalen<16>(sinang,day,-cosang,dax);
						l = xspan*xrepeat;
						rxi[1] = rxi[0] - mulscalen<16>(sinang,l);
						ryi[1] = ryi[0] + mulscalen<16>(cosang,l);
						l = yspan*yrepeat;
						k = -mulscalen<16>(cosang,l);
						rxi[2] = rxi[1]+k;
						rxi[3] = rxi[0]+k;
						k = -mulscalen<16>(sinang,l);
						ryi[2] = ryi[1]+k;
						ryi[3] = ryi[0]+k;

						dax = mulscalen<14>(sintable[(spr->ang-256+512)&2047],walldist);
						day = mulscalen<14>(sintable[(spr->ang-256)&2047],walldist);

						if ((rxi[0]-(*x))*(ryi[1]-(*y)) < (rxi[1]-(*x))*(ryi[0]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[1],ryi[1],rxi[0],ryi[0],rad) != 0)
								addclipline(rxi[1]-day,ryi[1]+dax,rxi[0]+dax,ryi[0]+day,(short)j+49152);
						}
						else if ((rxi[2]-(*x))*(ryi[3]-(*y)) < (rxi[3]-(*x))*(ryi[2]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[3],ryi[3],rxi[2],ryi[2],rad) != 0)
								addclipline(rxi[3]+day,ryi[3]-dax,rxi[2]-dax,ryi[2]-day,(short)j+49152);
						}

						if ((rxi[1]-(*x))*(ryi[2]-(*y)) < (rxi[2]-(*x))*(ryi[1]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[2],ryi[2],rxi[1],ryi[1],rad) != 0)
								addclipline(rxi[2]-dax,ryi[2]-day,rxi[1]-day,ryi[1]+dax,(short)j+49152);
						}
						else if ((rxi[3]-(*x))*(ryi[0]-(*y)) < (rxi[0]-(*x))*(ryi[3]-(*y)))
						{
							if (clipinsideboxline(cx,cy,rxi[0],ryi[0],rxi[3],ryi[3],rad) != 0)
								addclipline(rxi[0]+dax,ryi[0]+day,rxi[3]+day,ryi[3]-dax,(short)j+49152);
						}
					}
					break;
				}
			}
		}
	} while (clipsectcnt < clipsectnum);


	int hitwall{0};
	int cnt{clipmoveboxtracenum};
	
	do
	{
		int intx{goalx};
		int inty{goaly};

		if ((hitwall = raytrace(*x, *y, &intx, &inty)) >= 0)
		{
			const int lx = clipit[hitwall].x2-clipit[hitwall].x1;
			const int ly = clipit[hitwall].y2-clipit[hitwall].y1;
			int templong2 = lx * lx + ly * ly;

			if (templong2 > 0)
			{
				const int templong1 = (goalx - intx) * lx + (goaly - inty) * ly;
				
				int i{0};
				if ((std::abs(templong1) >> 11) < templong2)
					i = divscalen<20>(templong1, templong2);

				goalx = mulscalen<20>(lx, i) + intx;
				goaly = mulscalen<20>(ly, i) + inty;
			}

			const int templong1 = dmulscalen<6>(lx, oxvect, ly, oyvect);
			
			for(int i = cnt + 1; i <= clipmoveboxtracenum; ++i)
			{
				const int j = hitwalls[i];
				templong2 = dmulscalen<6>(clipit[j].x2-clipit[j].x1,oxvect,clipit[j].y2-clipit[j].y1,oyvect);
				if ((templong1 ^ templong2) < 0)
				{
					updatesector(*x,*y,sectnum);
					return retval;
				}
			}

			keepaway(&goalx, &goaly, hitwall);
			xvect = ((goalx-intx)<<14);
			yvect = ((goaly-inty)<<14);

			if (cnt == clipmoveboxtracenum)
				retval = clipobjectval[hitwall];

			hitwalls[cnt] = hitwall;
		}

		--cnt;

		*x = intx;
		*y = inty;
	} while (((xvect|yvect) != 0) && (hitwall >= 0) && (cnt > 0));

	for(int j{0}; j < clipsectnum; ++j) {
		if (inside(*x, *y, clipsectorlist[j]) == 1)
		{
			*sectnum = clipsectorlist[j];
			return retval;
		}
	}

	*sectnum = -1;
	int templong1 = 0x7fffffff;
	
	for(int j = numsectors - 1; j >= 0; --j) {
		if (inside(*x, *y, j) == 1)
		{
			int templong2{0};

			if (g_sector[j].ceilingstat&2)
				templong2 = (getceilzofslope((short)j,*x,*y)-(*z));
			else
				templong2 = (g_sector[j].ceilingz-(*z));

			if (templong2 > 0)
			{
				if (templong2 < templong1)
					{ *sectnum = j; templong1 = templong2; }
			}
			else
			{
				if (g_sector[j].floorstat&2)
					templong2 = ((*z)-getflorzofslope((short)j,*x,*y));
				else
					templong2 = ((*z)-g_sector[j].floorz);

				if (templong2 <= 0)
				{
					*sectnum = j;
					return retval;
				}
				if (templong2 < templong1) {
					*sectnum = j;
					templong1 = templong2;
				}
			}
		}
	}

	return retval;
}


//
// pushmove
//
int pushmove (int *x, int *y, const int *z, short *sectnum,
		 int walldist, int ceildist, int flordist, unsigned int cliptype)
{
	if ((*sectnum) < 0)
		return -1;

	const int dawalclipmask = cliptype & 65535;
	//int dasprclipmask = (cliptype>>16);

	int k{ 32 };
	int dir{ 1 };
	int bad{ 0 };

	do
	{
		bad = 0;
		
		clipsectorlist[0] = *sectnum;
		short clipsectcnt{0};
		clipsectnum = 1;
		
		do
		{
			/*Push FACE sprites
			for(i=headspritesect[clipsectorlist[clipsectcnt]];i>=0;i=nextspritesect[i])
			{
				spritetype *spr = &sprite[i];
				if (((spr->cstat&48) != 0) && ((spr->cstat&48) != 48)) continue;
				if ((spr->cstat&dasprclipmask) == 0) continue;

				dax = (*x)-spr->x; day = (*y)-spr->y;
				t = (spr->clipdist<<2)+walldist;
				if ((std::abs(dax) < t) && (std::abs(day) < t))
				{
					t = ((tilesizy[spr->picnum]*spr->yrepeat)<<2);
					if (spr->cstat&128) daz = spr->z+(t>>1); else daz = spr->z;
					if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
					if (((*z) < daz+ceildist) && ((*z) > daz-t-flordist))
					{
						t = (spr->clipdist<<2)+walldist;

						j = getangle(dax,day);
						dx = (sintable[(j+512)&2047]>>11);
						dy = (sintable[(j)&2047]>>11);
						bad2 = 16;
						do
						{
							*x = (*x) + dx; *y = (*y) + dy;
							bad2--; if (bad2 == 0) break;
						} while ((std::abs((*x)-spr->x) < t) && (std::abs((*y)-spr->y) < t));
						bad = -1;
						k--; if (k <= 0) return(bad);
						updatesector(*x,*y,sectnum);
					}
				}
			}*/

			auto* sec = &g_sector[clipsectorlist[clipsectcnt]];
			short startwall{0};
			short endwall{0};

			if (dir > 0) {
				startwall = sec->wallptr;
				endwall = startwall + sec->wallnum;
			}
			else {
				endwall = sec->wallptr;
				startwall = endwall + sec->wallnum;
			}

			auto* wal = &wall[startwall];
			for(int i{startwall}; i != endwall; i += dir, wal += dir) {
				if (clipinsidebox(*x,*y,i,walldist-4) == 1)
				{
					int j{0};
					
					if (wal->nextsector < 0)
						j = 1;
					
					if (wal->cstat&dawalclipmask)
						j = 1;

					if (j == 0)
					{
						auto* sec2 = &g_sector[wal->nextsector];


							//Find closest point on wall (dax, day) to (*x, *y)
						int dax = wall[wal->point2].pt.x - wal->pt.x;
						int day = wall[wal->point2].pt.y - wal->pt.y;
						int daz = dax * ((*x) - wal->pt.x) + day * ((*y) - wal->pt.y);
						int t{0};

						if (daz > 0)
						{
							const int daz2 = dax * dax + day * day;
							
							if (daz >= daz2)
								t = (1 << 30);
							else
								t = divscalen<30>(daz, daz2);
						}

						dax = wal->pt.x + mulscalen<30>(dax,t);
						day = wal->pt.y + mulscalen<30>(day,t);


						daz = getflorzofslope(clipsectorlist[clipsectcnt],dax,day);
						int daz2 = getflorzofslope(wal->nextsector,dax,day);

						if ((daz2 < daz-(1<<8)) && ((sec2->floorstat&1) == 0))
							if (*z >= daz2-(flordist-1))
								j = 1;

						daz = getceilzofslope(clipsectorlist[clipsectcnt],dax,day);
						daz2 = getceilzofslope(wal->nextsector,dax,day);
						if ((daz2 > daz+(1<<8)) && ((sec2->ceilingstat&1) == 0))
							if (*z <= daz2+(ceildist-1)) j = 1;
					}

					if (j != 0)
					{
						j = getangle(wall[wal->point2].pt.x-wal->pt.x,wall[wal->point2].pt.y-wal->pt.y);
						int dx = (sintable[(j+1024)&2047]>>11);
						int dy = (sintable[(j+512)&2047]>>11);
						char bad2 = 16;
						
						do
						{
							*x = (*x) + dx;
							*y = (*y) + dy;
							--bad2;

							if (bad2 == 0)
								break;
						} while (clipinsidebox(*x, *y, i, walldist - 4) != 0);
						
						bad = -1;
						k--; if (k <= 0) return(bad);
						updatesector(*x,*y,sectnum);
					}
					else
					{
						for(j=clipsectnum-1;j>=0;j--)
							if (wal->nextsector == clipsectorlist[j])
								break;

						if (j < 0)
							clipsectorlist[clipsectnum++] = wal->nextsector;
					}
				}
			}

			++clipsectcnt;
		} while (clipsectcnt < clipsectnum);

		dir = -dir;
	
	} while (bad != 0);

	return bad;
}


//
// updatesector[z]
//
void updatesector(int x, int y, short *sectnum)
{
	if (inside(x, y, *sectnum) == 1) {
		return;
	}

	if ((*sectnum >= 0) && (*sectnum < numsectors))
	{
		std::ranges::subrange wallrange{&wall[g_sector[*sectnum].wallptr],
		                                &wall[g_sector[*sectnum].wallptr + g_sector[*sectnum].wallnum]};

		for(const auto& wal : wallrange)
		{
			const int i = wal.nextsector;
			
			if (i >= 0)
				if (inside(x,y,(short)i) == 1)
				{
					*sectnum = i;
					return;
				}
		}
	}

	for(int i = numsectors - 1; i >= 0; --i) {
		if (inside(x, y, (short)i) == 1) {
			*sectnum = i;
			return;
		}
	}

	*sectnum = -1;
}

void updatesectorz(int x, int y, int z, short *sectnum)
{
	auto cfz = getzsofslope(*sectnum, x, y);

	if ((z >= cfz.ceilz) && (z <= cfz.floorz))
		if (inside(x,y,*sectnum) != 0)
			return;

	if ((*sectnum >= 0) && (*sectnum < numsectors))
	{
		std::ranges::subrange wallrange{&wall[g_sector[*sectnum].wallptr],
		                                &wall[g_sector[*sectnum].wallptr + g_sector[*sectnum].wallnum]};

		for(const auto& wal : wallrange)
		{
			const int i = wal.nextsector;

			if (i >= 0)
			{
				auto cfz = getzsofslope(i, x, y);
				if ((z >= cfz.ceilz) && (z <= cfz.floorz))
					if (inside(x,y,(short)i) == 1)
						{ *sectnum = i; return; }
			}
		}
	}

	for (int i = numsectors - 1; i >= 0; --i)
	{
		auto cfz = getzsofslope(i, x, y);

		if ((z >= cfz.ceilz) && (z <= cfz.floorz)) {
			if (inside(x, y, (short)i) == 1) {
				*sectnum = i;
				return;
			}
		}
	}

	*sectnum = -1;
}


//
// rotatepoint
//
void rotatepoint(int xpivot, int ypivot, int x, int y, short daang, int *x2, int *y2)
{
	const int dacos = sintable[(daang + 2560) & 2047];
	const int dasin = sintable[(daang + 2048) & 2047];

	x -= xpivot;
	y -= ypivot;
	
	*x2 = dmulscalen<14>(x, dacos, -y, dasin) + xpivot;
	*y2 = dmulscalen<14>(y, dacos, x, dasin) + ypivot;
}


//
// getmousevalues
//
void getmousevalues(int *mousx, int *mousy, int *bstatus)
{
	readmousexy(mousx,mousy);
	readmousebstatus(bstatus);
}


//
// krand
//
int krand()
{
	randomseed = (randomseed * 27584621) + 1;
	return(((unsigned int)randomseed) >> 16);
}


//
// getzrange
//
void getzrange(int x, int y, int z, short sectnum,
		 int *ceilz, int *ceilhit, int *florz, int *florhit,
		 int walldist, unsigned int cliptype)
{
	if (sectnum < 0)
	{
		*ceilz = 0x80000000;
		*ceilhit = -1;
		*florz = 0x7fffffff;
		*florhit = -1;
		
		return;
	}

		//Extra walldist for sprites on sector lines
	const int extdist = walldist + MAXCLIPDIST + 1;
	const int xmin = x - extdist;
	const int ymin = y - extdist;
	const int xmax = x + extdist;
	const int ymax = y + extdist;

	auto cfz = getzsofslope(sectnum, x, y);
	*ceilz = cfz.ceilz;
	*florz = cfz.floorz;
	*ceilhit = sectnum + 16384;
	*florhit = sectnum + 16384;

	const int dawalclipmask = cliptype & 65535;
	const int dasprclipmask = cliptype >> 16;

	clipsectorlist[0] = sectnum;
	int clipsectcnt{0};
	clipsectnum = 1;

	do  //Collect sectors inside your square first
	{
		auto* sec = &g_sector[clipsectorlist[clipsectcnt]];
		const int startwall = sec->wallptr;
		const int endwall = startwall + sec->wallnum;
		int j{startwall};

		for(auto* wal = &wall[startwall]; j < endwall; ++j, ++wal)
		{
			int k = wal->nextsector;
			
			if (k >= 0)
			{
				auto* wal2 = &wall[wal->point2];
				const int x1 = wal->pt.x;
				const int x2 = wal2->pt.x;

				if ((x1 < xmin) && (x2 < xmin))
					continue;
				if ((x1 > xmax) && (x2 > xmax))
					continue;

				const int y1 = wal->pt.y;
				const int y2 = wal2->pt.y;

				if ((y1 < ymin) && (y2 < ymin))
					continue;
				if ((y1 > ymax) && (y2 > ymax))
					continue;

				const int dx = x2 - x1;
				const int dy = y2 - y1;
				
				if (dx * (y - y1) < (x - x1) * dy)
					continue; //back

				int dax{0};
				if (dx > 0)
					dax = dx * (ymin - y1);
				else
					dax = dx * (ymax - y1);
				
				int day{0};
				if (dy > 0)
					day = dy * (xmax - x1);
				else
					day = dy * (xmin - x1);
				
				if (dax >= day) continue;

				if (wal->cstat&dawalclipmask)
					continue;

				sec = &g_sector[k];
				
				if (!editstatus)
				{
					if (((sec->ceilingstat&1) == 0) && (z <= sec->ceilingz+(3<<8)))
						continue;
					if (((sec->floorstat&1) == 0) && (z >= sec->floorz-(3<<8)))
						continue;
				}

				int cnum = clipsectnum - 1;
				for(; cnum >= 0; --cnum) {
					if (clipsectorlist[cnum] == k)
						break;
				}

				if (cnum < 0)
					clipsectorlist[clipsectnum++] = k;

				if ((x1 < xmin+MAXCLIPDIST) && (x2 < xmin+MAXCLIPDIST))
					continue;

				if ((x1 > xmax-MAXCLIPDIST) && (x2 > xmax-MAXCLIPDIST))
					continue;

				if ((y1 < ymin+MAXCLIPDIST) && (y2 < ymin+MAXCLIPDIST))
					continue;

				if ((y1 > ymax-MAXCLIPDIST) && (y2 > ymax-MAXCLIPDIST))
					continue;

				if (dx > 0)
					dax += dx * MAXCLIPDIST;
				else
					dax -= dx * MAXCLIPDIST;

				if (dy > 0)
					day -= dy * MAXCLIPDIST;
				else
					day += dy * MAXCLIPDIST;
				
				if (dax >= day)
					continue;

					//It actually got here, through all the continue's!!!
				auto dacfz = getzsofslope((short)k,x,y);
				if (dacfz.ceilz > *ceilz) {
					*ceilz = dacfz.ceilz;
					*ceilhit = k + 16384;
				}

				if (dacfz.floorz < *florz) {
					*florz = dacfz.floorz;
					*florhit = k + 16384;
				}
			}
		}

		clipsectcnt++;
	} while (clipsectcnt < clipsectnum);

	for(short cnum{0}; cnum < clipsectnum; ++cnum)
	{
		for(short j = headspritesect[clipsectorlist[cnum]]; j >= 0; j = nextspritesect[j])
		{
			auto* spr = &sprite[j];
			const short cstat = spr->cstat;
			
			if (cstat & dasprclipmask)
			{
				int x1 = spr->x;
				int y1 = spr->y;

				unsigned char clipyou{0};
				int k{0};
				int day{0};
				int dax{0};
				int daz{0};
				int daz2{0};
				int l{0};
				int tilenum{0};
				int xoff{0};
				int yoff{0};
				int x2{0};
				int y2{0};
				int x3{0};
				int y3{0};
				int x4{0};
				int y4{0};
				switch(cstat&48)
				{
					case 0:
						k = walldist+(spr->clipdist<<2)+1;
						if ((std::abs(x1-x) <= k) && (std::abs(y1-y) <= k))
						{
							daz = spr->z;
							k = ((tilesizy[spr->picnum]*spr->yrepeat)<<1);
							if (cstat&128) daz += k;
							if (picanm[spr->picnum]&0x00ff0000) daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
							daz2 = daz - (k<<1);
							clipyou = 1;
						}
						break;
					case 16:
						tilenum = spr->picnum;
						xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						if ((cstat&4) > 0)
								xoff = -xoff;

						k = spr->ang;
						l = spr->xrepeat;
						dax = sintable[k&2047]*l;
						day = sintable[(k+1536)&2047]*l;
						l = tilesizx[tilenum];
						k = (l>>1)+xoff;
						x1 -= mulscalen<16>(dax,k);
						x2 = x1+mulscalen<16>(dax,l);
						y1 -= mulscalen<16>(day,k);
						y2 = y1+mulscalen<16>(day,l);
						if (clipinsideboxline(x,y,x1,y1,x2,y2,walldist+1) != 0)
						{
							daz = spr->z;
							k = ((tilesizy[spr->picnum]*spr->yrepeat)<<1);
							
							if (cstat&128)
									daz += k;
							
							if (picanm[spr->picnum]&0x00ff0000)
								daz -= ((int)((signed char)((picanm[spr->picnum]>>16)&255))*spr->yrepeat<<2);
							daz2 = daz-(k<<1);
							clipyou = 1;
						}
						break;
					case 32:
						daz = spr->z;
						daz2 = daz;

						if ((cstat&64) != 0)
							if ((z > daz) == ((cstat&8)==0))
								continue;

						tilenum = spr->picnum;
						xoff = (int)((signed char)((picanm[tilenum]>>8)&255))+((int)spr->xoffset);
						yoff = (int)((signed char)((picanm[tilenum]>>16)&255))+((int)spr->yoffset);
						
						if ((cstat&4) > 0)
							xoff = -xoff;
						
						if ((cstat&8) > 0)
							yoff = -yoff;

						const int ang = spr->ang;
						const int cosang = sintable[(ang+512)&2047];
						const int sinang = sintable[ang];
						const int xspan = tilesizx[tilenum];
						const int xrepeat = spr->xrepeat;
						const int yspan = tilesizy[tilenum];
						const int yrepeat = spr->yrepeat;

						dax = ((xspan>>1)+xoff)*xrepeat; day = ((yspan>>1)+yoff)*yrepeat;
						x1 += dmulscalen<16>(sinang,dax,cosang,day)-x;
						y1 += dmulscalen<16>(sinang,day,-cosang,dax)-y;
						l = xspan*xrepeat;
						x2 = x1 - mulscalen<16>(sinang,l);
						y2 = y1 + mulscalen<16>(cosang,l);
						l = yspan*yrepeat;
						k = -mulscalen<16>(cosang,l);
						x3 = x2+k;
						x4 = x1+k;
						k = -mulscalen<16>(sinang,l);
						y3 = y2+k;
						y4 = y1+k;

						dax = mulscalen<14>(sintable[(spr->ang-256+512)&2047],walldist+4);
						day = mulscalen<14>(sintable[(spr->ang-256)&2047],walldist+4);
						x1 += dax;
						x2 -= day;
						x3 -= dax;
						x4 += day;
						y1 += day;
						y2 += dax;
						y3 -= day;
						y4 -= dax;

						if ((y1^y2) < 0)
						{
							if ((x1^x2) < 0) clipyou ^= (x1*y2<x2*y1)^(y1<y2);
							else if (x1 >= 0) clipyou ^= 1;
						}
						if ((y2^y3) < 0)
						{
							if ((x2^x3) < 0) clipyou ^= (x2*y3<x3*y2)^(y2<y3);
							else if (x2 >= 0) clipyou ^= 1;
						}
						if ((y3^y4) < 0)
						{
							if ((x3^x4) < 0) clipyou ^= (x3*y4<x4*y3)^(y3<y4);
							else if (x3 >= 0) clipyou ^= 1;
						}
						if ((y4^y1) < 0)
						{
							if ((x4^x1) < 0) clipyou ^= (x4*y1<x1*y4)^(y4<y1);
							else if (x4 >= 0) clipyou ^= 1;
						}
						break;
				}

				if (clipyou != 0)
				{
					if ((z > daz) && (daz > *ceilz)) {
						*ceilz = daz;
						*ceilhit = j + 49152;
					}
					
					if ((z < daz2) && (daz2 < *florz)) {
						*florz = daz2;
						*florhit = j + 49152;
					}
				}
			}
		}
	}
}


//
// setview
//
void setview(int x1, int y1, int x2, int y2)
{
	const float xfov = ((float)xdim / (float)ydim) / (4.F / 3.F);

	windowx1 = x1;
	wx1 = (x1<<12);
	windowy1 = y1;
	wy1 = (y1<<12);
	windowx2 = x2;
	wx2 = ((x2+1)<<12);
	windowy2 = y2;
	wy2 = ((y2+1)<<12);

	xdimen = (x2 - x1) + 1;
	halfxdimen = xdimen >> 1;
	xdimenrecip = divscalen<32>(1L, xdimen);
	ydimen = (y2 - y1) + 1;

	setaspect((int)(65536.F * xfov), pixelaspect);

	std::ranges::subrange startumostr{startumost.begin(), std::next(startumost.begin(), windowx1)};
	std::ranges::subrange startdmostr{startdmost.begin(), std::next(startdmost.begin(), windowx1)};
	std::ranges::fill(startumostr, 1);
	std::ranges::fill(startdmostr, 0);

	std::ranges::subrange winumost{startumostr.end(), std::next(startumostr.end(), windowx2 + 1)};
	std::ranges::subrange windmost{startdmostr.end(), std::next(startdmostr.end(), windowx2 + 1)};
	std::ranges::fill(winumost, windowy1);
	std::ranges::fill(windmost, windowy2 + 1);

	std::ranges::subrange win2uxdim{winumost.end(), std::next(winumost.end(), xdim)};
	std::ranges::subrange win2dxdim{windmost.end(), std::next(windmost.end(), xdim)};
	std::ranges::fill(win2uxdim, 1);
	std::ranges::fill(win2dxdim, 0);

#if USE_POLYMOST && USE_OPENGL
	polymost_setview();
#endif
}


//
// setaspect
//
void setaspect(int daxrange, int daaspect)
{
    const int ys = mulscalen<16>(200, pixelaspect);

    viewingrange = daxrange;
    viewingrangerecip = divscalen<32>(1L,daxrange);

    yxaspect = daaspect;
    xyaspect = divscalen<32>(1, yxaspect);
    xdimenscale = scale(xdimen, yxaspect, 320);
    xdimscale = scale(320, xyaspect, xdimen);

    ydimenscale = scale(ydimen, yxaspect, ys);
}


//
// flushperms
//
void flushperms()
{
	permhead = 0;
	permtail = 0;
}


//
// rotatesprite
//
void rotatesprite(int sx, int sy, int z, short a, short picnum, signed char dashade,
	unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2)
{
	if ((cx1 > cx2) || (cy1 > cy2)) {
		return;
	}

	if (z <= 16) {
		return;
	}

	if (picanm[picnum] & 192) {
		picnum += animateoffs(picnum, (short)0xc000);
	}

	if ((tilesizx[picnum] <= 0) || (tilesizy[picnum] <= 0)) {
		return;
	}

	if (((dastat&128) == 0) || (numpages < 2) || beforedrawrooms) {
		dorotatesprite(sx,sy,z,a,picnum,dashade,dapalnum,dastat,cx1,cy1,cx2,cy2,guniqhudid);
	}

	if ((dastat&64) && (cx1 <= 0) && (cy1 <= 0) && (cx2 >= xdim-1) && (cy2 >= ydim-1) &&
		 (sx == (160<<16)) && (sy == (100<<16)) && (z == 65536L) && (a == 0) && ((dastat&1) == 0))
		permhead = permtail = 0;

	if ((dastat & 128) == 0) {
		return;
	}

	if (numpages >= 2)
	{
		if (((permhead+1)&(MAXPERMS-1)) == permtail)
		{
			int gap{-1};
			for(int i{permtail}; i != permhead; i = ((i + 1) & (MAXPERMS - 1)))
			{
				if ((permfifo[i].pagesleft&127) == 0)
					{ if (gap < 0) gap = i; }
				else if (gap >= 0)
				{
					copybufbyte(&permfifo[i], &permfifo[gap], sizeof(permfifotype));
					permfifo[i].pagesleft = 0;
					for (;gap!=i;gap=((gap+1)&(MAXPERMS-1)))
						if ((permfifo[gap].pagesleft&127) == 0)
							break;
					if (gap==i) gap = -1;
				}
			}
			if (gap >= 0)
				permhead = gap;
			else
				permtail = ((permtail + 1) & (MAXPERMS - 1));
		}

		auto* per = &permfifo[permhead];
		per->sx = sx;
		per->sy = sy;
		per->z = z;
		per->a = a;
		per->picnum = picnum;
		per->dashade = dashade;
		per->dapalnum = dapalnum;
		per->dastat = dastat;
		per->pagesleft = numpages + ((beforedrawrooms ? 1 : 0) << 7);
		per->cx1 = cx1;
		per->cy1 = cy1;
		per->cx2 = cx2;
		per->cy2 = cy2;
		per->uniqid = guniqhudid;	//JF extension

			//Would be better to optimize out true bounding boxes
		if (dastat & 64)  //If non-masking write, checking for overlapping cases
		{
			for(int i{permtail}; i != permhead; i = ((i + 1) & (MAXPERMS - 1)))
			{
				auto* per2 = &permfifo[i];
				if ((per2->pagesleft&127) == 0) continue;
				if (per2->sx != per->sx) continue;
				if (per2->sy != per->sy) continue;
				if (per2->z != per->z) continue;
				if (per2->a != per->a) continue;
				if (tilesizx[per2->picnum] > tilesizx[per->picnum]) continue;
				if (tilesizy[per2->picnum] > tilesizy[per->picnum]) continue;
				if (per2->cx1 < per->cx1) continue;
				if (per2->cy1 < per->cy1) continue;
				if (per2->cx2 > per->cx2) continue;
				if (per2->cy2 > per->cy2) continue;
				per2->pagesleft = 0;
			}

			if (per->a == 0) {
				for(int i{permtail}; i != permhead; i = ((i + 1) & (MAXPERMS - 1)))
				{
					auto* per2 = &permfifo[i];
					if ((per2->pagesleft&127) == 0) continue;
					if (per2->z != per->z) continue;
					if (per2->a != 0) continue;
					if (per2->cx1 < per->cx1) continue;
					if (per2->cy1 < per->cy1) continue;
					if (per2->cx2 > per->cx2) continue;
					if (per2->cy2 > per->cy2) continue;
					if ((per2->sx>>16) < (per->sx>>16)) continue;
					if ((per2->sy>>16) < (per->sy>>16)) continue;
					if (per2->sx+(tilesizx[per2->picnum]*per2->z) > per->sx+(tilesizx[per->picnum]*per->z)) continue;
					if (per2->sy+(tilesizy[per2->picnum]*per2->z) > per->sy+(tilesizy[per->picnum]*per->z)) continue;
					per2->pagesleft = 0;
				}
			}
		}

		permhead = ((permhead+1)&(MAXPERMS-1));
	}
}


//
// makepalookup
//
int makepalookup(int palnum, unsigned char *remapbuf, signed char r, signed char g, signed char b, unsigned char dastat)
{
	// TODO: Are there situations where this wouldn't hold?
	if (palookup[palnum].empty())
	{
			//Allocate palookup buffer
		palookup[palnum].resize(numpalookups << 8);
	}

	if (dastat == 0) {
		return 0;
	}

	if ((r | g | b | 63) != 63) {
		return 0;
	}

	if ((r|g|b) == 0)
	{
		for(int i{0}; i < 256; ++i)
		{
			auto* ptr = (unsigned char *)((intptr_t)palookup[0].data() + remapbuf[i]);
			auto* ptr2 = (unsigned char *)((intptr_t)palookup[palnum].data() + i);

			for(int j{0}; j < numpalookups; ++j) {
				*ptr2 = *ptr;
				ptr += 256;
				ptr2 += 256;
			}
		}
#if USE_POLYMOST && USE_OPENGL
		palookupfog[palnum].r = 0;
		palookupfog[palnum].g = 0;
		palookupfog[palnum].b = 0;
#endif
	}
	else
	{
		auto* ptr2 = palookup[palnum].data();
		for(int i{0}; i < numpalookups; ++i)
		{
			const int palscale = divscalen<16>(i, numpalookups);
			for(int j{0}; j < 256; ++j)
			{
				auto* ptr = &palette[remapbuf[j]*3];
				*ptr2++ = getclosestcol((int)ptr[0]+mulscalen<16>(r-ptr[0],palscale),
							(int)ptr[1]+mulscalen<16>(g-ptr[1],palscale),
							(int)ptr[2]+mulscalen<16>(b-ptr[2],palscale));
			}
		}
#if USE_POLYMOST && USE_OPENGL
		palookupfog[palnum].r = r;
		palookupfog[palnum].g = g;
		palookupfog[palnum].b = b;
#endif
	}

	return 0;
}


// TODO: Make constexpr?
void setvgapalette()
{
	for (int i{0}; i < 256; i++) {
		curpalettefaded[i].b = curpalette[i].b = vgapal16[4 * i] << 2;
		curpalettefaded[i].g = curpalette[i].g = vgapal16[4 * i + 1] << 2;
		curpalettefaded[i].r = curpalette[i].r = vgapal16[4 * i + 2] << 2;
	}
	setpalette(0, 256, &vgapal16[0]);
}

//
// setbrightness
//
void setbrightness(int dabrightness, std::span<const unsigned char> dapal, char noapply)
{
	if ((noapply & 4) == 0) {
		curbrightness = std::min(std::max(dabrightness, 0), 15);
	}

	curgamma = 1.0F + (static_cast<float>(curbrightness) / 10.0F);

	const int j = []() {
		if(setgamma(curgamma) != 0)
			return curbrightness;

		return 0;
    }();

	for(int k{0}, i{0}; i < 256; i++)
	{
		// save palette without any brightness adjustment
		curpalette[i].r = dapal[i * 3 + 0] << 2;
		curpalette[i].g = dapal[i * 3 + 1] << 2;
		curpalette[i].b = dapal[i * 3 + 2] << 2;
		curpalette[i].f = 0;

		// brightness adjust the palette
		curpalettefaded[i].b = (tempbuf[k++] = britable[j][ curpalette[i].b ]);
		curpalettefaded[i].g = (tempbuf[k++] = britable[j][ curpalette[i].g ]);
		curpalettefaded[i].r = (tempbuf[k++] = britable[j][ curpalette[i].r ]);
		curpalettefaded[i].f = tempbuf[k++] = 0;
	}

	if ((noapply & 1) == 0) {
		setpalette(0, 256, (unsigned char*)&tempbuf[0]);
	}

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == rendmode_t::OpenGL) {
		static unsigned int lastpalettesum{0};
		const unsigned int newpalettesum = crc32once((unsigned char *)&curpalettefaded[0], sizeof(curpalettefaded));

		// only reset the textures if the preserve flag (bit 1 of noapply) is clear and
		// either (a) the new palette is different to the last, or (b) the brightness
		// changed and we couldn't set it using hardware gamma
		if (((noapply & 2) == 0) && (newpalettesum != lastpalettesum)) {
			polymost_texinvalidateall();
		}

		lastpalettesum = newpalettesum;
	}
#endif

	palfadergb.r = 0;
	palfadergb.g = 0;
	palfadergb.b = 0;
	palfadedelta = 0;
}


//
// setpalettefade
//
void setpalettefade(unsigned char r, unsigned char g, unsigned char b, unsigned char offset)
{
	palfadergb.r = std::min(static_cast<unsigned char>(63), r) << 2;
	palfadergb.g = std::min(static_cast<unsigned char>(63), g) << 2;
	palfadergb.b = std::min(static_cast<unsigned char>(63), b) << 2;
	palfadedelta = std::min(static_cast<unsigned char>(63), offset) << 2;

	int k{0};

	// FIXME: Check for gammabrightness only once.
	for (int i{0}; i < 256; i++) {
		palette_t p;

		if (gammabrightness) {
			p = curpalette[i];
		}
		else {
			p.b = britable[curbrightness][ curpalette[i].b ];
			p.g	= britable[curbrightness][ curpalette[i].g ];
			p.r = britable[curbrightness][ curpalette[i].r ];
		}

		tempbuf[k++] =
			(curpalettefaded[i].b =
				p.b + ( ( ( (int)palfadergb.b - (int)p.b ) * (int)offset ) >> 6 ) ) >> 2;
		tempbuf[k++] =
			(curpalettefaded[i].g =
				p.g + ( ( ( (int)palfadergb.g - (int)p.g ) * (int)offset ) >> 6 ) ) >> 2;
		tempbuf[k++] =
			(curpalettefaded[i].r =
				p.r + ( ( ( (int)palfadergb.r - (int)p.r ) * (int)offset ) >> 6 ) ) >> 2;
		tempbuf[k++] = curpalettefaded[i].f = 0;
	}

	setpalette(0, 256, (unsigned char*)&tempbuf[0]);
}


//
// clearview
//
void clearview(int dacol)
{
	if (qsetmode != 200) {
		return;
	}

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == rendmode_t::OpenGL) {
		palette_t p;

		if (gammabrightness) {
			p = curpalette[dacol];
		}
		else {
			p.r = britable[curbrightness][ curpalette[dacol].r ];
			p.g = britable[curbrightness][ curpalette[dacol].g ];
			p.b = britable[curbrightness][ curpalette[dacol].b ];
		}

		glfunc.glClearColor(((float)p.r)/255.0,
					  ((float)p.g)/255.0,
					  ((float)p.b)/255.0,
					  0);
		glfunc.glScissor(windowx1,yres-(windowy2+1),windowx2-windowx1+1,windowy2-windowy1+1);
		glfunc.glEnable(GL_SCISSOR_TEST);
		glfunc.glClear(GL_COLOR_BUFFER_BIT);
		glfunc.glDisable(GL_SCISSOR_TEST);
		return;
	}
#endif

	const int dx = windowx2 - windowx1 + 1;
	dacol += (dacol << 8);
	dacol += (dacol << 16);
	intptr_t p = frameplace + ylookup[windowy1] + windowx1;

	for(int y{windowy1}; y <= windowy2; y++) {
		clearbufbyte((void*)p, dx, dacol);
		p += ylookup[1];
	}

	faketimerhandler();
}


//
// clearallviews
//
void clearallviews(int dacol)
{
	if (qsetmode != 200) {
		return;
	}

#if USE_POLYMOST && USE_OPENGL
	if (rendmode == rendmode_t::OpenGL) {
		palette_t p;
		if (gammabrightness)
			p = curpalette[dacol];
		else {
			p.r = britable[curbrightness][ curpalette[dacol].r ];
			p.g = britable[curbrightness][ curpalette[dacol].g ];
			p.b = britable[curbrightness][ curpalette[dacol].b ];
		}

		glfunc.glViewport(0,0,xdim,ydim); glox1 = -1;
		glfunc.glClearColor(((float)p.r)/255.0,
					  ((float)p.g)/255.0,
					  ((float)p.b)/255.0,
					  0);
		glfunc.glClear(GL_COLOR_BUFFER_BIT);
		return;
	}
#endif

	dacol += (dacol<<8); dacol += (dacol<<16);
	clearbuf((void*)frameplace,imageSize>>2,dacol);

	faketimerhandler();
}


//
// plotpixel
//
void plotpixel(int x, int y, unsigned char col)
{
#if USE_POLYMOST && USE_OPENGL
	if (!polymost_plotpixel(x,y,col)) return;
#endif

	drawpixel((void*)(ylookup[y]+x+frameplace),(int)col);
}


//
// getpixel
//
unsigned char getpixel(int x, int y)
{
#if USE_POLYMOST && USE_OPENGL
	if (rendmode == rendmode_t::OpenGL && qsetmode == 200) return 0;
#endif

	return readpixel((void*)(ylookup[y] + x + frameplace));
}


	//MUST USE RESTOREFORDRAWROOMS AFTER DRAWING

//
// setviewtotile
//
void setviewtotile(short tilenume, int xsiz, int ysiz)
{
	//DRAWROOMS TO TILE BACKUP&SET CODE
	tilesizx[tilenume] = xsiz;
	tilesizy[tilenume] = ysiz;
	bakxsiz[setviewcnt] = xsiz;
	bakysiz[setviewcnt] = ysiz;
	bakframeplace[setviewcnt] = frameplace;
	frameplace = waloff[tilenume];
	bakwindowx1[setviewcnt] = windowx1;
	bakwindowy1[setviewcnt] = windowy1;
	bakwindowx2[setviewcnt] = windowx2;
	bakwindowy2[setviewcnt] = windowy2;
#if USE_POLYMOST
	if (setviewcnt == 0) {
		bakrendmode = rendmode;
		baktile = tilenume;
	}

	rendmode = rendmode_t::Classic; //2;
#endif
	copybufbyte(&startumost[windowx1],&bakumost[windowx1],(windowx2-windowx1+1)*sizeof(bakumost[0]));
	copybufbyte(&startdmost[windowx1],&bakdmost[windowx1],(windowx2-windowx1+1)*sizeof(bakdmost[0]));
	setviewcnt++;

	offscreenrendering = true;
	setview(0,0,ysiz-1,xsiz-1);
	setaspect(65536,65536);

	std::generate_n(ylookup.begin(), xsiz + 1,
		[n = 0, ysiz]() mutable {
			return ysiz * (n++);
		});

	setvlinebpl(ysiz);
}


//
// setviewback
//
void setviewback()
{
	if (setviewcnt <= 0) {
		return;
	}

	--setviewcnt;

	// FIXME: Change to bool
	offscreenrendering = (setviewcnt > 0);
#if USE_POLYMOST
	if (setviewcnt == 0) {
		rendmode = bakrendmode;
#if USE_OPENGL
		invalidatetile(baktile, -1, -1);
#endif
	}
#endif

	setview(bakwindowx1[setviewcnt],bakwindowy1[setviewcnt],
			  bakwindowx2[setviewcnt],bakwindowy2[setviewcnt]);
	copybufbyte(&bakumost[windowx1],&startumost[windowx1],(windowx2-windowx1+1)*sizeof(startumost[0]));
	copybufbyte(&bakdmost[windowx1],&startdmost[windowx1],(windowx2-windowx1+1)*sizeof(startdmost[0]));

	frameplace = bakframeplace[setviewcnt];

	int k{0};
	if (setviewcnt == 0)
		k = bakxsiz[0];
	else
		k = std::max(bakxsiz[setviewcnt - 1], bakxsiz[setviewcnt]);

	std::generate_n(ylookup.begin(), k + 1, [n = 0]() mutable { return bytesperline * (n++); });

	setvlinebpl(bytesperline);
	modechange = true;
}


//
// squarerotatetile
//
void squarerotatetile(short tilenume)
{
	const int xsiz = tilesizx[tilenume];
	const int ysiz = tilesizy[tilenume];

		//supports square tiles only for rotation part
	if (xsiz == ysiz)
	{
		const int k = (xsiz << 1);
		
		for(int i = xsiz - 1; i >= 0; --i)
		{
			auto* ptr1 = (unsigned char *)(waloff[tilenume] + i * (xsiz + 1));
			auto* ptr2 = ptr1;

			if ((i & 1) != 0) {
				ptr1--;
				ptr2 -= xsiz;
				swapchar(ptr1, ptr2);
			}

			for(int j = (i >> 1) - 1; j >= 0; --j) {
				ptr1 -= 2;
				ptr2 -= k;
				swapchar2(ptr1,ptr2,xsiz);
			}
		}
	}
}


//
// preparemirror
//
void preparemirror(int dax, int day, int daz, short daang, int dahoriz, short dawall, short dasector, int *tposx, int *tposy, short *tang)
{
	std::ignore = daz;
	std::ignore = dahoriz;
	std::ignore = dasector;

	const int x = wall[dawall].pt.x;
	const int dx = wall[wall[dawall].point2].pt.x - x;
	const int y = wall[dawall].pt.y;
	const int dy = wall[wall[dawall].point2].pt.y - y;
	const int j = dx * dx + dy * dy;
	
	if (j == 0) {
		return;
	}
	
	const int i = (((dax - x) * dx + (day - y) * dy) << 1);
	*tposx = (x << 1) + scale(dx, i, j) - dax;
	*tposy = (y << 1) + scale(dy, i, j) - day;
	*tang = (((getangle(dx,dy)<<1)-daang)&2047);

	inpreparemirror = true;
}


//
// completemirror
//
void completemirror()
{
#if USE_POLYMOST
	if (rendmode != rendmode_t::Classic) {
		return;
	}
#endif

		//Can't reverse with uninitialized data
	if (inpreparemirror) {
		inpreparemirror = false;
		return;
	}

	if (mirrorsx1 > 0) {
		mirrorsx1--;
	}

	if (mirrorsx2 < windowx2 - windowx1 - 1) {
		mirrorsx2++;
	}

	if (mirrorsx2 < mirrorsx1) {
		return;
	}

	intptr_t p = frameplace + ylookup[windowy1 + mirrorsy1] + windowx1 + mirrorsx1;
	const int i = windowx2 - windowx1 - mirrorsx2 - mirrorsx1;
	mirrorsx2 -= mirrorsx1;

	for(int dy = mirrorsy2 - mirrorsy1 - 1; dy >= 0; dy--)
	{
		copybufbyte((void*)(p + 1), &tempbuf[0], mirrorsx2 + 1);
		tempbuf[mirrorsx2] = tempbuf[mirrorsx2 - 1];
		copybufreverse(&tempbuf[mirrorsx2], (void*)(p + i), mirrorsx2 + 1);
		p += ylookup[1];
		faketimerhandler();
	}
}


//
// sectorofwall
//
int sectorofwall(short theline)
{
	if ((theline < 0) || (theline >= numwalls))
		return -1;

	int i = wall[theline].nextwall;
	
	if (i >= 0)
		return wall[i].nextsector;

	int gap = (numsectors >> 1);
	i = gap;

	while (gap > 1)
	{
		gap >>= 1;

		if (g_sector[i].wallptr < theline)
			i += gap;
		else
			i -= gap;
	}

	while (g_sector[i].wallptr > theline)
		i--;

	while (g_sector[i].wallptr+g_sector[i].wallnum <= theline)
		i++;
	
	return i;
}


//
// getceilzofslope
//
int getceilzofslope(short sectnum, int dax, int day)
{
	if (!(g_sector[sectnum].ceilingstat & 2))
		return g_sector[sectnum].ceilingz;

	const walltype* wal = &wall[g_sector[sectnum].wallptr];
	const int dx = wall[wal->point2].pt.x - wal->pt.x;
	const int dy = wall[wal->point2].pt.y - wal->pt.y;
	const int i = static_cast<int>(std::hypot(dx, dy)) << 5;
	
	if (i == 0)
		return g_sector[sectnum].ceilingz;
	
	const int j = dmulscalen<3>(dx, day - wal->pt.y, -dy, dax - wal->pt.x);

	return g_sector[sectnum].ceilingz + scale(g_sector[sectnum].ceilingheinum, j, i);
}


//
// getflorzofslope
//
int getflorzofslope(short sectnum, int dax, int day)
{
	if (!(g_sector[sectnum].floorstat & 2))
		return g_sector[sectnum].floorz;

	const walltype* wal = &wall[g_sector[sectnum].wallptr];
	const int dx = wall[wal->point2].pt.x - wal->pt.x;
	const int dy = wall[wal->point2].pt.y - wal->pt.y;
	const int i = static_cast<int>(std::hypot(dx, dy)) << 5;
	
	if (i == 0)
		return g_sector[sectnum].floorz;

	const int j = dmulscalen<3>(dx, day - wal->pt.y, -dy, dax - wal->pt.x);

	return g_sector[sectnum].floorz + scale(g_sector[sectnum].floorheinum, j, i);
}


//
// getzsofslope
//
ceilfloorz getzsofslope(short sectnum, int dax, int day)
{
	sectortype* sec = &g_sector[sectnum];

	ceilfloorz cfz{sec->ceilingz, sec->floorz};
	
	if ((sec->ceilingstat|sec->floorstat)&2)
	{
		const walltype* wal = &wall[sec->wallptr];
		const walltype* wal2 = &wall[wal->point2];
		const int dx = wal2->pt.x - wal->pt.x;
		const int dy = wal2->pt.y - wal->pt.y;
		const int i = static_cast<int>(std::hypot(dx, dy)) << 5;
		
		if (i == 0)
			return cfz;
		
		const int j = dmulscalen<3>(dx, day - wal->pt.y, -dy, dax - wal->pt.x);

		if (sec->ceilingstat&2)
			cfz.ceilz = cfz.ceilz + scale(sec->ceilingheinum, j, i);

		if (sec->floorstat&2)
			cfz.floorz = cfz.floorz + scale(sec->floorheinum, j, i);
	}

	return cfz;
}


//
// alignceilslope
//
void alignceilslope(short dasect, int x, int y, int z)
{
	const walltype* wal = &wall[g_sector[dasect].wallptr];
	const int dax = wall[wal->point2].pt.x-wal->pt.x;
	const int day = wall[wal->point2].pt.y-wal->pt.y;

	const int i = (y-wal->pt.y) * dax - (x-wal->pt.x) * day;
	
	if (i == 0) {
		return;
	}

	g_sector[dasect].ceilingheinum = scale((z - g_sector[dasect].ceilingz) << 8,
	  static_cast<int>(std::hypot(dax, day)), i);

	if (g_sector[dasect].ceilingheinum == 0) {
		g_sector[dasect].ceilingstat &= ~2;
	}
	else {
		g_sector[dasect].ceilingstat |= 2;
	}
}


//
// alignflorslope
//
void alignflorslope(short dasect, int x, int y, int z)
{
	const walltype* wal = &wall[g_sector[dasect].wallptr];
	const int dax = wall[wal->point2].pt.x-wal->pt.x;
	const int day = wall[wal->point2].pt.y-wal->pt.y;

	const int i = (y - wal->pt.y) * dax - (x - wal->pt.x) * day;

	if (i == 0) {
		return;
	}

	g_sector[dasect].floorheinum = scale((z - g_sector[dasect].floorz) << 8,
	  static_cast<int>(std::hypot(dax, day)), i);

	if (g_sector[dasect].floorheinum == 0) {
		g_sector[dasect].floorstat &= ~2;
	}
	else {
		g_sector[dasect].floorstat |= 2;
	}
}


//
// loopnumofsector
//
int loopnumofsector(short sectnum, short wallnum)
{
	int numloops{0};
	const int startwall = g_sector[sectnum].wallptr;
	const int endwall = startwall + g_sector[sectnum].wallnum;

	for(int i{startwall}; i < endwall; i++)
	{
		if (i == wallnum) {
			return numloops;
		}

		if (wall[i].point2 < i) {
			numloops++;
		}
	}

	return -1;
}


//
// setfirstwall
//
void setfirstwall(short sectnum, short newfirstwall)
{
	const int startwall = g_sector[sectnum].wallptr;
	const int danumwalls = g_sector[sectnum].wallnum;
	const int endwall = startwall + danumwalls;
	
	if ((newfirstwall < startwall) || (newfirstwall >= startwall+danumwalls))
		return;
	
	for(int i{0}; i < danumwalls; ++i) {
		std::memcpy(&wall[i+numwalls], &wall[i + startwall], sizeof(walltype));
	}

	int numwallsofloop{0};

	int infw = newfirstwall;
	
	do
	{
		numwallsofloop++;
		infw = wall[infw].point2;
	} while (infw != newfirstwall);

		//Put correct loop at beginning
	const int dagoalloop = loopnumofsector(sectnum, newfirstwall);
	if (dagoalloop > 0)
	{
		int j{0};

		while (loopnumofsector(sectnum, j + startwall) != dagoalloop) {
			++j;
		}
		
		for(int i{0}; i < danumwalls; ++i)
		{
			int k = i + j;
			if (k >= danumwalls)
				k -= danumwalls;
			std::memcpy(&wall[startwall+i],&wall[numwalls+k],sizeof(walltype));

			wall[startwall+i].point2 += danumwalls-startwall-j;
			if (wall[startwall+i].point2 >= danumwalls)
				wall[startwall+i].point2 -= danumwalls;
			wall[startwall+i].point2 += startwall;
		}

		newfirstwall += danumwalls - j;
		if (newfirstwall >= startwall+danumwalls)
			newfirstwall -= danumwalls;
	}

	std::copy_n(std::next(wall.begin(), startwall), numwallsofloop, std::next(wall.begin(), numwalls));

	for(int i{0}; i < numwallsofloop; ++i)
	{
		int k = i + newfirstwall - startwall;

		if (k >= numwallsofloop)
			k -= numwallsofloop;

		std::memcpy(&wall[startwall+i],&wall[numwalls+k],sizeof(walltype));

		wall[startwall+i].point2 += numwallsofloop-newfirstwall;
		
		if (wall[startwall+i].point2 >= numwallsofloop)
			wall[startwall+i].point2 -= numwallsofloop;
		
		wall[startwall+i].point2 += startwall;
	}

	for(int i{startwall}; i < endwall; ++i) {
		if (wall[i].nextwall >= 0)
			wall[wall[i].nextwall].nextwall = i;
	}
}


//
// drawline256
//
void drawline256(int x1, int y1, int x2, int y2, unsigned char col)
{
	col = palookup[0][col];

	const int dx = x2 - x1;
	const int dy = y2 - y1;

	if (dx >= 0) {
		if ((x1 >= wx2) || (x2 < wx1))
			return;

		if (x1 < wx1) {
			y1 += scale(wx1-x1,dy,dx);
			x1 = wx1;
		}

		if (x2 > wx2) {
			y2 += scale(wx2-x2,dy,dx);
			x2 = wx2;
		}
	}
	else
	{
		if ((x2 >= wx2) || (x1 < wx1))
			return;
		
		if (x2 < wx1) {
			y2 += scale(wx1-x2,dy,dx);
			x2 = wx1;
		}

		if (x1 > wx2) {
			y1 += scale(wx2-x1,dy,dx);
			x1 = wx2;
		}
	}

	if (dy >= 0)
	{
		if ((y1 >= wy2) || (y2 < wy1))
			return;

		if (y1 < wy1) {
			x1 += scale(wy1-y1,dx,dy);
			y1 = wy1;
		}

		if (y2 > wy2) {
			x2 += scale(wy2-y2,dx,dy);
			y2 = wy2;
		}
	}
	else
	{
		if ((y2 >= wy2) || (y1 < wy1)) return;
		if (y2 < wy1) x2 += scale(wy1-y2,dx,dy), y2 = wy1;
		if (y1 > wy2) x1 += scale(wy2-y1,dx,dy), y1 = wy2;
	}

#if USE_POLYMOST && USE_OPENGL
	if (!polymost_drawline256(x1,y1,x2,y2,col)) return;
#endif

	if (std::abs(dx) >= std::abs(dy))
	{
		if (dx == 0)
			return;

		if (dx < 0)
		{
			std::swap(x1, x2);
			std::swap(y1, y2);
			x1 += 4096;
			x2 += 4096;
		}

		const int inc = divscalen<12>(dy, dx);
		int plc = y1 + mulscalen<12>((2047 - x1) & 4095, inc);
		const int daend = ((x2 + 2048) >> 12);

		for(int i = ((x1 + 2048) >> 12); i < daend; ++i)
		{
			const int j = (plc >> 12);
			if ((j >= startumost[i]) && (j < startdmost[i]))
				drawpixel((void*)(frameplace + ylookup[j] + i), col);
			plc += inc;
		}
	}
	else
	{
		if (dy < 0)
		{
			std::swap(x1, x2);
			std::swap(y1, y2);
			y1 += 4096;
			y2 += 4096;
		}

		const int inc = divscalen<12>(dx, dy);
		int plc = x1 + mulscalen<12>((2047 - y1) & 4095, inc);
		int i = ((y1 + 2048) >> 12);
		const int daend = ((y2 + 2048) >> 12);

		intptr_t p = ylookup[i] + frameplace;
		for(; i < daend; ++i)
		{
			const int j = (plc >> 12);
			if ((i >= startumost[j]) && (i < startdmost[j]))
				drawpixel((void*)(j + p),col);
			plc += inc;
			p += ylookup[1];
		}
	}
}


//
// printext256
//
void printext256(int xpos, int ypos, short col, short backcol, std::string_view name, char fontsize)
{
#if USE_POLYMOST && USE_OPENGL
	if (!polymost_printext256(xpos, ypos, col, backcol, name, fontsize))
		return;
#endif

	const auto* f = &textfonts[std::min(static_cast<int>(fontsize), 2)]; // FIXME: Don't use char for indexing.
	int stx = xpos;

	for(int i{0}; name[i]; ++i) {
		const unsigned char* letptr = &f->font[((int)(unsigned char)name[i])*f->cellh + f->cellyoff];
		auto* ptr = (unsigned char *)(ylookup[ypos+f->charysiz-1]+stx+frameplace);
		
		for(int y = f->charysiz - 1; y >= 0; --y) {
			for(int x = f->charxsiz - 1; x >= 0; --x) {
				if (letptr[y] & pow2char[7 - x - f->cellxoff])
					ptr[x] = (unsigned char)col;
				else if (backcol >= 0)
					ptr[x] = (unsigned char)backcol;
			}

			ptr -= bytesperline;
		}

		stx += f->charxsiz;
	}
}

#if USE_POLYMOST

//
// setrendermode
//
void setrendermode(rendmode_t renderer)
{
	if (bpp == 8) {
		// TODO: Add greater quality modes.
		if (renderer == rendmode_t::OpenGL)
			renderer = rendmode_t::Software;
	} else {
		renderer = rendmode_t::OpenGL;
	}

	rendmode = renderer;
}

//
// getrendermode
//
rendmode_t getrendermode()
{
	return rendmode;
}


//
// setrollangle
//
void setrollangle(int rolla)
{
	if (rolla == 0)
		gtang = 0.0;
	else
		gtang = std::numbers::pi_v<double> * (double)rolla / 1024.0;
}

#endif //USE_POLYMOST

#if USE_POLYMOST && USE_OPENGL

//
// invalidatetile
//  pal: pass -1 to invalidate all palettes for the tile, or >=0 for a particular palette
//  how: pass -1 to invalidate all instances of the tile in texture memory, or a bitfield
//         bit 0: opaque or masked (non-translucent) texture, using repeating
//         bit 1: ignored
//         bit 2: ignored (33% translucence, using repeating)
//         bit 3: ignored (67% translucence, using repeating)
//         bit 4: opaque or masked (non-translucent) texture, using clamping
//         bit 5: ignored
//         bit 6: ignored (33% translucence, using clamping)
//         bit 7: ignored (67% translucence, using clamping)
//       clamping is for sprites, repeating is for walls
//
void invalidatetile(short tilenume, int pal, int how)
{
	if (rendmode != rendmode_t::OpenGL)
		return;

	const auto [numpal, firstpal] = [pal]() -> std::pair<int, int> {
		if(pal < 0) {
			return {MAXPALOOKUPS, 0};
		}
		else {
			return {1, pal % MAXPALOOKUPS};
		}
	}();

	for (int hp{0}; hp < 8; hp += 4) {
		if (!(how & pow2long[hp]))
			continue;

		for (int np{firstpal}; np < firstpal + numpal; ++np) {
			polymost_texinvalidate(tilenume, np, hp);
		}
	}
}


//
// setpolymost2dview
//  Sets OpenGL for 2D drawing
//
void setpolymost2dview()
{
	if (rendmode != rendmode_t::OpenGL)
		return;

	if (gloy1 != -1) {
		glfunc.glViewport(0, 0, xres, yres);
	}

	gloy1 = -1;

	glfunc.glDisable(GL_DEPTH_TEST);
	glfunc.glDisable(GL_BLEND);
}

#endif //USE_POLYMOST && USE_OPENGL

void buildputs(std::string_view str)
{
	fmt::print(stdout, "{}", str);

    if (logfile)
		fmt::print(logfile, "{}", str);
    
	initputs(str);  // the startup window
    OSD_Puts(str);  // the onscreen-display
}

void buildsetlogfile(const std::string& fn)
{
	if (logfile)
		std::fclose(logfile);

	logfile = nullptr;

	if (!fn.empty())
		logfile = std::fopen(fn.c_str(), "w");

	if (logfile)
		setvbuf(logfile, (char*)nullptr, _IONBF, 0);
}


/*
 * vim:ts=8:
 */

