// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "build.hpp"
#include "osd.hpp"
#include "baselayer.hpp"
#include "baselayer_priv.hpp"
#include "string_utils.hpp"

#ifdef RENDERTYPEWIN
#include "winlayer.hpp"
#endif

#if USE_OPENGL
#include "glbuild.hpp"
struct glbuild_info glinfo;
#endif //USE_OPENGL

#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>

void (*baselayer_videomodewillchange)() = nullptr;
void (*baselayer_videomodedidchange)() = nullptr;

//
// checkvideomode() -- makes sure the video mode passed is legal
//
int checkvideomode(int *x, int *y, int c, int fs, int forced)
{
	int nearest{-1};
	int odx = std::numeric_limits<int>::max();
	int ody = std::numeric_limits<int>::max();

	getvalidmodes();

#if USE_OPENGL
	if (c > 8 && glunavailable)
		return -1;
#else
	if (c > 8)
		return -1;
#endif

	// fix up the passed resolution values to be multiples of 8
	// and at least 320x200 or at most MAXXDIMxMAXYDIM
	*x = std::clamp(*x, 320, MAXXDIM);
	*y = std::clamp(*y, 200, MAXYDIM);
	*x &= 0xfffffff8L;

	for (int i{0}; const auto& vmode : validmode) {
		if (vmode.bpp != c)
			continue;
		
		if (vmode.fs != fs)
			continue;

		const int dx = std::abs(vmode.xdim - *x);
		const int dy = std::abs(vmode.ydim - *y);

		if (!(dx | dy)) { 	// perfect match
			nearest = i;
			break;
		}

		if ((dx <= odx) && (dy <= ody)) {
			nearest = i;
			odx = dx;
			ody = dy;
		}

		++i;
	}

#ifdef ANY_WINDOWED_SIZE
	if (!forced && (fs&1) == 0 && (nearest < 0 || validmode[nearest].xdim!=*x || validmode[nearest].ydim!=*y)) {
		// check the colour depth is recognised at the very least
		if (const auto vit = std::ranges::find_if(validmode, [c](const auto& vmode) { return vmode.bpp == c; });
			vit != validmode.end())
			return 0x7FFFFFFFL;

		return -1; // strange colour depth
	}
#endif

	if (nearest < 0) {
		// no mode that will match (eg. if no fullscreen modes)
		return -1;
	}

	*x = validmode[nearest].xdim;
	*y = validmode[nearest].ydim;

	return nearest;		// JBF 20031206: Returns the mode number
}

//
// bgetchar, bkbhit, bflushchars -- character-based input functions
//
unsigned char bgetchar()
{
	if (keyasciififoplc == keyasciififoend) {
		return 0;
	}

	const unsigned char c = keyasciififo[keyasciififoplc];
	keyasciififoplc = ((keyasciififoplc+1)&(KEYFIFOSIZ-1));
	return c;
}

int bkbhit()
{
	return keyasciififoplc != keyasciififoend;
}

void bflushchars()
{
	keyasciififoplc = 0;
	keyasciififoend = 0;
}

int bgetkey()
{
	if (keyfifoplc == keyfifoend) {
		return 0;
	}

	int c = keyfifo[keyfifoplc];
	if (!keyfifo[(keyfifoplc + 1) & (KEYFIFOSIZ - 1)]) {
		c = -c;
	}

	keyfifoplc = (keyfifoplc + 2) & (KEYFIFOSIZ - 1);

	return c;
}

int bkeyhit()
{
	return keyfifoplc != keyfifoend;
}

void bflushkeys()
{
	keyfifoplc = 0;
	keyfifoend = 0;
}


