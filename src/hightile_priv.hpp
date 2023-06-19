#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef HIGHTILE_PRIV_H
#define HIGHTILE_PRIV_H

#include <array>
#include <memory>

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
	std::array<std::string, 6> face;
};

struct hicreplctyp {
	hicreplctyp* next;
	unsigned char palnum, ignore, flags, filler;
	std::string filename;
	float alphacut;
	std::unique_ptr<hicskybox_t> skybox;
};

inline std::array<palette_t, MAXPALOOKUPS> hictinting{};
inline std::array<hicreplctyp*, MAXTILES> hicreplc{};
inline int hicfirstinit{0};

void hicinit();
hicreplctyp * hicfindsubst(int picnum, int palnum, int skybox);

#endif
