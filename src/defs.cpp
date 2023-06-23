/*
 * Definitions file parser for Build
 * by Jonathon Fowler (jf@jonof.id.au)
 * Remixed substantially by Ken Silverman
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "build.hpp"
#include "baselayer.hpp"
#include "scriptfile.hpp"

#include "string_utils.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

extern int nextvoxid;

namespace {

enum class TokenType {
	T_EOF,
	T_ERROR,
	T_INCLUDE,
	T_ECHO,
	T_DEFINE,
	T_DEFINESKYBOX,
	T_DEFINETINT,
	T_DEFINEMODEL,
	T_DEFINEMODELFRAME,
	T_DEFINEMODELANIM,
	T_DEFINEMODELSKIN,
	T_SELECTMODELSKIN,
	T_DEFINEVOXEL,
	T_DEFINEVOXELTILES,
	T_MODEL,
	T_FILE,
	T_SCALE,
	T_SHADE,
	T_FRAME,
	T_ANIM,
	T_SKIN,
	T_SURF,
	T_TILE,
	T_TILE0,
	T_TILE1,
	T_FRAME0,
	T_FRAME1,
	T_FPS,
	T_FLAGS,
	T_PAL,
	T_HUD,
	T_XADD,
	T_YADD,
	T_ZADD,
	T_ANGADD,
	T_FLIPPED,
	T_HIDE,
	T_NOBOB,
	T_NODEPTH,
	T_VOXEL,
	T_SKYBOX,
	T_FRONT,T_RIGHT,T_BACK,T_LEFT,T_TOP,T_BOTTOM,
	T_TINT,T_RED,T_GREEN,T_BLUE,
	T_TEXTURE,T_ALPHACUT,T_NOCOMPRESS,
	T_UNDEFMODEL,T_UNDEFMODELRANGE,T_UNDEFMODELOF,T_UNDEFTEXTURE,T_UNDEFTEXTURERANGE,
	T_TILEFROMTEXTURE,T_GLOW,T_SETUPTILERANGE,  // EDuke32 extensions
};

struct tokenlist {
	std::string_view text;
	TokenType tokenid;
};

constexpr auto basetokens = std::to_array<tokenlist>({
	{ "include",         TokenType::T_INCLUDE          },
	{ "#include",        TokenType::T_INCLUDE          },
	{ "define",          TokenType::T_DEFINE           },
	{ "#define",         TokenType::T_DEFINE           },
	{ "echo",            TokenType::T_ECHO             },

	{ "defineskybox",     TokenType::T_DEFINESKYBOX     },
	{ "definetint",       TokenType::T_DEFINETINT       },
	{ "definemodel",      TokenType::T_DEFINEMODEL      },
	{ "definemodelframe", TokenType::T_DEFINEMODELFRAME },
	{ "definemodelanim",  TokenType::T_DEFINEMODELANIM  },
	{ "definemodelskin",  TokenType::T_DEFINEMODELSKIN  },
	{ "selectmodelskin",  TokenType::T_SELECTMODELSKIN  },
	{ "definevoxel",      TokenType::T_DEFINEVOXEL      },
	{ "definevoxeltiles", TokenType::T_DEFINEVOXELTILES },

	// new style
	{ "model",             TokenType::T_MODEL             },
	{ "voxel",             TokenType::T_VOXEL             },
	{ "skybox",            TokenType::T_SKYBOX            },
	{ "tint",              TokenType::T_TINT              },
	{ "texture",           TokenType::T_TEXTURE           },
	{ "tile",              TokenType::T_TEXTURE           },
	{ "undefmodel",        TokenType::T_UNDEFMODEL        },
	{ "undefmodelrange",   TokenType::T_UNDEFMODELRANGE   },
	{ "undefmodelof",      TokenType::T_UNDEFMODELOF      },
	{ "undeftexture",      TokenType::T_UNDEFTEXTURE      },
	{ "undeftexturerange", TokenType::T_UNDEFTEXTURERANGE },

	// EDuke32 extensions
	{ "tilefromtexture",   TokenType::T_TILEFROMTEXTURE   },
	{ "setuptilerange",    TokenType::T_SETUPTILERANGE    },
});

constexpr auto modeltokens = std::to_array<tokenlist>({
	{ "scale",  TokenType::T_SCALE  },
	{ "shade",  TokenType::T_SHADE  },
	{ "zadd",   TokenType::T_ZADD   },
	{ "frame",  TokenType::T_FRAME  },
	{ "anim",   TokenType::T_ANIM   },
	{ "skin",   TokenType::T_SKIN   },
	{ "hud",    TokenType::T_HUD    },
});

constexpr auto modelframetokens = std::to_array<tokenlist>({
	{ "frame",  TokenType::T_FRAME   },
	{ "name",   TokenType::T_FRAME   },
	{ "tile",   TokenType::T_TILE   },
	{ "tile0",  TokenType::T_TILE0  },
	{ "tile1",  TokenType::T_TILE1  },
});

constexpr auto modelanimtokens = std::to_array<tokenlist>({
	{ "frame0", TokenType::T_FRAME0 },
	{ "frame1", TokenType::T_FRAME1 },
	{ "fps",    TokenType::T_FPS    },
	{ "flags",  TokenType::T_FLAGS  },
});

constexpr auto modelskintokens = std::to_array<tokenlist>({
	{ "pal",     TokenType::T_PAL    },
	{ "file",    TokenType::T_FILE   },
	{ "surf",    TokenType::T_SURF   },
	{ "surface", TokenType::T_SURF   },
});

constexpr auto modelhudtokens = std::to_array<tokenlist>({
	{ "tile",    TokenType::T_TILE   },
	{ "tile0",   TokenType::T_TILE0  },
	{ "tile1",   TokenType::T_TILE1  },
	{ "xadd",    TokenType::T_XADD   },
	{ "yadd",    TokenType::T_YADD   },
	{ "zadd",    TokenType::T_ZADD   },
	{ "angadd",  TokenType::T_ANGADD },
	{ "hide",    TokenType::T_HIDE   },
	{ "nobob",   TokenType::T_NOBOB  },
	{ "flipped", TokenType::T_FLIPPED},
	{ "nodepth", TokenType::T_NODEPTH},
});

constexpr auto voxeltokens = std::to_array<tokenlist>({
	{ "tile",   TokenType::T_TILE   },
	{ "tile0",  TokenType::T_TILE0  },
	{ "tile1",  TokenType::T_TILE1  },
	{ "scale",  TokenType::T_SCALE  },
});

constexpr auto skyboxtokens = std::to_array<tokenlist>({
	{ "tile"   , TokenType::T_TILE   },
	{ "pal"    , TokenType::T_PAL    },
	{ "ft"     , TokenType::T_FRONT  },{ "front"  , TokenType::T_FRONT  },{ "forward", TokenType::T_FRONT  },
	{ "rt"     , TokenType::T_RIGHT  },{ "right"  , TokenType::T_RIGHT  },
	{ "bk"     , TokenType::T_BACK   },{ "back"   , TokenType::T_BACK   },
	{ "lf"     , TokenType::T_LEFT   },{ "left"   , TokenType::T_LEFT   },{ "lt"     , TokenType::T_LEFT   },
	{ "up"     , TokenType::T_TOP    },{ "top"    , TokenType::T_TOP    },{ "ceiling", TokenType::T_TOP    },{ "ceil"   , TokenType::T_TOP    },
	{ "dn"     , TokenType::T_BOTTOM },{ "bottom" , TokenType::T_BOTTOM },{ "floor"  , TokenType::T_BOTTOM },{ "down"   , TokenType::T_BOTTOM }
});

constexpr auto tinttokens = std::to_array<tokenlist>({
	{ "pal",   TokenType::T_PAL },
	{ "red",   TokenType::T_RED   },{ "r", TokenType::T_RED },
	{ "green", TokenType::T_GREEN },{ "g", TokenType::T_GREEN },
	{ "blue",  TokenType::T_BLUE  },{ "b", TokenType::T_BLUE },
	{ "flags", TokenType::T_FLAGS }
});

constexpr auto texturetokens = std::to_array<tokenlist>({
	{ "pal",   TokenType::T_PAL  },
	{ "glow",  TokenType::T_GLOW },    // EDuke32 extension
});

constexpr auto texturetokens_pal = std::to_array<tokenlist>({
	{ "file",       TokenType::T_FILE },
	{ "name",       TokenType::T_FILE },
	{ "alphacut",   TokenType::T_ALPHACUT },
	{ "nocompress", TokenType::T_NOCOMPRESS },
});


TokenType getatoken(scriptfile* sf, std::span<const tokenlist> tl)
{
	if (!sf) {
		return TokenType::T_ERROR;
	}

	const auto tok = scriptfile_gettoken(sf);

	if (!tok.has_value()) {
		return TokenType::T_EOF;
	}

	for(const auto& token : tl) {
		if (tok.value() == token.text)
			return token.tokenid;
	}

	return TokenType::T_ERROR;
}

int lastmodelid{-1};
int lastvoxid{-1};
int modelskin{-1};
int lastmodelskin{-1};
bool seenframe{false};

constexpr auto skyfaces = std::to_array<std::string_view>({
	"front face", "right face", "back face",
	"left face", "top face", "bottom face"
});

int defsparser(scriptfile* script)
{
	while (1) {
		const auto tokn = getatoken(script, basetokens);
		char* cmdtokptr = script->ltextptr;
		switch (tokn) {
			case TokenType::T_ERROR:
				buildprintf("Error on line {}:{}.\n", script->filename, scriptfile_getlinum(script, cmdtokptr));
				break;
			case TokenType::T_EOF:
				return(0);
			case TokenType::T_INCLUDE:
				{
					auto fn = scriptfile_getstring(script);
					if (fn.has_value()) {
						std::string fnvstr{fn.value()};
						auto included = scriptfile_fromfile(fnvstr);
						if (!included) {
							buildprintf("Warning: Failed including {} on line {}:{}\n",
									fnvstr, script->filename,scriptfile_getlinum(script,cmdtokptr));
						} else {
							defsparser(included.get());
						}
					}
					break;
				}
			case TokenType::T_ECHO:
				{
				    auto str = scriptfile_getstring(script);
				    if (!str.has_value()) break;
				    buildputs(str.value());
				    buildputs("\n");
				}
				break;
			case TokenType::T_DEFINE:
				{
					auto name = scriptfile_getstring(script);
					if (!name.has_value()) break;
					auto number = scriptfile_getsymbol(script);
					if (!number.has_value())
						break;

					if (scriptfile_addsymbolvalue(name.value().data(), number.value()) < 0)
						buildprintf("Warning: Symbol {} was NOT redefined to {} on line {}:{}\n",
								name.value(), number.value(), script->filename,scriptfile_getlinum(script,cmdtokptr));
				}
				break;
			case TokenType::T_DEFINESKYBOX:
				{
					std::array<std::string, 6> fn;
					auto tile = scriptfile_getsymbol(script);
					if (!tile.has_value())
						break;
					auto pal = scriptfile_getsymbol(script);
					if (!pal.has_value())
						break;
					const auto i = scriptfile_getsymbol(script);
					if (!i.has_value())
						break; //future expansion
					int idx{0};
					for (; idx < 6; ++idx) { //grab the 6 faces
						auto fno = scriptfile_getstring(script);
						if (!fno.has_value())
							break;
						fn[idx] = fno.value();
					}
					if (idx < 6)
						break;
#if USE_POLYMOST && USE_OPENGL
					hicsetskybox(tile.value(), pal.value(), fn);
#endif
				}
				break;
			case TokenType::T_DEFINETINT:
				{
					auto pal = scriptfile_getsymbol(script);
					if (!pal.has_value())
						break;
					auto r = scriptfile_getnumber(script);
					if (!r.has_value())
						break;
					auto g = scriptfile_getnumber(script);
					if (!g.has_value())
						break;
					auto b = scriptfile_getnumber(script);
					if (!b.has_value())
						break;
					auto f = scriptfile_getnumber(script); // effects
					if (!f.has_value())
						break;
#if USE_POLYMOST && USE_OPENGL
					hicsetpalettetint(pal.value(), r.value(), g.value(), b.value(), f.value());
#endif
				}
				break;
			case TokenType::T_DEFINEMODEL:
				{
					auto modelfn = scriptfile_getstring(script);
					if(!modelfn.has_value())
						break;
					auto scale = scriptfile_getdouble(script);
					if(!scale.has_value())
						break;
					auto shadeoffs = scriptfile_getnumber(script);
					if (!shadeoffs.has_value())
						break;
#if USE_POLYMOST && USE_OPENGL
					lastmodelid = md_loadmodel(modelfn.value().data());
					if (lastmodelid < 0) {
						buildprintf("Failure loading MD2/MD3 model \"{}\"\n", modelfn.value());
						break;
					}
					md_setmisc(lastmodelid, static_cast<float>(scale.value()), shadeoffs.value(), 0.0); // FIXME: Narrowing.
#endif
					modelskin = 0;
					lastmodelskin = 0;
					seenframe = false;
				}
				break;
			case TokenType::T_DEFINEMODELFRAME:
				{
					auto framename = scriptfile_getstring(script);
					if(!framename.has_value())
						break;
					auto ftilenume = scriptfile_getnumber(script); // first tile number
					if (!ftilenume.has_value())
						break;
					auto ltilenume = scriptfile_getnumber(script); // last tile number (inclusive)
					if (!ltilenume.has_value())
						break;

					if (ltilenume.value() < ftilenume.value()) {
						buildprintf("Warning: backwards tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						std::swap(ftilenume, ltilenume);
					}

					if (lastmodelid < 0) {
						buildputs("Warning: Ignoring frame definition.\n");
						break;
					}
#if USE_POLYMOST && USE_OPENGL
					bool happy{true};
					int tilex{0};
					for (tilex = ftilenume.value(); tilex <= ltilenume.value() && happy; tilex++) {
						switch (md_defineframe(lastmodelid, framename.value(), tilex, std::max(0, modelskin))) {
							case 0: break;
							case -1: happy = false; break; // invalid model id!?
							case -2: buildprintf("Invalid tile number on line {}:{}\n",
										 script->filename, scriptfile_getlinum(script,cmdtokptr));
								 happy = false;
								 break;
							case -3: buildprintf("Invalid frame name on line {}:{}\n",
										 script->filename, scriptfile_getlinum(script,cmdtokptr));
								 happy = false;
								 break;
						}
					}
#endif
					seenframe = true;
				}
				break;
			case TokenType::T_DEFINEMODELANIM:
				{
					auto startframe = scriptfile_getstring(script);
					if(!startframe.has_value())
						break;
					auto endframe = scriptfile_getstring(script);
					if(!endframe.has_value())
						break;
					auto dfps = scriptfile_getdouble(script);  //animation frame rate
					if(!dfps.has_value())
						break;
					auto flags = scriptfile_getnumber(script);
					if (!flags.has_value())
						break;

					if (lastmodelid < 0) {
						buildputs("Warning: Ignoring animation definition.\n");
						break;
					}
#if USE_POLYMOST && USE_OPENGL
					switch (md_defineanimation(lastmodelid, startframe.value(), endframe.value(), (int)(dfps.value() * (65536.0*.001)), flags.value())) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: buildprintf("Invalid starting frame name on line {}:{}\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -3: buildprintf("Invalid ending frame name on line {}:{}\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -4: buildprintf("Out of memory on line {}:{}\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
					}
#endif
				}
				break;
			case TokenType::T_DEFINEMODELSKIN:
				{
					auto palnum = scriptfile_getsymbol(script);
					if(!palnum.has_value())
						break;
					auto skinfn = scriptfile_getstring(script); //skin filename
					if (!skinfn.has_value())
						break; 

					// if we see a sequence of definemodelskin, then a sequence of definemodelframe,
					// and then a definemodelskin, we need to increment the skin counter.
					//
					// definemodel "mymodel.md2" 1 1
					// definemodelskin 0 "normal.png"   // skin 0
					// definemodelskin 21 "normal21.png"
					// definemodelframe "foo" 1000 1002   // these use skin 0
					// definemodelskin 0 "wounded.png"   // skin 1
					// definemodelskin 21 "wounded21.png"
					// definemodelframe "foo2" 1003 1004   // these use skin 1
					// selectmodelskin 0         // resets to skin 0
					// definemodelframe "foo3" 1005 1006   // these use skin 0
					if (seenframe) { modelskin = ++lastmodelskin; }
					seenframe = false;

#if USE_POLYMOST && USE_OPENGL
					switch (md_defineskin(lastmodelid, skinfn.value().data(), palnum.value(), std::max(0, modelskin), 0)) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: buildprintf("Invalid skin filename on line {}:{}\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -3: buildprintf("Invalid palette number on line {}:{}\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -4: buildprintf("Out of memory on line {}:{}\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
					}
#endif
				}
				break;
			case TokenType::T_SELECTMODELSKIN:
				{
					auto possiblemodelskin = scriptfile_getsymbol(script);
					if (!possiblemodelskin.has_value())
						break;
					modelskin = possiblemodelskin.value();
				}
				break;
			case TokenType::T_DEFINEVOXEL:
				{
					auto fn = scriptfile_getstring(script); //voxel filename

					if (!fn.has_value())
						break;

					if (nextvoxid == MAXVOXELS) {
						buildputs("Maximum number of voxels already defined.\n");
						break;
					}

					std::string fnstrv{fn.value()}; // FIXME: Remove copy.
					if (qloadkvx(nextvoxid, fnstrv)) {
						buildprintf("Failure loading voxel file \"{}\"\n", fnstrv);
						break;
					}

					lastvoxid = nextvoxid++;
				}
				break;
			case TokenType::T_DEFINEVOXELTILES:
				{
					auto ftilenume = scriptfile_getnumber(script); // 1st tile #
					if (!ftilenume.has_value())
						break;

					auto ltilenume = scriptfile_getnumber(script); //last tile #
					if (!ltilenume.has_value())
						break;

					if (ltilenume.value() < ftilenume.value()) {
						buildprintf("Warning: backwards tile range on line {}:{}\n",
								script->filename, scriptfile_getlinum(script,cmdtokptr));
						std::swap(ftilenume, ltilenume);
					}
					if (ltilenume.value() < 0 || ftilenume.value() >= MAXTILES) {
						buildprintf("Invalid tile range on line {}:{}\n",
								script->filename, scriptfile_getlinum(script,cmdtokptr));
						break;
					}

					if (lastvoxid < 0) {
						buildputs("Warning: Ignoring voxel tiles definition.\n");
						break;
					}

					int tilex;
					for (tilex = ftilenume.value(); tilex <= ltilenume.value(); tilex++) {
						tiletovox[tilex] = lastvoxid;
					}
				}
				break;

				// NEW (ENCOURAGED) DEFINITION SYNTAX
			case TokenType::T_MODEL:
				{
					char* modelend;
					double scale{1.0};
					double mzadd{0.0};
					int shadeoffs{0};

					modelskin = 0;
					lastmodelskin = 0;
					seenframe = false;

					auto modelfn = scriptfile_getstring(script);
					if (!modelfn.has_value()) break;

#if USE_POLYMOST && USE_OPENGL
					lastmodelid = md_loadmodel(modelfn.value().data());
					if (lastmodelid < 0) {
						buildprintf("Failure loading MD2/MD3 model \"{}\"\n", modelfn.value());
						break;
					}
#endif
					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script, modeltokens)) {
							//case TokenType::T_ERROR: buildprintf("Error on line {}:{} in model tokens\n", script->filename,script->linenum); break;
							case TokenType::T_SCALE:
								scale = scriptfile_getdouble(script).value_or(1.0);
								break;
							case TokenType::T_SHADE:
								shadeoffs = scriptfile_getnumber(script).value_or(0);
								break;
							case TokenType::T_ZADD:
								mzadd = scriptfile_getdouble(script).value_or(0.0);
								break;
							case TokenType::T_FRAME:
							{
								char *frametokptr = script->ltextptr;
								char *frameend;
								std::string framename;
								bool happy{true};
								std::optional<int> ftilenume{};
								std::optional<int> ltilenume{};
								int tilex = 0;

								if (scriptfile_getbraces(script,&frameend)) break;
								while (script->textptr < frameend) {
									switch(getatoken(script, modelframetokens)) {
										case TokenType::T_FRAME: framename = scriptfile_getstring(script).value_or(""); break; // FIXME: Default name?
										case TokenType::T_TILE:  ftilenume = scriptfile_getsymbol(script); ltilenume = ftilenume; break;
										case TokenType::T_TILE0: ftilenume = scriptfile_getsymbol(script); break; //first tile number
										case TokenType::T_TILE1: ltilenume = scriptfile_getsymbol(script); break; //last tile number (inclusive)
									}
								}

								if (!ftilenume.has_value()) {
									buildprintf("Error: missing 'first tile number' for frame definition near line {}:{}\n", script->filename, scriptfile_getlinum(script,frametokptr));
									happy = false;
								}

								if (!ltilenume.has_value()) {
									buildprintf("Error: missing 'last tile number' for frame definition near line {}:{}\n", script->filename, scriptfile_getlinum(script,frametokptr));
									happy = false;
								}

								if (!happy)
									break;

								if (ltilenume < ftilenume) {
									buildprintf("Warning: backwards tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,frametokptr));
									std::swap(ltilenume, ftilenume);
								}

								if (lastmodelid < 0) {
									buildputs("Warning: Ignoring frame definition.\n");
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								for (tilex = ftilenume.value(); tilex <= ltilenume && happy; ++tilex) {
									switch (md_defineframe(lastmodelid, framename.c_str(), tilex, std::max(0, modelskin))) {
										case 0: break;
										case -1: happy = false; break; // invalid model id!?
										case -2: buildprintf("Invalid tile number on line {}:{}\n",
													 script->filename, scriptfile_getlinum(script,frametokptr));
											 happy = false;
											 break;
										case -3: buildprintf("Invalid frame name on line {}:{}\n",
													 script->filename, scriptfile_getlinum(script,frametokptr));
											 happy = false;
											 break;
									}
								}
#endif
								seenframe = true;
								}
								break;
							case TokenType::T_ANIM:
							{
								char *animtokptr = script->ltextptr;
								char *animend;
								std::string startframe;
								std::string endframe;
								bool happy{true};
								int flags{0};
								double dfps{1.0};

								if (scriptfile_getbraces(script,&animend)) break;
								while (script->textptr < animend) {
									switch(getatoken(script, modelanimtokens)) {
										case TokenType::T_FRAME0: startframe = scriptfile_getstring(script).value_or(""); break; // FIXME: Default name?
										case TokenType::T_FRAME1: endframe = scriptfile_getstring(script).value_or(""); break; // FIXME: Default name?
										case TokenType::T_FPS:
											dfps = scriptfile_getdouble(script).value_or(1.0);
											break; //animation frame rate
										case TokenType::T_FLAGS: flags = scriptfile_getsymbol(script).value_or(0); break;
									}
								}

								if (startframe.empty()) {
									buildprintf("Error: missing 'start frame' for anim definition near line {}:{}\n", script->filename, scriptfile_getlinum(script, animtokptr));
									happy = false;
								}
								if (endframe.empty()) {
									buildprintf("Error: missing 'end frame' for anim definition near line {}:{}\n", script->filename, scriptfile_getlinum(script, animtokptr));
									happy = false;
								}
								if (!happy) break;

								if (lastmodelid < 0) {
									buildputs("Warning: Ignoring animation definition.\n");
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								switch (md_defineanimation(lastmodelid, startframe.c_str(), endframe.c_str(), (int)(dfps*(65536.0*.001)), flags)) {
									case 0: break;
									case -1: break; // invalid model id!?
									case -2: buildprintf("Invalid starting frame name on line {}:{}\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
									case -3: buildprintf("Invalid ending frame name on line {}:{}\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
									case -4: buildprintf("Out of memory on line {}:{}\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
								}
#endif
							} break;
							case TokenType::T_SKIN:
							{
								char *skintokptr = script->ltextptr;
								char *skinend;
								std::string skinfn;
								int palnum{0};
								int surfnum{0};

								if (scriptfile_getbraces(script, &skinend)) break;
								while (script->textptr < skinend) {
									switch(getatoken(script, modelskintokens)) {
										case TokenType::T_PAL: palnum   = scriptfile_getsymbol(script).value_or(0); break;
										case TokenType::T_FILE: skinfn  = scriptfile_getstring(script).value_or(""); break; //skin filename // FIXME: Default skin name?
										case TokenType::T_SURF: surfnum = scriptfile_getnumber(script).value_or(0); break;
									}
								}

								if (skinfn.empty()) {
										buildprintf("Error: missing 'skin filename' for skin definition near line {}:{}\n", script->filename, scriptfile_getlinum(script,skintokptr));
										break;
								}

								if (seenframe) {
									modelskin = ++lastmodelskin;
								}
								
								seenframe = false;

#if USE_POLYMOST && USE_OPENGL
								switch (md_defineskin(lastmodelid, skinfn.c_str(), palnum, std::max(0, modelskin), surfnum)) {
									case 0: break;
									case -1: break; // invalid model id!?
									case -2: buildprintf("Invalid skin filename on line {}:{}\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
									case -3: buildprintf("Invalid palette number on line {}:{}\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
									case -4: buildprintf("Out of memory on line {}:{}\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
								}
#endif
							} break;
							case TokenType::T_HUD:
							{
								char *hudtokptr = script->ltextptr;
								char happy{true};
								char *frameend;
								std::optional<int> ftilenume;
								std::optional<int> ltilenume;
								int tilex = 0;
								int flags = 0;
								double xadd{0.0};
								double yadd{0.0};
								double zadd{0.0};
								double angadd{0.0};

								if (scriptfile_getbraces(script,&frameend)) break;
								while (script->textptr < frameend) {
									switch(getatoken(script, modelhudtokens)) {
										case TokenType::T_TILE:  ftilenume = scriptfile_getsymbol(script); ltilenume = ftilenume; break;
										case TokenType::T_TILE0: ftilenume = scriptfile_getsymbol(script); break; //first tile number
										case TokenType::T_TILE1: ltilenume = scriptfile_getsymbol(script); break; //last tile number (inclusive)
										case TokenType::T_XADD:  xadd = scriptfile_getdouble(script).value_or(0.0); break;
										case TokenType::T_YADD:  yadd = scriptfile_getdouble(script).value_or(0.0); break;
										case TokenType::T_ZADD:  zadd = scriptfile_getdouble(script).value_or(0.0); break;
										case TokenType::T_ANGADD: angadd = scriptfile_getdouble(script).value_or(0.0); break;
										case TokenType::T_HIDE:    flags |= 1; break;
										case TokenType::T_NOBOB:   flags |= 2; break;
										case TokenType::T_FLIPPED: flags |= 4; break;
										case TokenType::T_NODEPTH: flags |= 8; break;
									}
								}

								if (!ftilenume.has_value()) {
									buildprintf("Error: missing 'first tile number' for hud definition near line {}:{}\n", script->filename, scriptfile_getlinum(script,hudtokptr));
									happy = false;
								}
								if (!ltilenume.has_value()) {
									buildprintf("Error: missing 'last tile number' for hud definition near line {}:{}\n", script->filename, scriptfile_getlinum(script,hudtokptr));
									happy = false;
								}
								if (!happy) break;

								if (ltilenume < ftilenume) {
									buildprintf("Warning: backwards tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,hudtokptr));
									std::swap(ftilenume, ltilenume);
								}

								if (lastmodelid < 0) {
									buildputs("Warning: Ignoring frame definition.\n");
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								for (tilex = ftilenume.value(); tilex <= ltilenume && happy; tilex++) {
									switch (md_definehud(lastmodelid, tilex, xadd, yadd, zadd, angadd, flags)) {
										case 0: break;
										case -1:
											happy = false;
											break; // invalid model id!?
										case -2: buildprintf("Invalid tile number on line {}:{}\n",
												             script->filename, scriptfile_getlinum(script, hudtokptr));
											happy = false;
											break;
										case -3: buildprintf("Invalid frame name on line {}:{}\n",
												script->filename, scriptfile_getlinum(script,hudtokptr));
											happy = false;
											break;
									}
								}
#endif
							} break;
						}
					}

#if USE_POLYMOST && USE_OPENGL
					md_setmisc(lastmodelid, (float) scale, shadeoffs, (float)mzadd);
#endif

					modelskin = lastmodelskin = 0;
					seenframe = false;

				}
				break;
			case TokenType::T_VOXEL:
				{
					char *voxeltokptr = script->ltextptr;
					char *modelend;
					std::optional<int> tile0;
					std::optional<int> tile1;
					std::optional<int> tilex;
					auto fn = scriptfile_getstring(script); // voxel filename
					if (!fn.has_value())
						break;
					
					if (nextvoxid == MAXVOXELS) {
						buildputs("Maximum number of voxels already defined.\n");
						break;
					}

					std::string fnvstr{fn.value()};
					if (qloadkvx(nextvoxid, fnvstr)) {
						buildprintf("Failure loading voxel file \"{}\"\n", fn.value());
						break;
					}

					lastvoxid = nextvoxid++;

					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script, voxeltokens)) {
							//case TokenType::T_ERROR: buildprintf("Error on line {}:{} in voxel tokens\n", script->filename,linenum); break;
							case TokenType::T_TILE:
								tilex = scriptfile_getsymbol(script);
								if (tilex.has_value() && (unsigned int)tilex.value() < MAXTILES)
									tiletovox[tilex.value()] = lastvoxid;
								else
									buildprintf("Invalid tile number on line {}:{}\n",script->filename, scriptfile_getlinum(script,voxeltokptr));
								break;
							case TokenType::T_TILE0: // TODO: Why default to MAXTILES here?
								tile0 = scriptfile_getsymbol(script).value_or(MAXTILES);
									break; //1st tile #
							case TokenType::T_TILE1:
								tile1 = scriptfile_getsymbol(script);
								if (tile0 > tile1)
								{
									buildprintf("Warning: backwards tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,voxeltokptr));
									std::swap(tile0, tile1);
								}
								if (!tile1.has_value() || !tile0.has_value() || (tile0 >= MAXTILES))
									{ buildprintf("Invalid tile range on line {}:{}\n",script->filename, scriptfile_getlinum(script,voxeltokptr)); break; }
								for(auto tmptile = tile0; tmptile <= tile1; ++tmptile.value()) {
									tiletovox[tmptile.value()] = lastvoxid;
								}
								break; //last tile number (inclusive)
							case TokenType::T_SCALE: {
								double scale = scriptfile_getdouble(script).value_or(1.0);
								voxscale[lastvoxid] = 65536 * scale;
								break;
							}
						}
					}
					lastvoxid = -1;
				}
				break;
			case TokenType::T_SKYBOX:
				{
					char *skyboxtokptr = script->ltextptr;
					std::array<std::string, 6> fn{};
					char *modelend;
					bool happy{true};
					int i;
					std::optional<int> tile;
					int pal{0};

					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script, skyboxtokens)) {
							//case TokenType::T_ERROR: buildprintf("Error on line {}:{} in skybox tokens\n",script->filename,linenum); break;
							case TokenType::T_TILE:  tile  = scriptfile_getsymbol(script); break;
							case TokenType::T_PAL:   pal   = scriptfile_getsymbol(script).value_or(0); break;
							case TokenType::T_FRONT: fn[0] = scriptfile_getstring(script).value_or(""); break; // FIXME: Default names?
							case TokenType::T_RIGHT: fn[1] = scriptfile_getstring(script).value_or(""); break;
							case TokenType::T_BACK:  fn[2] = scriptfile_getstring(script).value_or(""); break;
							case TokenType::T_LEFT:  fn[3] = scriptfile_getstring(script).value_or(""); break;
							case TokenType::T_TOP:   fn[4] = scriptfile_getstring(script).value_or(""); break;
							case TokenType::T_BOTTOM:fn[5] = scriptfile_getstring(script).value_or(""); break;
						}
					}

					if (!tile.has_value()) {
						buildprintf("Error: missing 'tile number' for skybox definition near line {}:{}\n", script->filename, scriptfile_getlinum(script,skyboxtokptr));
						happy = false;
					}
					for (const auto& name : fn) {
						if (name.empty()) {
							buildprintf("Error: missing '{} filename' for skybox definition near line {}:{}\n", skyfaces[i], script->filename, scriptfile_getlinum(script,skyboxtokptr));
							happy = false;
						}
					}

					if (!happy) break;

#if USE_POLYMOST && USE_OPENGL
					hicsetskybox(tile.value(), pal, fn);
#endif
				}
				break;
			case TokenType::T_TINT:
				{
					char *tinttokptr = script->ltextptr;
					int red{255};
					int green{255};
					int blue{255};
					std::optional<int> pal;
					int flags{0};
					char *tintend;

					if (scriptfile_getbraces(script,&tintend)) break;
					while (script->textptr < tintend) {
						switch (getatoken(script, tinttokens)) {
							case TokenType::T_PAL:   pal   = scriptfile_getsymbol(script);   break;
							case TokenType::T_RED:   red   = scriptfile_getnumber(script).value_or(255);   red   = std::min(255, std::max(0, red));   break;
							case TokenType::T_GREEN: green = scriptfile_getnumber(script).value_or(255);   green = std::min(255, std::max(0, green)); break;
							case TokenType::T_BLUE:  blue  = scriptfile_getnumber(script).value_or(255);   blue  = std::min(255, std::max(0, blue));  break;
							case TokenType::T_FLAGS: flags = scriptfile_getsymbol(script).value_or(0); break;
						}
					}

					if (!pal.has_value()) {
							buildprintf("Error: missing 'palette number' for tint definition near line {}:{}\n", script->filename, scriptfile_getlinum(script,tinttokptr));
							break;
					}

#if USE_POLYMOST && USE_OPENGL
					hicsetpalettetint(pal.value(), red, green, blue, flags);
#endif
				}
				break;
			case TokenType::T_TEXTURE:
				{
					char *texturetokptr = script->ltextptr;
					char *textureend;
					auto tile = scriptfile_getsymbol(script);
					if (!tile.has_value())
						break;
					if (scriptfile_getbraces(script,&textureend)) break;
					while (script->textptr < textureend) {
						switch (getatoken(script, texturetokens)) {
							case TokenType::T_PAL: {
								char *paltokptr = script->ltextptr;
								char *palend;
								std::string fn;
								double alphacut{-1.0};
								char flags = 0;
								auto pal = scriptfile_getsymbol(script);
								if (!pal.has_value())
									break;
								if (scriptfile_getbraces(script,&palend)) break;
								while (script->textptr < palend) {
									switch (getatoken(script, texturetokens_pal)) {
										case TokenType::T_FILE:     fn = scriptfile_getstring(script).value_or(""); break; // FIXME: Default value?
										case TokenType::T_ALPHACUT: alphacut = scriptfile_getdouble(script).value_or(-1.0); break;
										case TokenType::T_NOCOMPRESS: flags |= 1; break;
										default: break;
									}
								}

								if ((unsigned)tile.value() > (unsigned)MAXTILES)
									break;	// message is printed later

								if ((unsigned)pal.value() > (unsigned)MAXPALOOKUPS) {
									buildprintf("Error: missing or invalid 'palette number' for texture definition near "
												"line {}:{}\n", script->filename, scriptfile_getlinum(script,paltokptr));
									break;
								}

								if (fn.empty()) {
									buildprintf("Error: missing 'file name' for texture definition near line {}:{}\n",
												script->filename, scriptfile_getlinum(script,paltokptr));
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								hicsetsubsttex(tile.value(), pal.value(), fn, alphacut, flags);
#endif
							} break;

							// an EDuke32 extension which we quietly parse over
							case TokenType::T_GLOW: {
							    char *glowend;
							    if (scriptfile_getbraces(script, &glowend)) break;
							    script->textptr = glowend+1;
							} break;

							default: break;
						}
					}

					if ((unsigned)tile.value() >= (unsigned)MAXTILES) {
						buildprintf("Error: missing or invalid 'tile number' for texture definition near line {}:{}\n",
									script->filename, scriptfile_getlinum(script,texturetokptr));
						break;
					}
				}
				break;

			// an EDuke32 extension which we quietly parse over
			case TokenType::T_TILEFROMTEXTURE:
				{
				    char *textureend;
				    if (!scriptfile_getsymbol(script).has_value()) // tile
						break;
				    if (scriptfile_getbraces(script,&textureend)) break;
				    script->textptr = textureend+1;
				}
				break;
			case TokenType::T_SETUPTILERANGE:
				{
					for(int i{0}; i < 6; ++i) {
						if(!scriptfile_getsymbol(script).has_value()) // tile
							break;
					}
				}
				break;

			case TokenType::T_UNDEFMODEL:
			case TokenType::T_UNDEFMODELRANGE:
				{
					auto r0 = scriptfile_getsymbol(script);
					if (!r0.has_value())
						break;
					
					std::optional<int> r1{};
					if (tokn == TokenType::T_UNDEFMODELRANGE) {
						r1 = scriptfile_getsymbol(script);
						if (!r1.has_value())
							break;
						if (r1 < r0) {
							std::swap(r0, r1);
							buildprintf("Warning: backwards tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						}
						if (r0 < 0 || r1 >= MAXTILES) {
							buildprintf("Error: invalid tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					} else {
						r1 = r0;
						if ((unsigned)r0.value() >= (unsigned)MAXTILES) {
							buildprintf("Error: invalid tile number on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					}
#if USE_POLYMOST && USE_OPENGL
					for (; r0 <= r1; ++r0.value()) {
						md_undefinetile(r0.value());
					}
#endif
				}
				break;

			case TokenType::T_UNDEFMODELOF:
				{
					auto r0 = scriptfile_getsymbol(script);
					if (!r0.has_value()) {
						break;
					}

					if ((unsigned)r0.value() >= (unsigned)MAXTILES) {
						buildprintf("Error: invalid tile number on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						break;
					}

#if USE_POLYMOST && USE_OPENGL
					const int mid = md_tilehasmodel(r0.value());
					if (mid < 0)
						break;

					md_undefinemodel(mid);
#endif
				}
				break;

			case TokenType::T_UNDEFTEXTURE:
			case TokenType::T_UNDEFTEXTURERANGE:
				{
					std::optional<int> r1;
					auto r0 = scriptfile_getsymbol(script);
					if (!r0.has_value()) break;
					if (tokn == TokenType::T_UNDEFTEXTURERANGE) {
						r1 = scriptfile_getsymbol(script);
						if (!r1.has_value())
							break;
						if (r1 < r0) {
							std::swap(r0, r1);
							buildprintf("Warning: backwards tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						}
						if (r0 < 0 || r1 >= MAXTILES) {
							buildprintf("Error: invalid tile range on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					} else {
						r1 = r0;
						if ((unsigned)r0.value() >= (unsigned)MAXTILES) {
							buildprintf("Error: invalid tile number on line {}:{}\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					}

#if USE_POLYMOST && USE_OPENGL
					for (; r0 <= r1; ++r0.value())
						for (int i=MAXPALOOKUPS-1; i>=0; i--)
							hicclearsubst(r0.value(), i);
#endif
				}
				break;

			default:
				buildputs("Unknown token.\n"); break;
		}
	}
	return 0;
}

} // namespace

int loaddefinitionsfile(const char *fn)
{
	auto script = scriptfile_fromfile(fn);
	
	if (!script) {
		return -1;
	}

	defsparser(script.get());

	scriptfile_clearsymbols();

	return 0;
}

// vim:ts=4:
