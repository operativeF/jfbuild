#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef HIGHTILE_PRIV_H
#define HIGHTILE_PRIV_H

#include <array>

enum {
	HICEFFECT_NONE = 0,
	HICEFFECT_GREYSCALE = 1,
	HICEFFECT_INVERT = 2,
	HICEFFECTMASK = 3,
};

enum {
	HIC_NOCOMPRESS = 1,
};

struct hicskybox_t {
	long ignore;
	char *face[6];
};

struct hicreplctyp {
	hicreplctyp* next;
	unsigned char palnum, ignore, flags, filler;
	char *filename;
	float alphacut;
	struct hicskybox_t *skybox;
};

extern std::array<palette_t, MAXPALOOKUPS> hictinting;
extern std::array<hicreplctyp*, MAXTILES> hicreplc;
extern int hicfirstinit;

void hicinit();
hicreplctyp * hicfindsubst(int picnum, int palnum, int skybox);

#endif
