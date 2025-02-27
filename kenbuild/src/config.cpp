// Evil and Nasty Configuration File Reader for KenBuild
// by Jonathon Fowler

#include "build.hpp"
#include "game.hpp"
#include "osd.hpp"
#include "scriptfile.hpp"
#include "string_utils.hpp"

#ifdef RENDERTYPEWIN
#include "winlayer.hpp"
#endif
#include "baselayer.hpp"

#include <fmt/core.h>

#include <array>

/*
 * SETUP.DAT
 * 0      = video mode (0:chained 1:vesa 2:screen buffered 3/4/5:tseng/paradise/s3 6:red-blue)
 * 1      = sound (0:none)
 * 2      = music (0:none)
 * 3      = input (0:keyboard 1:+mouse 2:+joystick)
 * 4      = multiplayer (0:single 1-4:com 5-11:ipx)
 * 5&0xf0 = com speed
 * 5&0x0f = com irq
 * 6&0xf0 = chained y-res
 * 6&0x0f = chained x-res or vesa mode
 * 7&0xf0 = sound samplerate
 * 7&0x01 = sound quality
 * 7&0x02 = 8/16 bit
 * 7&0x04 = mono/stereo
 *
 * bytes 8 to 26 are key settings:
 * 0      = Forward (0xc8)
 * 1      = Backward (0xd0)
 * 2      = Turn left (0xcb)
 * 3      = Turn right (0xcd)
 * 4      = Run (0x2a)
 * 5      = Strafe (0x9d)
 * 6      = Fire (0x1d)
 * 7      = Use (0x39)
 * 8      = Stand high (0x1e)
 * 9      = Stand low (0x2c)
 * 10     = Look up (0xd1)
 * 11     = Look down (0xc9)
 * 12     = Strafe left (0x33)
 * 13     = Strafe right (0x34)
 * 14     = 2D/3D switch (0x9c)
 * 15     = View cycle (0x1c)
 * 16     = 2D Zoom in (0xd)
 * 17     = 2D Zoom out (0xc)
 * 18     = Chat (0xf)
 */

