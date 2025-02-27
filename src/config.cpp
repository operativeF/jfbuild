// Evil and Nasty Configuration File Reader for KenBuild
// by Jonathon Fowler

#include "build.hpp"
#include "editor.hpp"
#include "osd.hpp"
#include "scriptfile.hpp"
#include "string_utils.hpp"

#ifdef RENDERTYPEWIN
#include "winlayer.hpp"
#endif
#include "baselayer.hpp"

#include <array>
#include <variant>

extern std::array<int, NUMBUILDKEYS> keys;

/*
 * SETUP.DAT
 * 0      = video mode (0:chained 1:vesa 2:screen buffered 3/4/5:tseng/paradise/s3 6:red-blue)
 * 1      = sound (0:none)
 * 2      = music (0:none)
 * 3      = input (0:keyboard 1:+mouse)
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


enum class config_t {
	type_bool,	//int
	type_double,
	type_int,
	type_hex,
	type_fixed16,	//int
};

#if USE_POLYMOST
// FIXME: Figure out way to get data into an enum here.
int tmprenderer{0};
#endif
int tmpbrightness = -1;
#ifdef RENDERTYPEWIN
unsigned tmpmaxrefreshfreq = -1;
#endif

struct configspec_t {
	std::string_view name;
	config_t type;
	std::variant<bool*, int*, double*, std::string*> store;
	std::string_view doc;
};

const auto configspec = std::to_array<configspec_t>({
	{ "forcesetup", config_t::type_bool, &forcesetup,
		"; Always show configuration options on startup\n"
		";   0 - No\n"
		";   1 - Yes\n"
	},
	{ "fullscreen", config_t::type_bool, &fullscreen,
		"; Video mode selection\n"
		";   0 - Windowed\n"
		";   1 - Fullscreen\n"
	},
	{ "xdim2d", config_t::type_int, &xdim2d,
		"; Video resolution\n"
	},
	{ "ydim2d", config_t::type_int, &ydim2d, {} },
	{ "xdim3d", config_t::type_int, &xdimgame, {} },
	{ "ydim3d", config_t::type_int, &ydimgame, {} },
	{ "bpp",    config_t::type_int, &bppgame,
		"; 3D-mode colour depth\n"
	},
#if USE_POLYMOST
	{ "renderer", config_t::type_int, &tmprenderer,
		"; 3D-mode renderer type\n"
		";   0  - classic\n"
		";   2  - software Polymost\n"
		";   3  - OpenGL Polymost\n"
	},
#endif
	{ "brightness", config_t::type_int, &tmpbrightness,
		"; 3D mode brightness setting\n"
		";   0  - lowest\n"
		";   15 - highest\n"
	},
#if USE_POLYMOST && USE_OPENGL
	{ "glusetexcache", config_t::type_bool, &glusetexcache,
		"; OpenGL mode options\n"
	},
#endif
#ifdef RENDERTYPEWIN
	{ "maxrefreshfreq", config_t::type_int, &tmpmaxrefreshfreq,
		"; Maximum OpenGL mode refresh rate (Windows only, in Hertz)\n"
	},
#endif
	{ "mousesensitivity", config_t::type_fixed16, &msens,
		"; Mouse sensitivity\n"
	},
	{ "keyforward", config_t::type_hex, &keys[0],
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
	{ "keybackward", config_t::type_hex, &keys[1], {} },
	{ "keyturnleft", config_t::type_hex, &keys[2], {} },
	{ "keyturnright", config_t::type_hex, &keys[3], {} },
	{ "keyrun", config_t::type_hex, &keys[4], {} },
	{ "keystrafe", config_t::type_hex, &keys[5], {} },
	{ "keyfire", config_t::type_hex, &keys[6], {} },
	{ "keyuse", config_t::type_hex, &keys[7], {} },
	{ "keystandhigh", config_t::type_hex, &keys[8], {} },
	{ "keystandlow", config_t::type_hex, &keys[9], {} },
	{ "keylookup", config_t::type_hex, &keys[10], {} },
	{ "keylookdown", config_t::type_hex, &keys[11], {} },
	{ "keystrafeleft", config_t::type_hex, &keys[12], {} },
	{ "keystraferight", config_t::type_hex, &keys[13], {} },
	{ "key2dmode", config_t::type_hex, &keys[14], {} },
	{ "keyviewcycle", config_t::type_hex, &keys[15], {} },
	{ "key2dzoomin", config_t::type_hex, &keys[16], {} },
	{ "key2dzoomout", config_t::type_hex, &keys[17], {} },
	{ "keychat", config_t::type_hex, &keys[18], {} },
	{ "keyconsole", config_t::type_hex, &keys[19], {} }
});

} // namespace

// FIXME: Use a variant instead of switch-type resolver.
int loadsetup(const std::string& fn)
{
	auto cfg = scriptfile_fromfile(fn);
	
	if (!cfg.get()) {
		return -1;
	}

	scriptfile_clearsymbols();

	option[0] = 1;	// vesa all the way...
	option[1] = 1;	// sound all the way...
	option[4] = 0;	// no multiplayer
	option[5] = 0;

	while (1) {
		auto token = scriptfile_gettoken(cfg.get());

		if (!token.has_value()) {
			break;	//EOF
		}
		
		for (const auto& cfgitem : configspec) {
			if (IsSameAsNoCase(token.value(), cfgitem.name)) {
				// Seek past any = symbol.
				token = scriptfile_peektoken(cfg.get());
				
				if(!token.has_value())
					break;

				if (token.value() == "=") {
					scriptfile_gettoken(cfg.get());
				}

				switch (cfgitem.type) {
					case config_t::type_bool: {
						auto value = scriptfile_getbool(cfg.get());
						if (!value.has_value()) break;
						*std::get<bool*>(cfgitem.store) = value.value();
						break;
					}
					case config_t::type_int: {
						auto value = scriptfile_getnumber(cfg.get());
						if (!value.has_value()) break;
						*std::get<int*>(cfgitem.store) = value.value();
						break;
					}
					case config_t::type_hex: {
						auto value = scriptfile_gethex(cfg.get());
						if (!value.has_value()) break;
						*std::get<int*>(cfgitem.store) = value.value();
						break;
					}
					case config_t::type_fixed16: {
						auto value = scriptfile_getdouble(cfg.get());
						if (!value.has_value())
							break;
						*std::get<int*>(cfgitem.store) = (int)(value.value() * 65536.0);
						break;
					}
					case config_t::type_double: {
						auto value = scriptfile_getdouble(cfg.get());
						if (!value.has_value())
							break;
						*std::get<double*>(cfgitem.store) = value.value();
						break;
					}
					default: {
						buildputs("loadsetup: unhandled value type\n");
						break;
					}
				}
				
				if (cfgitem.name.empty()) {
					buildprintf("loadsetup: error on line {}\n", scriptfile_getlinum(cfg.get(), cfg->ltextptr));
					continue;
				}
			}
		}
	}

#if USE_POLYMOST
	setrendermode(static_cast<rendmode_t>(tmprenderer));
#endif
#ifdef RENDERTYPEWIN
	win_setmaxrefreshfreq(tmpmaxrefreshfreq);
#endif
	if (tmpbrightness >= 0) {
		brightness = std::min(std::max(tmpbrightness, 0), 15);
	}
	OSD_CaptureKey(keys[19]);

	scriptfile_clearsymbols();

	return 0;
}

// FIXME: Use a variant instead of a switch type resolver.
int writesetup(const std::string& fn)
{
	std::FILE* fp = std::fopen(fn.c_str(), "wt");

	if (!fp) {
		return -1;
	}

	tmpbrightness = brightness;
#if USE_POLYMOST
	tmprenderer = static_cast<std::underlying_type_t<rendmode_t>>(getrendermode());
#endif

#ifdef RENDERTYPEWIN
	tmpmaxrefreshfreq = win_getmaxrefreshfreq();
#endif

	for (const auto& cfgitem : configspec) {
		if (!cfgitem.doc.empty()) {
			fmt::print(fp, "\n{}", cfgitem.doc);
		}

		fmt::print(fp, "{} = ", cfgitem.name);
		
		switch (cfgitem.type) {
			case config_t::type_bool: {
				fmt::print(fp, "{}\n", *std::get<bool*>(cfgitem.store));
				break;
			}
			case config_t::type_int: {
				fmt::print(fp, "{}\n", *std::get<int*>(cfgitem.store));
				break;
			}
			case config_t::type_hex: {
				fmt::print(fp, "{:X}\n", *std::get<int*>(cfgitem.store));
				break;
			}
			case config_t::type_fixed16: {
				fmt::print(fp, "{}\n", static_cast<double>(*std::get<int*>(cfgitem.store)) / 65536.0);
				break;
			}
			case config_t::type_double: {
				fmt::print(fp, "{}\n", *std::get<double*>(cfgitem.store));
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
