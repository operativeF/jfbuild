// game.h

constexpr auto NUMOPTIONS{8};
constexpr auto NUMKEYS{20};

extern std::array<char, NUMOPTIONS> option;

inline std::array<int, NUMKEYS> keys =
{
	0xc8,0xd0,0xcb,0xcd,0x2a,0x9d,0x1d,0x39,
	0x1e,0x2c,0xd1,0xc9,0x33,0x34,
	0x9c,0x1c,0xd,0xc,0xf,0x2b
};

extern int xdimgame, ydimgame, bppgame;
extern int forcesetup;

void	operatesector(short dasector);
void	operatesprite(short dasprite);
int	changehealth(short snum, short deltahealth);
void	changenumbombs(short snum, short deltanumbombs);
void	changenummissiles(short snum, short deltanummissiles);
void	changenumgrabbers(short snum, short deltanumgrabbers);
void	drawstatusflytime(short snum);
void	drawstatusbar(short snum);
void	prepareboard(char *daboardfilename);
void	checktouchsprite(short snum, short sectnum);
void	checkgrabbertouchsprite(short snum, short sectnum);
void	shootgun(short snum, int x, int y, int z, short daang, int dahoriz, short dasectnum, unsigned char guntype);
void	analyzesprites(int dax, int day);
void	tagcode();
void	statuslistcode();
void	activatehitag(short dahitag);
void	bombexplode(int i);
void	processinput(short snum);
void	view(short snum, int *vx, int *vy, int *vz, short *vsectnum, short ang, int horiz);
void	drawscreen(short snum, int dasmoothratio);
void	movethings();
void	fakedomovethings();
void	fakedomovethingscorrect();
void	domovethings();
void	getinput();
void	initplayersprite(short snum);
void	playback();
void	setup3dscreen();
void	findrandomspot(int *x, int *y, short *sectnum);
void	warp(int *x, int *y, int *z, short *daang, short *dasector);
void	warpsprite(short spritenum);
void	initlava();
void	movelava(unsigned char *dapic);
void	doanimations();
int	getanimationgoal(int *animptr);
int	setanimation(int *animptr, int thegoal, int thevel, int theacc);
void	checkmasterslaveswitch();
int	testneighborsectors(short sect1, short sect2);
int	loadgame();
int	savegame();
void	faketimerhandler();
void	getpackets();
void	drawoverheadmap(int cposx, int cposy, int czoom, short cang);
int	movesprite(short spritenum, int dx, int dy, int dz, int ceildist, int flordist, int clipmask);
void	waitforeverybody();
void	searchmap(short startsector);
void	setinterpolation(int *posptr);
void	stopinterpolation(int *posptr);
void	updateinterpolations();
void	dointerpolations();
void	restoreinterpolations();
void	printext(int x, int y, char *buffer, short tilenum, unsigned char invisiblecol);
void	drawtilebackground (int thex, int they, short tilenum, signed char shade, int cx1, int cy1, int cx2, int cy2, unsigned char dapalnum);



struct startwin_settings {
    int fullscreen;
    int xdim3d, ydim3d, bpp3d;
    int forcesetup;

    int numplayers;
    char *joinhost;
    int netoverride;
};
