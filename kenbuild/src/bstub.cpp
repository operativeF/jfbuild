// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "compat.hpp"
#include "build.hpp"
#include "editor.hpp"
#include "pragmas.hpp"
#include "baselayer.hpp"
#include "names.hpp"
#include "osd.hpp"
#include "cache1d.hpp"
#include "engine_priv.hpp"

#include <fmt/core.h>

#include <array>
#include <string>

static std::array<unsigned char, 256> tempbuf;

std::array<int, NUMBUILDKEYS> keys =
{
	0xc8,0xd0,0xcb,0xcd,0x2a,0x9d,0x1d,0x39,
	0x1e,0x2c,0xd1,0xc9,0x33,0x34,
	0x9c,0x1c,0xd,0xc,0xf,0x45
};



//static int hang = 0;
//static int rollangle = 0;

//Detecting 2D / 3D mode:
//   qsetmode is 200 in 3D mode
//   qsetmode is 350/480 in 2D mode
//
//You can read these variables when F5-F8 is pressed in 3D mode only:
//
//   If (searchstat == 0)  WALL        searchsector=sector, searchwall=wall
//   If (searchstat == 1)  CEILING     searchsector=sector
//   If (searchstat == 2)  FLOOR       searchsector=sector
//   If (searchstat == 3)  SPRITE      searchsector=sector, searchwall=sprite
//   If (searchstat == 4)  MASKED WALL searchsector=sector, searchwall=wall
//
//   searchsector is the sector of the selected item for all 5 searchstat's
//
//   searchwall is undefined if searchstat is 1 or 2
//   searchwall is the wall if searchstat = 0 or 4
//   searchwall is the sprite if searchstat = 3 (Yeah, I know - it says wall,
//                                      but trust me, it's the sprite number)

int averagefps;
constexpr auto AVERAGEFRAMES{32};
static std::array<unsigned int, AVERAGEFRAMES> frameval;
static int framecnt{0};

int nextvoxid = 0;

