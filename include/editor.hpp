// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.

#ifndef __editor_h__
#define __editor_h__

#include <string>

inline constexpr auto NUMBUILDKEYS{20};

// extern int qsetmode;
extern short searchwall;
extern short searchstat;
extern int zmode;
extern int kensplayerheight;
extern int kenswalldist;
extern short defaultspritecstat;
extern int posx;
extern int posy;
extern int posz;
extern int horiz;
extern short ang;
extern short cursectnum;
extern short ceilingheinum;
extern short floorheinum;
extern int zlock;
extern bool editstatus;
extern short searchit;

extern short temppicnum;
extern short tempcstat;
extern short templotag;
extern short temphitag;
extern short tempextra;
extern unsigned char tempshade;
extern unsigned char temppal;
extern unsigned char tempxrepeat;
extern unsigned char tempyrepeat;
extern unsigned char somethingintab;
extern char names[MAXTILES][25];

extern std::array<int, NUMBUILDKEYS> buildkeys;

inline int ydim16{0};
inline int halfxdim16{0};
inline int midydim16{0};
extern int ydimgame;
extern int bppgame;
extern int forcesetup;

struct startwin_settings {
    int fullscreen;
    int xdim2d;
    int ydim2d;
    int xdim3d;
    int ydim3d;
    int bpp3d;
    int forcesetup;
};

extern int ExtInit();
extern void ExtUnInit();
extern void ExtPreCheckKeys();
extern void ExtAnalyzeSprites();
extern void ExtCheckKeys();
extern void ExtPreLoadMap();
extern void ExtLoadMap(const char *mapname);
extern void ExtPreSaveMap();
extern void ExtSaveMap(const char *mapname);
extern const char *ExtGetSectorCaption(short sectnum);
extern const char *ExtGetWallCaption(short wallnum);
extern const char *ExtGetSpriteCaption(short spritenum);
extern void ExtShowSectorData(short sectnum);
extern void ExtShowWallData(short wallnum);
extern void ExtShowSpriteData(short spritenum);
extern void ExtEditSectorData(short sectnum);
extern void ExtEditWallData(short wallnum);
extern void ExtEditSpriteData(short spritenum);

inline constexpr auto STATUS2DSIZ{144};

int loadsetup(const std::string& fn);	// from config.c
int writesetup(const std::string& fn);	// from config.c

void editinput();
void clearmidstatbar16();

void drawline16(int x1, int y1, int x2, int y2, unsigned char col);
void drawcircle16(int x1, int y1, int r, unsigned char col);

void printext16(int xpos, int ypos, short col, short backcol, std::string_view name, char fontsize);
short getnumber256(char *namestart, short num, int maxnumber, char sign);
short getnumber16(char *namestart, short num, int maxnumber, char sign);
void printmessage256(const char* name);
void printmessage16(const char* name);

void getpoint(int searchxe, int searchye, int *x, int *y);
int getpointhighlight(int xplc, int yplc);

#endif
