// A.ASM replacement using C
// Mainly by Ken Silverman, with things melded with my port by
// Jonathon Fowler (jf@jonof.id.au)
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.

#include "a.hpp"

#ifdef USING_A_C

#include "compat.hpp"

#include <utility>

int krecip(int num);	// from engine.c

constexpr auto BITSOFPRECISION{3};
constexpr auto BITSOFPRECISIONPOW{8};

extern int asm1;
extern int asm2;
extern int asm4;
extern int fpuasm;
extern int globalx3;
extern int globaly3;
extern intptr_t asm3;

namespace {

int bpl;
bool transmode{false};
int glogx;
int glogy;
int gbxinc;
int gbyinc;
int gpinc;
unsigned char *gbuf;
unsigned char *gpal;
unsigned char *ghlinepal;
unsigned char *gtrans;

} // namespace

	//Global variable functions
void setvlinebpl(int dabpl) {
	bpl = dabpl;
}

void fixtransluscence(void *datransoff) {
	gtrans = static_cast<unsigned char *>(datransoff);
}

void settransnormal() {
	transmode = false;
}

void settransreverse() {
	transmode = true;
}


	//Ceiling/floor horizontal line functions
void sethlinesizes(int logx, int logy, void *bufplc) {
	glogx = logx;
	glogy = logy;
	gbuf = static_cast<unsigned char *>(bufplc);
}

void setpalookupaddress(void *paladdr) {
	ghlinepal = static_cast<unsigned char *>(paladdr);
}

void setuphlineasm4(int bxinc, int byinc) {
	gbxinc = bxinc;
	gbyinc = byinc;
}

void hlineasm4(int cnt, int skiploadincs, int paloffs, unsigned int by, unsigned int bx, void *p)
{
	const unsigned char* palptr = &ghlinepal[paloffs];
	auto pp = static_cast<unsigned char *>(p);

	if (!skiploadincs) {
		gbxinc = asm1;
		gbyinc = asm2; 
	}

	for(; cnt >= 0; cnt--) {
		*pp = palptr[gbuf[((bx >> (32 - glogx)) << glogy) + (by >> (32 - glogy))]];
		bx -= gbxinc;
		by -= gbyinc;
		pp--;
	}
}


	//Sloped ceiling/floor vertical line functions
void setupslopevlin(int logylogx, void *bufplc, int pinc)
{
	glogx = (logylogx & 255);
	glogy = (logylogx >> 8);
	gbuf = static_cast<unsigned char *>(bufplc);
	gpinc = pinc;
}

void slopevlin(void *p, int i, void *slopaloffs, int cnt, int bx, int by)
{
	int bz = (int)asm3;
	const int bzinc = (asm1 >> 3);
	auto slopalptr = static_cast<intptr_t *>(slopaloffs);
	auto pp = static_cast<unsigned char *>(p);

	for(; cnt > 0; cnt--)
	{
		i = krecip(bz >> 6);
		bz += bzinc;
		const unsigned int u = bx + globalx3 * i;
		const unsigned int v = by + globaly3 * i;
		*pp = *(unsigned char *)(slopalptr[0] + gbuf[((u >> (32 - glogx)) << glogy) + (v >> (32 - glogy))]); // FIXME: Correct cast?
		slopalptr--;
		pp += gpinc;
	}
}


	//Wall,face sprite/wall sprite vertical line functions
void setupvlineasm(int neglogy) {
	glogy = neglogy;
}

void vlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p)
{
	gbuf = static_cast<unsigned char *>(bufplc);
	gpal = static_cast<unsigned char *>(paloffs);
	auto pp = static_cast<unsigned char *>(p);

	for(; cnt >= 0; cnt--)
	{
		*pp = gpal[gbuf[vplc >> glogy]];
		pp += bpl;
		vplc += vinc;
	}
}

void setupmvlineasm(int neglogy) {
	glogy = neglogy;
}

void mvlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p)
{
	gbuf = static_cast<unsigned char *>(bufplc);
	gpal = static_cast<unsigned char *>(paloffs);
	auto pp = static_cast<unsigned char *>(p);

	for(; cnt >= 0; cnt--)
	{
		const unsigned char ch = gbuf[vplc >> glogy];
		
		if (ch != 255) {
			*pp = gpal[ch];
		}

		pp += bpl;
		vplc += vinc;
	}
}

void setuptvlineasm(int neglogy) {
	glogy = neglogy;
}

void tvlineasm1(int vinc, void *paloffs, int cnt, unsigned int vplc, void *bufplc, void *p)
{
	gbuf = static_cast<unsigned char *>(bufplc);
	gpal = static_cast<unsigned char *>(paloffs);
	auto pp = static_cast<unsigned char *>(p);

	if (transmode)
	{
		for(; cnt >= 0; cnt--)
		{
			const unsigned char ch = gbuf[vplc >> glogy];

			if (ch != 255) {
				*pp = gtrans[(*pp) + (gpal[ch] << 8)];
			}

			pp += bpl;
			vplc += vinc;
		}
	}
	else
	{
		for(; cnt >= 0; cnt--)
		{
			const unsigned char ch = gbuf[vplc >> glogy];

			if (ch != 255) {
				*pp = gtrans[((*pp) << 8) + gpal[ch]];
			}

			pp += bpl;
			vplc += vinc;
		}
	}
}

	//Floor sprite horizontal line functions
void msethlineshift(int logx, int logy) {
	glogx = logx;
	glogy = logy;
}