#if USE_POLYMOST
static int osdfunc_setrendermode(const osdfuncparm_t *parm)
{
	constexpr std::string_view modestrs[] = {
		"classic software",
		"polygonal flat-shaded software",
		"polygonal textured software",
		"polygonal OpenGL"
	};

	if (parm->parms.size() != 1)
		return OSDCMD_SHOWHELP;

	const std::string_view parmv{parm->parms[0]};
	int m{0};
	auto [ptr, ec] = std::from_chars(parmv.data(), parmv.data() + parmv.size(), m);

	if (m < 0 || m > 3 || (ec != std::errc{}))
		return OSDCMD_SHOWHELP;

	setrendermode(m);
	buildprintf("Rendering method changed to {}\n", modestrs[ getrendermode() ] );

	return OSDCMD_OK;
}
#endif //USE_POLYMOST

#if defined(DEBUGGINGAIDS) && USE_POLYMOST && USE_OPENGL
static int osdcmd_hicsetpalettetint(const osdfuncparm_t *parm)
{
	int pal, cols[3], eff;

	if (parm->parms.size() != 5) return OSDCMD_SHOWHELP;

	pal = atoi(parm->parms[0]);
	cols[0] = atoi(parm->parms[1]);
	cols[1] = atoi(parm->parms[2]);
	cols[2] = atoi(parm->parms[3]);
	eff = atoi(parm->parms[4]);

	hicsetpalettetint(pal,cols[0],cols[1],cols[2],eff);

	return OSDCMD_OK;
}
#endif //DEBUGGINGAIDS && USE_POLYMOST && USE_OPENGL

static int osdcmd_vars(const osdfuncparm_t *parm)
{
	const bool showval = parm->parms.size() < 1;

	if (IsSameAsNoCase(parm->name, "screencaptureformat")) {
		constexpr std::array<const char*, 3> fmts = { "TGA", "PCX", "PNG" };
		if (!showval) {
			int i;
			for (i=0; i<3; i++)
				if (IsSameAsNoCase(parm->parms[0], fmts[i]) || std::atoi(parm->parms[0].data()) == i) {
					captureformat = i;
					break;
				}
			if (i == 3) return OSDCMD_SHOWHELP;
		}
		buildprintf("screencaptureformat is {}\n", fmts[captureformat]);
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "novoxmips")) {
		if (showval) {
			buildprintf("novoxmips is {}\n", novoxmips);
		}
		else {
			const std::string_view parmv{parm->parms[0]};
			int tmpval{0};
			std::from_chars(parmv.data(), parmv.data() + parmv.size(), tmpval);
			// TODO: Use return values here?
			novoxmips = tmpval != 0;
		}
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "usevoxels")) {
		if (showval) { buildprintf("usevoxels is {}\n", usevoxels); }
		else {
			const std::string_view parmv{parm->parms[0]};
			int tmpval{0};
			std::from_chars(parmv.data(), parmv.data() + parmv.size(), tmpval);
			// TODO: Use return values here?
			usevoxels = tmpval != 0;
		}
		return OSDCMD_OK;
	}
	return OSDCMD_SHOWHELP;
}

int baselayer_init()
{
    OSD_Init();

	OSD_RegisterFunction("screencaptureformat","screencaptureformat: sets the output format for screenshots (TGA, PCX, PNG)",osdcmd_vars);

	OSD_RegisterFunction("novoxmips","novoxmips: turn off/on the use of mipmaps when rendering 8-bit voxels",osdcmd_vars);
	OSD_RegisterFunction("usevoxels","usevoxels: enable/disable automatic sprite->voxel rendering",osdcmd_vars);

#if USE_POLYMOST
	OSD_RegisterFunction("setrendermode","setrendermode <number>: sets the engine's rendering mode.\n"
			"Mode numbers are:\n"
			"   0 - Classic Build software\n"
			"   1 - Polygonal flat-shaded software\n"
			"   2 - Polygonal textured software\n"
#if USE_OPENGL
			"   3 - Polygonal OpenGL\n"
#endif
			,
			osdfunc_setrendermode);
#endif //USE_POLYMOST

#if defined(DEBUGGINGAIDS) && USE_POLYMOST && USE_OPENGL
	OSD_RegisterFunction("hicsetpalettetint","hicsetpalettetint: sets palette tinting values",osdcmd_hicsetpalettetint);
#endif

	return 0;
}

