#ifndef ENGINE_PRIV_H
#define ENGINE_PRIV_H

#include <array>
#include <span>

inline constexpr auto MAXCLIPNUM{1024};
inline constexpr auto MAXPERMS{1024};
inline constexpr auto MAXTILEFILES{256};
inline constexpr auto MAXYSAVES = ((MAXXDIM * MAXSPRITES) >> 7);
inline constexpr auto MAXNODESPERLINE{42};   //WARNING: This depends on MAXYSAVES & MAXYDIM!
inline constexpr auto MAXWALLSB{4096};
inline constexpr auto MAXCLIPDIST{1024};
inline int startposx{0};
inline int startposy{0};
inline int startposz{0};
inline short startang{0};
inline short startsectnum{0};



inline constexpr std::array<unsigned char, 8> pow2char = {
	1, 2, 4, 8, 16, 32, 64, 128
};

inline constexpr std::array<int, 32> pow2long = {
	1L,2L,4L,8L,
	16L,32L,64L,128L,
	256L,512L,1024L,2048L,
	4096L,8192L,16384L,32768L,
	65536L,131072L,262144L,524288L,
	1048576L,2097152L,4194304L,8388608L,
	16777216L,33554432L,67108864L,134217728L,
	268435456L,536870912L,1073741824L,2147483647L
};

inline std::array<short, MAXWALLSB> thesector{};
inline std::array<short, MAXWALLSB> thewall{};
inline std::array<short, MAXWALLSB> bunchfirst{};
inline std::array<short, MAXWALLSB> bunchlast{};
inline std::array<short, MAXWALLSB> maskwall{};
inline short maskwallcnt{0};
inline spritetype* tspriteptr[MAXSPRITESONSCREEN]{};
inline int xdimen{-1};
inline int xdimenrecip{0};
inline int halfxdimen{0};
inline int xdimenscale{0};
inline int xdimscale{0};
inline int ydimen{0};
inline int ydimenscale{0};
inline intptr_t frameoffset{0};
inline int globalposx{0};
inline int globalposy{0};
inline int globalposz{0};
inline int globalhoriz{0};
inline short globalang{0};
inline short globalcursectnum{0};
inline int globalpal{0};
inline int cosglobalang{0};
inline int singlobalang{0};
inline int cosviewingrangeglobalang{0};
inline int sinviewingrangeglobalang{0};
extern int globalvisibility;
extern int asm1, asm2, asm4;
extern intptr_t asm3;
inline int globalshade{0};
inline short globalpicnum{0};
inline int globalx1{0};
inline int globaly2{0};
inline int globalorientation{0};

extern "C" short searchwall;
inline short searchstat{-1};

inline char inpreparemirror{0};

inline int curbrightness{0};
inline int gammabrightness{0};
inline float curgamma{1.0F};
inline unsigned char britable[16][256]{};
inline std::array<unsigned char, MAXTILES> picsiz{};
inline std::array<int, MAXYDIM> lastx{};
inline unsigned char* transluc{nullptr};
inline std::array<short, 256> sectorborder{};
inline short sectorbordercnt{0};
inline int qsetmode{0};

#if USE_POLYMOST
inline int hitallsprites{0};
#endif

inline std::array<int, MAXWALLSB> xb1{};
inline std::array<int, MAXWALLSB> rx1{};
inline std::array<int, MAXWALLSB> ry1{};
inline std::array<short, MAXWALLSB> p2{};
inline short numscans{0};
inline short numhits{0};
inline short numbunches{0};

struct textfontspec {
	const unsigned char *font;
	int charxsiz;
	int charysiz;
	int cellh;
	int cellxoff;
	int cellyoff;
};

extern const struct textfontspec textfonts[3];

#if USE_OPENGL
inline std::array<palette_t, MAXPALOOKUPS> palookupfog{};
#endif

int wallmost(std::span<short> mostbuf, int w, int sectnum, unsigned char dastat);
int wallfront(int l1, int l2);
int animateoffs(short tilenum, short fakevar);


#if defined(__WATCOMC__) && USE_ASM

#pragma aux setgotpic =\
"mov ebx, eax",\
"cmp byte ptr walock[eax], 200",\
"jae skipit",\
"mov byte ptr walock[eax], 199",\
"skipit: shr eax, 3",\
"and ebx, 7",\
"mov dl, byte ptr gotpic[eax]",\
"mov bl, byte ptr pow2char[ebx]",\
"or dl, bl",\
"mov byte ptr gotpic[eax], dl",\
parm [eax]\
modify exact [eax ebx ecx edx]
void setgotpic(int);

#elif defined(_MSC_VER) && defined(_M_IX86) && USE_ASM	// __WATCOMC__

static inline void setgotpic(int a)
{
	_asm {
		push ebx
		mov eax, a
		mov ebx, eax
		cmp byte ptr walock[eax], 200
		jae skipit
		mov byte ptr walock[eax], 199
skipit:
		shr eax, 3
		and ebx, 7
		mov dl, byte ptr gotpic[eax]
		mov bl, byte ptr pow2char[ebx]
		or dl, bl
		mov byte ptr gotpic[eax], dl
		pop ebx
	}
}

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM	// _MSC_VER

#define setgotpic(a) \
({ int __a=(a); \
	__asm__ __volatile__ ( \
			       "movl %%eax, %%ebx\n\t" \
			       "cmpb $200, %[walock](%%eax)\n\t" \
			       "jae 0f\n\t" \
			       "movb $199, %[walock](%%eax)\n\t" \
			       "0:\n\t" \
			       "shrl $3, %%eax\n\t" \
			       "andl $7, %%ebx\n\t" \
			       "movb %[gotpic](%%eax), %%dl\n\t" \
			       "movb %[pow2char](%%ebx), %%bl\n\t" \
			       "orb %%bl, %%dl\n\t" \
			       "movb %%dl, %[gotpic](%%eax)" \
			       : "=a" (__a) \
			       : "a" (__a), [walock] "m" (walock[0]), \
			         [gotpic] "m" (gotpic[0]), \
			         [pow2char] "m" (pow2char[0]) \
			       : "ebx", "edx", "memory", "cc"); \
				       __a; })

#else	// __GNUC__ && __i386__

static inline void setgotpic(int tilenume)
{
	if (walock[tilenume] < 200) walock[tilenume] = 199;
	gotpic[tilenume >> 3] |= pow2char[tilenume & 7];
}

#endif

#endif	/* ENGINE_PRIV_H */
