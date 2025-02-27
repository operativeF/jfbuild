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
#include <vector>

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
	bool ignore{false};
	std::array<std::string, 6> face;
};

struct hicreplctyp {
	unsigned char palnum;
	bool ignore{false};
	unsigned char flags;
	unsigned char filler;
	std::string filename;
	float alphacut;
	hicskybox_t skybox;
};

inline std::array<palette_t, MAXPALOOKUPS> hictinting{};
inline std::vector<std::vector<hicreplctyp>> hicreplc(MAXTILES);
inline int hicfirstinit{0};

void hicinit();
hicreplctyp * hicfindsubst(int picnum, int palnum, int skybox);

#endif