int ExtInit()
{
	int i, rv = 0;

	/*printf("------------------------------------------------------------------------------\n");
	std::printf("   BUILD.EXE copyright(c) 1996 by Ken Silverman.  You are granted the\n");
	std::printf("   right to use this software for your personal use only.  This is a\n");
	std::printf("   special version to be used with \"Happy Fun KenBuild\" and may not work\n");
	std::printf("   properly with other Build engine games.  Please refer to license.doc\n");
	std::printf("   for distribution rights\n");
	std::printf("------------------------------------------------------------------------------\n");
	getch();
	*/

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
			// FIXME: Defaults to '/'; use '\ on windows.
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
        std::string supportdir = Bgetsupportdir(0);
        char dirpath[BMAX_PATH+1];
        int asperr;

        if (!supportdir.empty()) {
#if defined(_WIN32) || defined(__APPLE__)
            constexpr std::string_view dirname = "KenBuild";
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

	initgroupfile("stuff.dat");
	bpp = 8;
	if (loadsetup("build.cfg") < 0) buildputs("Configuration file not found, using defaults.\n"), rv = 1;
	std::memcpy((void *)&buildkeys[0],(void *)&keys[0],sizeof(buildkeys));   //Trick to make build use setup.dat keys
	if (option[4] > 0) option[4] = 0;
	if (!initengine()) {
		wm_msgbox("Build Engine Initialisation Error",
				"There was a problem initialising the Build engine: %s", engineerrstr);
		return -1;
	}

		//You can load your own palette lookup tables here if you just
		//copy the right code!
	for(i=0;i<256;i++)
		tempbuf[i] = ((i+32)&255);  //remap colors for screwy palette sectors
	makepalookup(16, &tempbuf[0], 0, 0, 0, 1);

	kensplayerheight = 32;
	zmode = 0;
	defaultspritecstat = 0;
	pskyoff[0] = 0; pskyoff[1] = 0; pskybits = 1;

	tiletovox[PLAYER] = nextvoxid++;
	tiletovox[BROWNMONSTER] = nextvoxid++;

#ifdef _WIN32
//	allowtaskswitching(0);
#endif
	return rv;
}

void ExtUnInit()
{
	uninitgroupfile();
	writesetup("build.cfg");
}

//static int daviewingrange, daaspect, horizval1, horizval2;
void ExtPreCheckKeys()
{
	int /*cosang, sinang, dx, dy, mindx,*/ j, k;

	if (keystatus[0x3e])  //F4 - screen re-size
	{
		keystatus[0x3e] = 0;

			//cycle through all vesa modes, then screen-buffer mode
		if (keystatus[0x2a]|keystatus[0x36]) {
			setgamemode(!fullscreen, xdim, ydim, bpp);
		} else {

			//cycle through all modes
			j=-1;

			// work out a mask to select the mode
			for(int i{0}; const auto& vmode : validmode) {
				if ((validmode[i].xdim == xdim) &&
					(validmode[i].ydim == ydim) &&
					(validmode[i].fs == fullscreen) &&
					(validmode[i].bpp == bpp)) {
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

			setgamemode(fullscreen, validmode[j].xdim, validmode[j].ydim, bpp);
		}
	}

#if 0
	if (keystatus[0x2a]|keystatus[0x36])
	{
		if (keystatus[0xcf]) hang = std::max(hang - 1,-182);
		if (keystatus[0xc7]) hang = std::min(hang + 1, 182);
	}
	else
	{
		if (keystatus[0xcf]) hang = std::max(hang - 8,-182);
		if (keystatus[0xc7]) hang = std::min(hang + 8, 182);
	}
	if (keystatus[0x4c]) { hang = 0; horiz = 100; }
	if (hang != 0)
	{
		walock[4094] = 255;

		// JBF 20031117: scale each dimension by a factor of 1.2, and work out
		// the aspect of the screen. Anywhere you see 'i' below was the value
		// '200' before I changed it. NOTE: This whole trick crashes in resolutions
		// above 800x600. I'm not sure why, and fixing it is not something I intend
		// to do in a real big hurry.
		dx = (xdim + (xdim >> 3) + (xdim >> 4) + (xdim >> 6)) & (~7);
		dy = (ydim + (ydim >> 3) + (ydim >> 4) + (ydim >> 6)) & (~7);
		i = scale(320,ydim,xdim);
		
		if (waloff[4094] == 0) allocache(&waloff[4094],/*240L*384L*/dx*dy,&walock[4094]);
		setviewtotile(4094,/*240L,384L*/dy,dx);

		cosang = sintable[(hang+512)&2047];
		sinang = sintable[hang&2047];

		dx = dmulscalen<1>(320,cosang,i,sinang); mindx = dx;
		dy = dmulscalen<1>(-i,cosang,320,sinang);
		horizval1 = dy*(320>>1)/dx-1;

		dx = dmulscalen<1>(320,cosang,-i,sinang);
		mindx = std::min(dx, mindx);
		dy = dmulscalen<1>(i,cosang,320,sinang);
		horizval2 = dy*(320>>1)/dx+1;

		daviewingrange = scale(65536,16384*(xdim>>1),mindx-16);
		daaspect = scale(daviewingrange,scale(320,tilesizx[4094],tilesizy[4094]),horizval2+6-horizval1);
		setaspect(daviewingrange,scale(daaspect,ydim*320,xdim*i));
		horiz = 100-divscalen<15>(horizval1+horizval2,daviewingrange);
	}
#endif
}

void ExtAnalyzeSprites()
{
	int i, *longptr;
	spritetype *tspr;

	for(i=0,tspr=&tsprite[0];i<spritesortcnt;i++,tspr++)
	{
		if (usevoxels && tiletovox[tspr->picnum] >= 0)
		{
		switch(tspr->picnum)
		{
			case PLAYER:
				if (!voxoff[ tiletovox[PLAYER] ][0]) {
					if (qloadkvx(tiletovox[PLAYER], "voxel000.kvx")) {
						tiletovox[PLAYER] = -1;
						break;
					}
				}
				//tspr->cstat |= 48; tspr->picnum = tiletovox[tspr->picnum];
				longptr = (int *)voxoff[ tiletovox[PLAYER] ][0];
				tspr->xrepeat = scale(tspr->xrepeat,56,longptr[2]);
				tspr->yrepeat = scale(tspr->yrepeat,56,longptr[2]);
				tspr->shade -= 6;
				break;
			case BROWNMONSTER:
				if (!voxoff[ tiletovox[BROWNMONSTER] ][0]) {
					if (qloadkvx(tiletovox[BROWNMONSTER], "voxel001.kvx")) {
						tiletovox[BROWNMONSTER] = -1;
						break;
					}
				}
				//tspr->cstat |= 48; tspr->picnum = tiletovox[tspr->picnum];
				break;
		}
		}

		tspr->shade += 6;
		if (g_sector[tspr->sectnum].ceilingstat&1)
			tspr->shade += g_sector[tspr->sectnum].ceilingshade;
		else
			tspr->shade += g_sector[tspr->sectnum].floorshade;
	}
}

void ExtCheckKeys()
{
	int i;//, p, y, dx, dy, cosang, sinang, bufplc, tsizy, tsizyup15;
	int j;

	if (qsetmode == 200)    //In 3D mode
	{
#if 0
		if (hang != 0)
		{
			bufplc = waloff[4094]+(mulscalen<16>(horiz-100,xdimenscale)+(tilesizx[4094]>>1))*tilesizy[4094];
			setviewback();
			cosang = sintable[(hang+512)&2047];
			sinang = sintable[hang&2047];
			dx = dmulscalen<1>(xdim,cosang,ydim,sinang);
			dy = dmulscalen<1>(-ydim,cosang,xdim,sinang);

			tsizy = tilesizy[4094];
			tsizyup15 = (tsizy<<15);
			dx = mulscalen<14>(dx,daviewingrange);
			dy = mulscalen<14>(dy,daaspect);
			sinang = mulscalen<14>(sinang,daviewingrange);
			cosang = mulscalen<14>(cosang,daaspect);
			p = ylookup[windowy1]+frameplace+windowx2+1;
			for(y=windowy1;y<=windowy2;y++)
			{
				i = divscalen<16>(tsizyup15,dx);
				stretchhline(0,(xdim>>1)*i+tsizyup15,xdim>>2,i,mulscalen<32>(i,dy)*tsizy+bufplc,p);
				dx -= sinang; dy += cosang; p += ylookup[1];
			}
			walock[4094] = 1;

			std::sprintf(tempbuf,"%d",(hang*180)>>10);
			printext256(0L,8L,31,-1,tempbuf,1);
		}
#endif
		if (keystatus[0xa]) setaspect(viewingrange+(viewingrange>>8),yxaspect+(yxaspect>>8));
		if (keystatus[0xb]) setaspect(viewingrange-(viewingrange>>8),yxaspect-(yxaspect>>8));
		if (keystatus[0xc]) setaspect(viewingrange,yxaspect-(yxaspect>>8));
		if (keystatus[0xd]) setaspect(viewingrange,yxaspect+(yxaspect>>8));
		//if (keystatus[0x38]) setrollangle(rollangle+=((keystatus[0x2a]|keystatus[0x36])*6+2));
		//if (keystatus[0xb8]) setrollangle(rollangle-=((keystatus[0x2a]|keystatus[0x36])*6+2));
		//if (keystatus[0x1d]|keystatus[0x9d]) setrollangle(rollangle=0);

		i = frameval[framecnt&(AVERAGEFRAMES-1)];
		j = frameval[framecnt&(AVERAGEFRAMES-1)] = getticks(); framecnt++;
		if (i != j) averagefps = ((mul3(averagefps)+((AVERAGEFRAMES*1000)/(j-i)) )>>2);
		fmt::format_to(&tempbuf[0], "{}", averagefps);
		printext256(0L,0L,31,-1, (char *)&tempbuf[0], 1);

		editinput();
	}
	else
	{
	}
}

void ExtCleanUp()
{
}

void ExtPreLoadMap()
{
}

void ExtLoadMap(const std::string& mapname)
{
	wm_setwindowtitle(mapname);
}

void ExtPreSaveMap()
{
}

void ExtSaveMap(const char *mapname)
{
	wm_setwindowtitle(mapname);
}

const char *ExtGetSectorCaption(short sectnum)
{
	if ((g_sector[sectnum].lotag|g_sector[sectnum].hitag) == 0)
	{
		tempbuf[0] = 0;
	}
	else
	{
		std::sprintf((char *)&tempbuf[0],"%hu,%hu",(unsigned short)g_sector[sectnum].hitag,
								  (unsigned short)g_sector[sectnum].lotag);
	}
	return((char *)&tempbuf[0]);
}

const char *ExtGetWallCaption(short wallnum)
{
	if ((wall[wallnum].lotag|wall[wallnum].hitag) == 0)
	{
		tempbuf[0] = 0;
	}
	else
	{
		std::sprintf((char *)&tempbuf[0],"%hu,%hu",(unsigned short)wall[wallnum].hitag,
								  (unsigned short)wall[wallnum].lotag);
	}
	return((char *)&tempbuf[0]);
}

const char *ExtGetSpriteCaption(short spritenum)
{
	if ((sprite[spritenum].lotag|sprite[spritenum].hitag) == 0)
	{
		tempbuf[0] = 0;
	}
	else
	{
		std::sprintf((char *)&tempbuf[0],"%hu,%hu",(unsigned short)sprite[spritenum].hitag,
								  (unsigned short)sprite[spritenum].lotag);
	}
	return((char *)&tempbuf[0]);
}

//printext16 parameters:
//printext16(int xpos, int ypos, short col, short backcol,
//           char name[82], char fontsize)
//  xpos 0-639   (top left)
//  ypos 0-479   (top left)
//  col 0-15
//  backcol 0-15, -1 is transparent background
//  name
//  fontsize 0=8*8, 1=3*5

//drawline16 parameters:
// drawline16(int x1, int y1, int x2, int y2, char col)
//  x1, x2  0-639
//  y1, y2  0-143  (status bar is 144 high, origin is top-left of STATUS BAR)
//  col     0-15

void ExtShowSectorData(short sectnum)   //F5
{
	if (qsetmode == 200)    //In 3D mode
	{
	}
	else
	{
		clearmidstatbar16();             //Clear middle of status bar

		std::sprintf((char *)&tempbuf[0],"Sector %d",sectnum);
		printext16(8,32,11,-1, (char *)&tempbuf[0], 0);

		printext16(8,48,11,-1,"8*8 font:  ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789",0);
		printext16(8,56,11,-1,"3*5 font:  ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789",1);
		printext16(8,62,11,-1,"8*14 font: ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789",2);

		drawline16(320,68,344,80,4);       //Draw house
		drawline16(344,80,344,116,4);
		drawline16(344,116,296,116,4);
		drawline16(296,116,296,80,4);
		drawline16(296,80,320,68,4);
	}
}

void ExtShowWallData(short wallnum)       //F6
{
	if (qsetmode == 200)    //In 3D mode
	{
	}
	else
	{
		clearmidstatbar16();             //Clear middle of status bar

		std::sprintf((char *)&tempbuf[0],"Wall %d",wallnum);
		printext16(8,32,11,-1,(char *)&tempbuf[0], 0);
	}
}

void ExtShowSpriteData(short spritenum)   //F6
{
	if (qsetmode == 200)    //In 3D mode
	{
	}
	else
	{
		clearmidstatbar16();             //Clear middle of status bar

		std::sprintf((char *)&tempbuf[0],"Sprite %d",spritenum);
		printext16(8,32,11,-1, (char *)&tempbuf[0], 0);
	}
}

void ExtEditSectorData(short sectnum)    //F7
{
	short nickdata;

	if (qsetmode == 200)    //In 3D mode
	{
			//Ceiling
		if (searchstat == 1)
			g_sector[searchsector].ceilingpicnum++;   //Just a stupid example

			//Floor
		if (searchstat == 2)
			g_sector[searchsector].floorshade++;      //Just a stupid example
	}
	else                    //In 2D mode
	{
		std::sprintf((char *)&tempbuf[0],"Sector (%d) Nick's variable: ",sectnum);
		nickdata = 0;
		nickdata = getnumber16((char *)&tempbuf[0], nickdata, 65536L, 0);

		printmessage16("");              //Clear message box (top right of status bar)
		ExtShowSectorData(sectnum);
	}
}

void ExtEditWallData(short wallnum)       //F8
{
	short nickdata;

	if (qsetmode == 200)    //In 3D mode
	{
	}
	else
	{
		std::sprintf((char *)&tempbuf[0],"Wall (%d) Nick's variable: ",wallnum);
		nickdata = 0;
		nickdata = getnumber16((char *)&tempbuf[0], nickdata, 65536L, 0);

		printmessage16("");              //Clear message box (top right of status bar)
		ExtShowWallData(wallnum);
	}
}

void ExtEditSpriteData(short spritenum)   //F8
{
	short nickdata;

	if (qsetmode == 200)    //In 3D mode
	{
	}
	else
	{
		std::sprintf((char *)&tempbuf[0],"Sprite (%d) Nick's variable: ",spritenum);
		nickdata = 0;
		nickdata = getnumber16((char *)&tempbuf[0], nickdata, 65536L, 0);
		printmessage16("");

		printmessage16("");              //Clear message box (top right of status bar)
		ExtShowSpriteData(spritenum);
	}
}

void faketimerhandler()
{
	sampletimer();
}

	//Just thought you might want my getnumber16 code
/*
getnumber16(char namestart[80], short num, int maxnumber)
{
	char buffer[80];
	int j, k, n, danum, oldnum;

	danum = (int)num;
	oldnum = danum;
	while ((keystatus[0x1c] != 2) && (keystatus[0x1] == 0))  //Enter, ESC
	{
		sprintf(&buffer,"%s%ld_ ",namestart,danum);
		printmessage16(buffer);

		for(j=2;j<=11;j++)                //Scan numbers 0-9
			if (keystatus[j] > 0)
			{
				keystatus[j] = 0;
				k = j-1;
				if (k == 10) k = 0;
				n = (danum*10)+k;
				if (n < maxnumber) danum = n;
			}
		if (keystatus[0xe] > 0)    // backspace
		{
			danum /= 10;
			keystatus[0xe] = 0;
		}
		if (keystatus[0x1c] == 1)   //L. enter
		{
			oldnum = danum;
			keystatus[0x1c] = 2;
			asksave = true;
		}
	}
	keystatus[0x1c] = 0;
	keystatus[0x1] = 0;
	return((short)oldnum);
}
*/

/*
 * vim:ts=4:
 */