void mhline(void *bufplc, unsigned int bx, int cntup16, int junk, unsigned int by, void *p)
{
	std::ignore = junk;

	gbuf = static_cast<unsigned char *>(bufplc);
	gpal = (unsigned char *)asm3; // FIXME: Correct cast?
	auto pp = static_cast<unsigned char *>(p);

	for(cntup16 >>= 16; cntup16 > 0; cntup16--)
	{
		const unsigned char ch = gbuf[((bx >> (32 - glogx)) << glogy) + (by >> (32 - glogy))];
		
		if (ch != 255) {
			*pp = gpal[ch];
		}

		bx += asm1;
		by += asm2;
		pp++;
	}
}

void tsethlineshift(int logx, int logy) {
	glogx = logx;
	glogy = logy;
}

void thline(void *bufplc, unsigned int bx, int cntup16, int junk, unsigned int by, void *p)
{
	std::ignore = junk;

	gbuf = static_cast<unsigned char *>(bufplc);
	gpal = (unsigned char *)asm3; // FIXME: Correct cast?
	auto pp = static_cast<unsigned char *>(p);

	if (transmode)
	{
		for(cntup16 >>= 16; cntup16 > 0; cntup16--)
		{
			const auto ch = gbuf[((bx >> (32 - glogx)) << glogy) + (by >> (32 - glogy))];

			if (ch != 255) {
				*pp = gtrans[(*pp) + (gpal[ch] << 8)];
			}

			bx += asm1;
			by += asm2;
			pp++;
		}
	}
	else
	{
		for(cntup16 >>= 16; cntup16 > 0; cntup16--)
		{
			const unsigned char ch = gbuf[((bx >> (32 - glogx)) << glogy) + (by >> (32 - glogy))];

			if (ch != 255) {
				*pp = gtrans[((*pp) << 8) + gpal[ch]];
			}

			bx += asm1;
			by += asm2;
			pp++;
		}
	}
}


	//Rotatesprite vertical line functions
void setupspritevline(void *paloffs, int bxinc, int byinc, int ysiz)
{
	gpal = static_cast<unsigned char *>(paloffs);
	gbxinc = bxinc;
	gbyinc = byinc;
	glogy = ysiz;
}

void spritevline(int bx, int by, int cnt, void *bufplc, void *p)
{
	gbuf = static_cast<unsigned char *>(bufplc);
	auto pp = static_cast<unsigned char *>(p);

	for(;cnt>1;cnt--)
	{
		*pp = gpal[gbuf[(bx >> 16) * glogy + (by >> 16)]];
		bx += gbxinc;
		by += gbyinc;
		pp += bpl;
	}
}

	//Rotatesprite vertical line functions
void msetupspritevline(void *paloffs, int bxinc, int byinc, int ysiz)
{
	gpal = static_cast<unsigned char *>(paloffs);
	gbxinc = bxinc;
	gbyinc = byinc;
	glogy = ysiz;
}

void mspritevline(int bx, int by, int cnt, void *bufplc, void *p)
{
	gbuf = static_cast<unsigned char *>(bufplc);
	auto pp = static_cast<unsigned char *>(p);

	for(;cnt>1;cnt--)
	{
		const unsigned char ch = gbuf[(bx >> 16) * glogy + (by >> 16)];

		if (ch != 255) {
			*pp = gpal[ch];
		}

		bx += gbxinc;
		by += gbyinc;
		pp += bpl;
	}
}

void tsetupspritevline(void *paloffs, int bxinc, int byinc, int ysiz)
{
	gpal = static_cast<unsigned char *>(paloffs);
	gbxinc = bxinc;
	gbyinc = byinc;
	glogy = ysiz;
}

void tspritevline(int bx, int by, int cnt, void *bufplc, void *p)
{
	gbuf = static_cast<unsigned char *>(bufplc);
	auto pp = static_cast<unsigned char *>(p);

	if (transmode)
	{
		for(;cnt>1;cnt--)
		{
			const unsigned char ch = gbuf[(bx >> 16) * glogy + (by >> 16)];

			if (ch != 255) {
				*pp = gtrans[(*pp) + (gpal[ch] << 8)];
			}

			bx += gbxinc;
			by += gbyinc;
			pp += bpl;
		}
	}
	else
	{
		for(;cnt>1;cnt--)
		{
			const unsigned char ch = gbuf[(bx >> 16) * glogy + (by >> 16)];

			if (ch != 255) {
				*pp = gtrans[((*pp) << 8) + gpal[ch]];
			}

			bx += gbxinc;
			by += gbyinc;
			pp += bpl;
		}
	}
}

void setupdrawslab (int dabpl, void *pal) {
	bpl = dabpl;
	gpal = static_cast<unsigned char *>(pal);
}

void drawslab (int dx, int v, int dy, int vi, void *vptr, void *p)
{	
	auto pp = static_cast<unsigned char *>(p);
	auto vpptr = static_cast<const unsigned char *>(vptr);

	while (dy > 0)
	{
		for(int x{0}; x < dx; x++) {
			*(pp + x) = gpal[(int)(*(vpptr + (v >> 16)))];
		}

		pp += bpl;
		v += vi;
		dy--;
	}
}

void stretchhline (void *p0, int u, int cnt, int uinc, void *rptr, void *p)
{
	std::ignore = p0;

	auto rpptr = static_cast<const unsigned char *>(rptr);
	auto pp = static_cast<unsigned char *>(p);
	auto np = (const unsigned char *)((intptr_t)p - (cnt << 2)); // FIXME: Correct cast?
	
	do
	{
		pp--;
		*pp = *(rpptr+(u >> 16));
		u -= uinc;
	} while (pp > np);
}


void mmxoverlay() { }

#endif
/*
 * vim:ts=4:
 */