namespace {

enum {
	type_bool = 0,	//int
	type_double = 1,
	type_int = 2,
	type_hex = 3,
};

#if USE_POLYMOST
int tmprenderer = -1;
#endif
int tmpbrightness = -1;
int tmpsamplerate = -1;
int tmpmusic = -1;
int tmpmouse = -1;
int tmpjoystick = -1;
#ifdef RENDERTYPEWIN
unsigned tmpmaxrefreshfreq = -1;
#endif

struct {
	const char *name;
	int type;
	void *store;
	const char *doc;
} configspec[] = {
	{ "forcesetup", type_bool, &forcesetup,
		"; Always show configuration options on startup\n"
		";   0 - No\n"
		";   1 - Yes\n"
	},
	{ "fullscreen", type_bool, &fullscreen,
		"; Video mode selection\n"
		";   0 - Windowed\n"
		";   1 - Fullscreen\n"
	},
	{ "xdim", type_int, &xdimgame,
		"; Video resolution\n"
	},
	{ "ydim", type_int, &ydimgame, nullptr },
	{ "bpp",  type_int, &bppgame,
		"; Video colour depth\n"
	},
#if USE_POLYMOST
	{ "renderer", type_int, &tmprenderer,
		"; 3D-mode renderer type\n"
		";   0  - classic\n"
		";   2  - software Polymost\n"
		";   3  - OpenGL Polymost\n"
	},
#endif
	{ "brightness", type_int, &tmpbrightness,
		"; 3D mode brightness setting\n"
		";   0  - lowest\n"
		";   15 - highest\n"
	},
#if USE_POLYMOST && USE_OPENGL
	{ "glusetexcache", type_bool, &glusetexcache,
		"; OpenGL mode options\n"
	},
#endif
#ifdef RENDERTYPEWIN
	{ "maxrefreshfreq", type_int, &tmpmaxrefreshfreq,
		"; Maximum OpenGL mode refresh rate (Windows only, in Hertz)\n"
	},
#endif
	{ "samplerate", type_int, &tmpsamplerate,
		"; Sound sample frequency\n"
		";   0 - 6 KHz\n"
		";   1 - 8 KHz\n"
		";   2 - 11.025 KHz\n"
		";   3 - 16 KHz\n"
		";   4 - 22.05 KHz\n"
		";   5 - 32 KHz\n"
		";   6 - 44.1 KHz\n"
		";   7 - 48 KHz\n"
	},
	{ "music", type_bool, &tmpmusic,
		"; Music playback\n"
		";   0 - Off\n"
		";   1 - On\n"
	},
	{ "mouse", type_bool, &tmpmouse,
		"; Enable mouse\n"
		";   0 - No\n"
		";   1 - Yes\n"
	},
	{ "joystick", type_bool, &tmpjoystick,
		"; Enable joystick\n"
		";   0 - No\n"
		";   1 - Yes\n"
	},
	{ "keyforward", type_hex, &keys[0],
		"; Key Settings\n"
		";  Here's a map of all the keyboard scan codes: NOTE: values are listed in hex!\n"
		"; +---------------------------------------------------------------------------------------------+\n"
		"; | 01   3B  3C  3D  3E   3F  40  41  42   43  44  57  58          46                           |\n"
		"; |ESC   F1  F2  F3  F4   F5  F6  F7  F8   F9 F10 F11 F12        SCROLL                         |\n"
		"; |                                                                                             |\n"
		"; |29  02  03  04  05  06  07  08  09  0A  0B  0C  0D   0E     D2  C7  C9      45  B5  37  4A   |\n"
		"; | ` '1' '2' '3' '4' '5' '6' '7' '8' '9' '0'  -   =  BACK    INS HOME PGUP  NUMLK KP/ KP* KP-  |\n"
		"; |                                                                                             |\n"
		"; | 0F  10  11  12  13  14  15  16  17  18  19  1A  1B  2B     D3  CF  D1      47  48  49  4E   |\n"
		"; |TAB  Q   W   E   R   T   Y   U   I   O   P   [   ]    \\    DEL END PGDN    KP7 KP8 KP9 KP+   |\n"
		"; |                                                                                             |\n"
		"; | 3A   1E  1F  20  21  22  23  24  25  26  27  28     1C                     4B  4C  4D       |\n"
		"; |CAPS  A   S   D   F   G   H   J   K   L   ;   '   ENTER                    KP4 KP5 KP6    9C |\n"
		"; |                                                                                      KPENTER|\n"
		"; |  2A    2C  2D  2E  2F  30  31  32  33  34  35    36            C8          4F  50  51       |\n"
		"; |LSHIFT  Z   X   C   V   B   N   M   ,   .   /   RSHIFT          UP         KP1 KP2 KP3       |\n"
		"; |                                                                                             |\n"
		"; | 1D     38              39                  B8     9D       CB  D0   CD      52    53        |\n"
		"; |LCTRL  LALT           SPACE                RALT   RCTRL   LEFT DOWN RIGHT    KP0    KP.      |\n"
		"; +---------------------------------------------------------------------------------------------+\n"
	},
	{ "keybackward", type_hex, &keys[1], nullptr },
	{ "keyturnleft", type_hex, &keys[2], nullptr },
	{ "keyturnright", type_hex, &keys[3], nullptr },
	{ "keyrun", type_hex, &keys[4], nullptr },
	{ "keystrafe", type_hex, &keys[5], nullptr },
	{ "keyfire", type_hex, &keys[6], nullptr },
	{ "keyuse", type_hex, &keys[7], nullptr },
	{ "keystandhigh", type_hex, &keys[8], nullptr },
	{ "keystandlow", type_hex, &keys[9], nullptr },
	{ "keylookup", type_hex, &keys[10], nullptr },
	{ "keylookdown", type_hex, &keys[11], nullptr },
	{ "keystrafeleft", type_hex, &keys[12], nullptr },
	{ "keystraferight", type_hex, &keys[13], nullptr },
	{ "key2dmode", type_hex, &keys[14], nullptr },
	{ "keyviewcycle", type_hex, &keys[15], nullptr },
	{ "key2dzoomin", type_hex, &keys[16], nullptr },
	{ "key2dzoomout", type_hex, &keys[17], nullptr },
	{ "keychat", type_hex, &keys[18], nullptr },
	{ "keyconsole", type_hex, &keys[19], nullptr },
	{ nullptr, 0, nullptr, nullptr }
};

} // namespace

