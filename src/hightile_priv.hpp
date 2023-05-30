#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef HIGHTILE_PRIV_H
#define HIGHTILE_PRIV_H

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

extern palette_t hictinting[MAXPALOOKUPS];
extern hicreplctyp *hicreplc[MAXTILES];
extern int hicfirstinit;

void hicinit();
hicreplctyp * hicfindsubst(int picnum, int palnum, int skybox);

#endif