int loadsetup(const std::string& fn)
{
	std::optional<std::string_view> token;
	int item;

	auto cfg = scriptfile_fromfile(fn);
	
	if (!cfg) {
		return -1;
	}

	scriptfile_clearsymbols();

	while (1) {
		token = scriptfile_gettoken(cfg.get());
		if (!token.has_value())
			break;	//EOF

		for (item = 0; configspec[item].name; item++) {
			if (IsSameAsNoCase(token.value(), configspec[item].name)) {
				// Seek past any = symbol.
				token = scriptfile_peektoken(cfg.get());
				if(!token.has_value())
					break;
				if (token.value() == "=") {
					scriptfile_gettoken(cfg.get());
				}

				switch (configspec[item].type) {
					case type_bool: {
						auto value = scriptfile_getbool(cfg.get());
						if (!value.has_value()) break;
						*(bool*)configspec[item].store = value.value();
						break;
					}
					case type_int: {
						auto value = scriptfile_getnumber(cfg.get());
						if (!value.has_value()) break;
						*(int*)configspec[item].store = value.value();
						break;
					}
					case type_hex: {
						auto value = scriptfile_gethex(cfg.get());
						if (!value.has_value()) break;
						*(int*)configspec[item].store = value.value();
						break;
					}
					case type_double: {
						auto value = scriptfile_getdouble(cfg.get());
						if (!value.has_value()) break;
						*(double*)configspec[item].store = value.value();
						break;
					}
					default: {
						buildputs("loadsetup: unhandled value type\n");
						break;
					}
				}
				break;
			}
		}
		if (!configspec[item].name) {
			buildprintf("loadsetup: error on line {}\n", scriptfile_getlinum(cfg.get(), cfg->ltextptr));
			continue;
		}
	}

#if USE_POLYMOST
	if (tmprenderer >= 0) {
		setrendermode(static_cast<rendmode_t>(tmprenderer));
	}
#endif
#ifdef RENDERTYPEWIN
	win_setmaxrefreshfreq(tmpmaxrefreshfreq);
#endif
	if (tmpbrightness >= 0) {
		brightness = std::min(std::max(tmpbrightness, 0), 15);
	}
	if (tmpsamplerate >= 0) {
		option[7] = (tmpsamplerate & 0x0f) << 4;
		option[7] |= 1|2|4;
	}
	if (tmpmusic >= 0) {
		option[2] = !!tmpmusic;
	}
	if (tmpmouse == 0) {
		option[3] &= ~1;
	} else if (tmpmouse > 0) {
		option[3] |= 1;
	}
	if (tmpjoystick == 0) {
		option[3] &= ~2;
	} else if (tmpjoystick > 0) {
		option[3] |= 2;
	}
	OSD_CaptureKey(keys[19]);

	scriptfile_clearsymbols();

	return 0;
}

int writesetup(const std::string& fn)
{
	std::FILE *fp;
	int item;

	fp = std::fopen(fn.c_str(), "wt");
	if (!fp) return -1;

	tmpbrightness = brightness;
#if USE_POLYMOST
	tmprenderer = static_cast<std::underlying_type_t<rendmode_t>>(getrendermode());
#endif
#ifdef RENDERTYPEWIN
	tmpmaxrefreshfreq = win_getmaxrefreshfreq();
#endif
	tmpsamplerate = option[7]>>4;
	tmpmusic = option[2];
	tmpmouse = !!(option[3]&1);
	tmpjoystick = !!(option[3]&2);

	for (item = 0; configspec[item].name; item++) {
		if (configspec[item].doc) {
			if (item > 0)
				fmt::print(fp, "\n");
			fmt::print(fp, "{}", configspec[item].doc);
		}
		
		fmt::print(fp, "{} = ", configspec[item].name);

		switch (configspec[item].type) {
			case type_bool: {
				fmt::print(fp, "{}\n", (*(int*)configspec[item].store != 0));
				break;
			}
			case type_int: {
				fmt::print(fp, "{}\n", *(int*)configspec[item].store);
				break;
			}
			case type_hex: {
				fmt::print(fp, "{:X}\n", *(int*)configspec[item].store);
				break;
			}
			case type_double: {
				fmt::print(fp, "{}\n", *(double*)configspec[item].store);
				break;
			}
			default: {
				fmt::print(fp, "?\n");
				break;
			}
		}
	}

	std::fclose(fp);

	return 0;
}
