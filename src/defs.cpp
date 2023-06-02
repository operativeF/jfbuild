/*
 * Definitions file parser for Build
 * by Jonathon Fowler (jf@jonof.id.au)
 * Remixed substantially by Ken Silverman
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "build.hpp"
#include "baselayer.hpp"
#include "scriptfile.hpp"

#include <array>
#include <span>
#include <string_view>

enum class TokenType {
	T_EOF,
	T_ERROR,
	T_INCLUDE,
	T_ECHO,
	T_DEFINE,
	T_DEFINETEXTURE,
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

static constexpr auto basetokens = std::to_array<tokenlist>({
	{ "include",         TokenType::T_INCLUDE          },
	{ "#include",        TokenType::T_INCLUDE          },
	{ "define",          TokenType::T_DEFINE           },
	{ "#define",         TokenType::T_DEFINE           },
	{ "echo",            TokenType::T_ECHO             },

	// deprecated style
	{ "definetexture",    TokenType::T_DEFINETEXTURE    },
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

static constexpr auto modeltokens = std::to_array<tokenlist>({
	{ "scale",  TokenType::T_SCALE  },
	{ "shade",  TokenType::T_SHADE  },
	{ "zadd",   TokenType::T_ZADD   },
	{ "frame",  TokenType::T_FRAME  },
	{ "anim",   TokenType::T_ANIM   },
	{ "skin",   TokenType::T_SKIN   },
	{ "hud",    TokenType::T_HUD    },
});

static constexpr auto modelframetokens = std::to_array<tokenlist>({
	{ "frame",  TokenType::T_FRAME   },
	{ "name",   TokenType::T_FRAME   },
	{ "tile",   TokenType::T_TILE   },
	{ "tile0",  TokenType::T_TILE0  },
	{ "tile1",  TokenType::T_TILE1  },
});

static constexpr auto modelanimtokens = std::to_array<tokenlist>({
	{ "frame0", TokenType::T_FRAME0 },
	{ "frame1", TokenType::T_FRAME1 },
	{ "fps",    TokenType::T_FPS    },
	{ "flags",  TokenType::T_FLAGS  },
});

static constexpr auto modelskintokens = std::to_array<tokenlist>({
	{ "pal",     TokenType::T_PAL    },
	{ "file",    TokenType::T_FILE   },
	{ "surf",    TokenType::T_SURF   },
	{ "surface", TokenType::T_SURF   },
});

static constexpr auto modelhudtokens = std::to_array<tokenlist>({
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

static constexpr auto voxeltokens = std::to_array<tokenlist>({
	{ "tile",   TokenType::T_TILE   },
	{ "tile0",  TokenType::T_TILE0  },
	{ "tile1",  TokenType::T_TILE1  },
	{ "scale",  TokenType::T_SCALE  },
});

static constexpr auto skyboxtokens = std::to_array<tokenlist>({
	{ "tile"   , TokenType::T_TILE   },
	{ "pal"    , TokenType::T_PAL    },
	{ "ft"     , TokenType::T_FRONT  },{ "front"  , TokenType::T_FRONT  },{ "forward", TokenType::T_FRONT  },
	{ "rt"     , TokenType::T_RIGHT  },{ "right"  , TokenType::T_RIGHT  },
	{ "bk"     , TokenType::T_BACK   },{ "back"   , TokenType::T_BACK   },
	{ "lf"     , TokenType::T_LEFT   },{ "left"   , TokenType::T_LEFT   },{ "lt"     , TokenType::T_LEFT   },
	{ "up"     , TokenType::T_TOP    },{ "top"    , TokenType::T_TOP    },{ "ceiling", TokenType::T_TOP    },{ "ceil"   , TokenType::T_TOP    },
	{ "dn"     , TokenType::T_BOTTOM },{ "bottom" , TokenType::T_BOTTOM },{ "floor"  , TokenType::T_BOTTOM },{ "down"   , TokenType::T_BOTTOM }
});

static constexpr auto tinttokens = std::to_array<tokenlist>({
	{ "pal",   TokenType::T_PAL },
	{ "red",   TokenType::T_RED   },{ "r", TokenType::T_RED },
	{ "green", TokenType::T_GREEN },{ "g", TokenType::T_GREEN },
	{ "blue",  TokenType::T_BLUE  },{ "b", TokenType::T_BLUE },
	{ "flags", TokenType::T_FLAGS }
});

static constexpr auto texturetokens = std::to_array<tokenlist>({
	{ "pal",   TokenType::T_PAL  },
	{ "glow",  TokenType::T_GLOW },    // EDuke32 extension
});

static constexpr auto texturetokens_pal = std::to_array<tokenlist>({
	{ "file",       TokenType::T_FILE },
	{ "name",       TokenType::T_FILE },
	{ "alphacut",   TokenType::T_ALPHACUT },
	{ "nocompress", TokenType::T_NOCOMPRESS },
});


static TokenType getatoken(scriptfile *sf, std::span<const tokenlist> tl)
{
	if (!sf) {
		return TokenType::T_ERROR;
	}

	const char* tok = scriptfile_gettoken(sf);

	if (!tok) {
		return TokenType::T_EOF;
	}

	for(const auto& token : tl) {
		if (!Bstrcasecmp(tok, &token.text[0]))
			return token.tokenid;
	}

	return TokenType::T_ERROR;
}

static int lastmodelid{-1};
static int lastvoxid{-1};
static int modelskin{-1};
static int lastmodelskin{-1};
static int seenframe{0};
extern int nextvoxid;

static constexpr auto skyfaces = std::to_array<std::string_view>({
	"front face", "right face", "back face",
	"left face", "top face", "bottom face"
});

static int defsparser(scriptfile *script)
{
	while (1) {
		const auto tokn = getatoken(script, basetokens);
		char* cmdtokptr = script->ltextptr;
		switch (tokn) {
			case TokenType::T_ERROR:
				buildprintf("Error on line %s:%d.\n", script->filename,scriptfile_getlinum(script,cmdtokptr));
				break;
			case TokenType::T_EOF:
				return(0);
			case TokenType::T_INCLUDE:
				{
					char *fn;
					if (!scriptfile_getstring(script,&fn)) {
						scriptfile *included;

						included = scriptfile_fromfile(fn);
						if (!included) {
							buildprintf("Warning: Failed including %s on line %s:%d\n",
									fn, script->filename,scriptfile_getlinum(script,cmdtokptr));
						} else {
							defsparser(included);
							scriptfile_close(included);
						}
					}
					break;
				}
			case TokenType::T_ECHO:
				{
				    char *str;
				    if (scriptfile_getstring(script, &str)) break;
				    buildputs(str);
				    buildputs("\n");
				}
				break;
			case TokenType::T_DEFINE:
				{
					char *name;
					int number;

					if (scriptfile_getstring(script,&name)) break;
					if (scriptfile_getsymbol(script,&number)) break;

					if (scriptfile_addsymbolvalue(name,number) < 0)
						buildprintf("Warning: Symbol %s was NOT redefined to %d on line %s:%d\n",
								name,number,script->filename,scriptfile_getlinum(script,cmdtokptr));
					break;
				}

				// OLD (DEPRECATED) DEFINITION SYNTAX
			case TokenType::T_DEFINETEXTURE:
				{
					int tile;
					int pal;
					int fnoo;
					char *fn;

					if (scriptfile_getsymbol(script, &tile)) break;
					if (scriptfile_getsymbol(script, &pal))  break;
					if (scriptfile_getnumber(script, &fnoo)) break; //x-center
					if (scriptfile_getnumber(script, &fnoo)) break; //y-center
					if (scriptfile_getnumber(script, &fnoo)) break; //x-size
					if (scriptfile_getnumber(script, &fnoo)) break; //y-size
					if (scriptfile_getstring(script, &fn))  break;
#if USE_POLYMOST && USE_OPENGL
					hicsetsubsttex(tile,pal,fn,-1.0,0);
#endif
				}
				break;
			case TokenType::T_DEFINESKYBOX:
				{
					int tile;
					int pal;
					int i;
					char *fn[6];

					if (scriptfile_getsymbol(script, &tile)) break;
					if (scriptfile_getsymbol(script, &pal)) break;
					if (scriptfile_getsymbol(script, &i)) break; //future expansion
					for (i=0;i<6;i++) if (scriptfile_getstring(script,&fn[i])) break; //grab the 6 faces
					if (i < 6) break;
#if USE_POLYMOST && USE_OPENGL
					hicsetskybox(tile,pal,fn);
#endif
				}
				break;
			case TokenType::T_DEFINETINT:
				{
					int pal, r,g,b,f;

					if (scriptfile_getsymbol(script,&pal)) break;
					if (scriptfile_getnumber(script,&r)) break;
					if (scriptfile_getnumber(script,&g)) break;
					if (scriptfile_getnumber(script,&b)) break;
					if (scriptfile_getnumber(script,&f)) break; //effects
#if USE_POLYMOST && USE_OPENGL
					hicsetpalettetint(pal,r,g,b,f);
#endif
				}
				break;
			case TokenType::T_DEFINEMODEL:
				{
					char *modelfn;
					double scale;
					int shadeoffs;

					if (scriptfile_getstring(script,&modelfn)) break;
					if (scriptfile_getdouble(script,&scale)) break;
					if (scriptfile_getnumber(script,&shadeoffs)) break;

#if USE_POLYMOST && USE_OPENGL
					lastmodelid = md_loadmodel(modelfn);
					if (lastmodelid < 0) {
						buildprintf("Failure loading MD2/MD3 model \"%s\"\n", modelfn);
						break;
					}
					md_setmisc(lastmodelid,(float)scale, shadeoffs,0.0);
#endif
					modelskin = 0;
					lastmodelskin = 0;
					seenframe = 0;
				}
				break;
			case TokenType::T_DEFINEMODELFRAME:
				{
					char *framename;
					int ftilenume, ltilenume, tilex;

					if (scriptfile_getstring(script,&framename)) break;
					if (scriptfile_getnumber(script,&ftilenume)) break; //first tile number
					if (scriptfile_getnumber(script,&ltilenume)) break; //last tile number (inclusive)
					if (ltilenume < ftilenume) {
						buildprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						tilex = ftilenume;
						ftilenume = ltilenume;
						ltilenume = tilex;
					}

					if (lastmodelid < 0) {
						buildputs("Warning: Ignoring frame definition.\n");
						break;
					}
#if USE_POLYMOST && USE_OPENGL
					char happy = 1;
					for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++) {
						switch (md_defineframe(lastmodelid, framename, tilex, max(0,modelskin))) {
							case 0: break;
							case -1: happy = 0; break; // invalid model id!?
							case -2: buildprintf("Invalid tile number on line %s:%d\n",
										 script->filename, scriptfile_getlinum(script,cmdtokptr));
								 happy = 0;
								 break;
							case -3: buildprintf("Invalid frame name on line %s:%d\n",
										 script->filename, scriptfile_getlinum(script,cmdtokptr));
								 happy = 0;
								 break;
						}
					}
#endif
					seenframe = 1;
				}
				break;
			case TokenType::T_DEFINEMODELANIM:
				{
					char* startframe;
					char* endframe;
					int flags;
					double dfps;

					if (scriptfile_getstring(script,&startframe)) break;
					if (scriptfile_getstring(script,&endframe)) break;
					if (scriptfile_getdouble(script,&dfps)) break; //animation frame rate
					if (scriptfile_getnumber(script,&flags)) break;

					if (lastmodelid < 0) {
						buildputs("Warning: Ignoring animation definition.\n");
						break;
					}
#if USE_POLYMOST && USE_OPENGL
					switch (md_defineanimation(lastmodelid, startframe, endframe, (int)(dfps*(65536.0*.001)), flags)) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: buildprintf("Invalid starting frame name on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -3: buildprintf("Invalid ending frame name on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -4: buildprintf("Out of memory on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
					}
#endif
				}
				break;
			case TokenType::T_DEFINEMODELSKIN:
				{
					int palnum;
					char *skinfn;

					if (scriptfile_getsymbol(script,&palnum)) break;
					if (scriptfile_getstring(script,&skinfn)) break; //skin filename

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
					seenframe = 0;

#if USE_POLYMOST && USE_OPENGL
					switch (md_defineskin(lastmodelid, skinfn, palnum, max(0,modelskin), 0)) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: buildprintf("Invalid skin filename on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -3: buildprintf("Invalid palette number on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -4: buildprintf("Out of memory on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
					}
#endif
				}
				break;
			case TokenType::T_SELECTMODELSKIN:
				{
					if (scriptfile_getsymbol(script,&modelskin)) break;
				}
				break;
			case TokenType::T_DEFINEVOXEL:
				{
					char *fn;

					if (scriptfile_getstring(script,&fn)) break; //voxel filename

					if (nextvoxid == MAXVOXELS) {
						buildputs("Maximum number of voxels already defined.\n");
						break;
					}

					if (qloadkvx(nextvoxid, fn)) {
						buildprintf("Failure loading voxel file \"%s\"\n",fn);
						break;
					}

					lastvoxid = nextvoxid++;
				}
				break;
			case TokenType::T_DEFINEVOXELTILES:
				{
					int ftilenume, ltilenume, tilex;

					if (scriptfile_getnumber(script,&ftilenume)) break; //1st tile #
					if (scriptfile_getnumber(script,&ltilenume)) break; //last tile #

					if (ltilenume < ftilenume) {
						buildprintf("Warning: backwards tile range on line %s:%d\n",
								script->filename, scriptfile_getlinum(script,cmdtokptr));
						tilex = ftilenume;
						ftilenume = ltilenume;
						ltilenume = tilex;
					}
					if (ltilenume < 0 || ftilenume >= MAXTILES) {
						buildprintf("Invalid tile range on line %s:%d\n",
								script->filename, scriptfile_getlinum(script,cmdtokptr));
						break;
					}

					if (lastvoxid < 0) {
						buildputs("Warning: Ignoring voxel tiles definition.\n");
						break;
					}

					for (tilex = ftilenume; tilex <= ltilenume; tilex++) {
						tiletovox[tilex] = lastvoxid;
					}
				}
				break;

				// NEW (ENCOURAGED) DEFINITION SYNTAX
			case TokenType::T_MODEL:
				{
					char* modelend;
					char* modelfn;
					double scale{1.0};
					double mzadd{0.0};
					int shadeoffs{0};

					modelskin = 0;
					lastmodelskin = 0;
					seenframe = 0;

					if (scriptfile_getstring(script, &modelfn)) break;

#if USE_POLYMOST && USE_OPENGL
					lastmodelid = md_loadmodel(modelfn);
					if (lastmodelid < 0) {
						buildprintf("Failure loading MD2/MD3 model \"%s\"\n", modelfn);
						break;
					}
#endif
					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script, modeltokens)) {
							//case TokenType::T_ERROR: buildprintf("Error on line %s:%d in model tokens\n", script->filename,script->linenum); break;
							case TokenType::T_SCALE: scriptfile_getdouble(script,&scale); break;
							case TokenType::T_SHADE: scriptfile_getnumber(script,&shadeoffs); break;
							case TokenType::T_ZADD:  scriptfile_getdouble(script,&mzadd); break;
							case TokenType::T_FRAME:
							{
								char *frametokptr = script->ltextptr;
								char *frameend, *framename = nullptr, happy=1;
								int ftilenume = -1, ltilenume = -1, tilex = 0;

								if (scriptfile_getbraces(script,&frameend)) break;
								while (script->textptr < frameend) {
									switch(getatoken(script, modelframetokens)) {
										case TokenType::T_FRAME: scriptfile_getstring(script,&framename); break;
										case TokenType::T_TILE:  scriptfile_getsymbol(script,&ftilenume); ltilenume = ftilenume; break;
										case TokenType::T_TILE0: scriptfile_getsymbol(script,&ftilenume); break; //first tile number
										case TokenType::T_TILE1: scriptfile_getsymbol(script,&ltilenume); break; //last tile number (inclusive)
									}
								}

								if (ftilenume < 0) {
									buildprintf("Error: missing 'first tile number' for frame definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,frametokptr));
									happy = 0;
								}
								if (ltilenume < 0) {
									buildprintf("Error: missing 'last tile number' for frame definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,frametokptr));
									happy = 0;
								}
								if (!happy) break;

								if (ltilenume < ftilenume) {
									buildprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,frametokptr));
									tilex = ftilenume;
									ftilenume = ltilenume;
									ltilenume = tilex;
								}

								if (lastmodelid < 0) {
									buildputs("Warning: Ignoring frame definition.\n");
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++) {
									switch (md_defineframe(lastmodelid, framename, tilex, max(0,modelskin))) {
										case 0: break;
										case -1: happy = 0; break; // invalid model id!?
										case -2: buildprintf("Invalid tile number on line %s:%d\n",
													 script->filename, scriptfile_getlinum(script,frametokptr));
											 happy = 0;
											 break;
										case -3: buildprintf("Invalid frame name on line %s:%d\n",
													 script->filename, scriptfile_getlinum(script,frametokptr));
											 happy = 0;
											 break;
									}
								}
#endif
								seenframe = 1;
								}
								break;
							case TokenType::T_ANIM:
							{
								char *animtokptr = script->ltextptr;
								char *animend, *startframe = nullptr, *endframe = nullptr, happy=1; // FIXME: char* == 1?
								int flags{0};
								double dfps{1.0};

								if (scriptfile_getbraces(script,&animend)) break;
								while (script->textptr < animend) {
									switch(getatoken(script, modelanimtokens)) {
										case TokenType::T_FRAME0: scriptfile_getstring(script,&startframe); break;
										case TokenType::T_FRAME1: scriptfile_getstring(script,&endframe); break;
										case TokenType::T_FPS: scriptfile_getdouble(script,&dfps); break; //animation frame rate
										case TokenType::T_FLAGS: scriptfile_getsymbol(script,&flags); break;
									}
								}

								if (!startframe) {
									buildprintf("Error: missing 'start frame' for anim definition near line %s:%d\n", script->filename, scriptfile_getlinum(script, animtokptr));
									happy = 0;
								}
								if (!endframe) {
									buildprintf("Error: missing 'end frame' for anim definition near line %s:%d\n", script->filename, scriptfile_getlinum(script, animtokptr));
									happy = 0;
								}
								if (!happy) break;

								if (lastmodelid < 0) {
									buildputs("Warning: Ignoring animation definition.\n");
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								switch (md_defineanimation(lastmodelid, startframe, endframe, (int)(dfps*(65536.0*.001)), flags)) {
									case 0: break;
									case -1: break; // invalid model id!?
									case -2: buildprintf("Invalid starting frame name on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
									case -3: buildprintf("Invalid ending frame name on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
									case -4: buildprintf("Out of memory on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
								}
#endif
							} break;
							case TokenType::T_SKIN:
							{
								char *skintokptr = script->ltextptr;
								char *skinend, *skinfn = nullptr;
								int palnum{0};
								int surfnum{0};

								if (scriptfile_getbraces(script,&skinend)) break;
								while (script->textptr < skinend) {
									switch(getatoken(script, modelskintokens)) {
										case TokenType::T_PAL: scriptfile_getsymbol(script,&palnum); break;
										case TokenType::T_FILE: scriptfile_getstring(script,&skinfn); break; //skin filename
										case TokenType::T_SURF: scriptfile_getnumber(script,&surfnum); break;
									}
								}

								if (!skinfn) {
										buildprintf("Error: missing 'skin filename' for skin definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,skintokptr));
										break;
								}

								if (seenframe) { modelskin = ++lastmodelskin; }
								seenframe = 0;

#if USE_POLYMOST && USE_OPENGL
								switch (md_defineskin(lastmodelid, skinfn, palnum, max(0,modelskin), surfnum)) {
									case 0: break;
									case -1: break; // invalid model id!?
									case -2: buildprintf("Invalid skin filename on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
									case -3: buildprintf("Invalid palette number on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
									case -4: buildprintf("Out of memory on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
								}
#endif
							} break;
							case TokenType::T_HUD:
							{
								char *hudtokptr = script->ltextptr;
								char happy=1, *frameend;
								int ftilenume = -1, ltilenume = -1, tilex = 0, flags = 0;
								double xadd = 0.0, yadd = 0.0, zadd = 0.0, angadd = 0.0;

								if (scriptfile_getbraces(script,&frameend)) break;
								while (script->textptr < frameend) {
									switch(getatoken(script, modelhudtokens)) {
										case TokenType::T_TILE:  scriptfile_getsymbol(script,&ftilenume); ltilenume = ftilenume; break;
										case TokenType::T_TILE0: scriptfile_getsymbol(script,&ftilenume); break; //first tile number
										case TokenType::T_TILE1: scriptfile_getsymbol(script,&ltilenume); break; //last tile number (inclusive)
										case TokenType::T_XADD:  scriptfile_getdouble(script,&xadd); break;
										case TokenType::T_YADD:  scriptfile_getdouble(script,&yadd); break;
										case TokenType::T_ZADD:  scriptfile_getdouble(script,&zadd); break;
										case TokenType::T_ANGADD:scriptfile_getdouble(script,&angadd); break;
										case TokenType::T_HIDE:    flags |= 1; break;
										case TokenType::T_NOBOB:   flags |= 2; break;
										case TokenType::T_FLIPPED: flags |= 4; break;
										case TokenType::T_NODEPTH: flags |= 8; break;
									}
								}

								if (ftilenume < 0) {
									buildprintf("Error: missing 'first tile number' for hud definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,hudtokptr));
									happy = 0;
								}
								if (ltilenume < 0) {
									buildprintf("Error: missing 'last tile number' for hud definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,hudtokptr));
									happy = 0;
								}
								if (!happy) break;

								if (ltilenume < ftilenume) {
									buildprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,hudtokptr));
									tilex = ftilenume;
									ftilenume = ltilenume;
									ltilenume = tilex;
								}

								if (lastmodelid < 0) {
									buildputs("Warning: Ignoring frame definition.\n");
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++) {
									switch (md_definehud(lastmodelid, tilex, xadd, yadd, zadd, angadd, flags)) {
										case 0: break;
										case -1: happy = 0; break; // invalid model id!?
										case -2: buildprintf("Invalid tile number on line %s:%d\n",
												script->filename, scriptfile_getlinum(script,hudtokptr));
											happy = 0;
											break;
										case -3: buildprintf("Invalid frame name on line %s:%d\n",
												script->filename, scriptfile_getlinum(script,hudtokptr));
											happy = 0;
											break;
									}
								}
#endif
							} break;
						}
					}

#if USE_POLYMOST && USE_OPENGL
					md_setmisc(lastmodelid,(float)scale,shadeoffs,(float)mzadd);
#endif

					modelskin = lastmodelskin = 0;
					seenframe = 0;

				}
				break;
			case TokenType::T_VOXEL:
				{
					char *voxeltokptr = script->ltextptr;
					char *fn, *modelend;
					int tile0 = MAXTILES, tile1 = -1, tilex = -1;

					if (scriptfile_getstring(script,&fn)) break; //voxel filename
					if (nextvoxid == MAXVOXELS) { buildputs("Maximum number of voxels already defined.\n"); break; }
					if (qloadkvx(nextvoxid, fn)) { buildprintf("Failure loading voxel file \"%s\"\n",fn); break; }
					lastvoxid = nextvoxid++;

					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script, voxeltokens)) {
							//case TokenType::T_ERROR: buildprintf("Error on line %s:%d in voxel tokens\n", script->filename,linenum); break;
							case TokenType::T_TILE:
								scriptfile_getsymbol(script,&tilex);
								if ((unsigned int)tilex < MAXTILES) tiletovox[tilex] = lastvoxid;
								else buildprintf("Invalid tile number on line %s:%d\n",script->filename, scriptfile_getlinum(script,voxeltokptr));
								break;
							case TokenType::T_TILE0:
								scriptfile_getsymbol(script,&tile0); break; //1st tile #
							case TokenType::T_TILE1:
								scriptfile_getsymbol(script,&tile1);
								if (tile0 > tile1)
								{
									buildprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,voxeltokptr));
									tilex = tile0; tile0 = tile1; tile1 = tilex;
								}
								if ((tile1 < 0) || (tile0 >= MAXTILES))
									{ buildprintf("Invalid tile range on line %s:%d\n",script->filename, scriptfile_getlinum(script,voxeltokptr)); break; }
								for(tilex=tile0;tilex<=tile1;tilex++) tiletovox[tilex] = lastvoxid;
								break; //last tile number (inclusive)
							case TokenType::T_SCALE: {
								double scale=1.0;
								scriptfile_getdouble(script,&scale);
								voxscale[lastvoxid] = 65536*scale;
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
					char *fn[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}, *modelend, happy=1;
					int i, tile = -1, pal = 0;

					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script, skyboxtokens)) {
							//case TokenType::T_ERROR: buildprintf("Error on line %s:%d in skybox tokens\n",script->filename,linenum); break;
							case TokenType::T_TILE:  scriptfile_getsymbol(script,&tile ); break;
							case TokenType::T_PAL:   scriptfile_getsymbol(script,&pal  ); break;
							case TokenType::T_FRONT: scriptfile_getstring(script,&fn[0]); break;
							case TokenType::T_RIGHT: scriptfile_getstring(script,&fn[1]); break;
							case TokenType::T_BACK:  scriptfile_getstring(script,&fn[2]); break;
							case TokenType::T_LEFT:  scriptfile_getstring(script,&fn[3]); break;
							case TokenType::T_TOP:   scriptfile_getstring(script,&fn[4]); break;
							case TokenType::T_BOTTOM:scriptfile_getstring(script,&fn[5]); break;
						}
					}

					if (tile < 0) {
						buildprintf("Error: missing 'tile number' for skybox definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,skyboxtokptr));
						happy=0;
					}
					for (i=0;i<6;i++) {
						if (!fn[i]) {
							buildprintf("Error: missing '%s filename' for skybox definition near line %s:%d\n", skyfaces[i], script->filename, scriptfile_getlinum(script,skyboxtokptr));
							happy = 0;
						}
					}

					if (!happy) break;

#if USE_POLYMOST && USE_OPENGL
					hicsetskybox(tile,pal,fn);
#endif
				}
				break;
			case TokenType::T_TINT:
				{
					char *tinttokptr = script->ltextptr;
					int red=255, green=255, blue=255, pal=-1, flags=0;
					char *tintend;

					if (scriptfile_getbraces(script,&tintend)) break;
					while (script->textptr < tintend) {
						switch (getatoken(script, tinttokens)) {
							case TokenType::T_PAL:   scriptfile_getsymbol(script,&pal);   break;
							case TokenType::T_RED:   scriptfile_getnumber(script,&red);   red   = min(255,max(0,red));   break;
							case TokenType::T_GREEN: scriptfile_getnumber(script,&green); green = min(255,max(0,green)); break;
							case TokenType::T_BLUE:  scriptfile_getnumber(script,&blue);  blue  = min(255,max(0,blue));  break;
							case TokenType::T_FLAGS: scriptfile_getsymbol(script,&flags); break;
						}
					}

					if (pal < 0) {
							buildprintf("Error: missing 'palette number' for tint definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,tinttokptr));
							break;
					}

#if USE_POLYMOST && USE_OPENGL
					hicsetpalettetint(pal,red,green,blue,flags);
#endif
				}
				break;
			case TokenType::T_TEXTURE:
				{
					char *texturetokptr = script->ltextptr, *textureend;
					int tile=-1;

					if (scriptfile_getsymbol(script,&tile)) break;
					if (scriptfile_getbraces(script,&textureend)) break;
					while (script->textptr < textureend) {
						switch (getatoken(script, texturetokens)) {
							case TokenType::T_PAL: {
								char *paltokptr = script->ltextptr, *palend;
								int pal=-1;
								char *fn = nullptr;
								double alphacut = -1.0;
								char flags = 0;

								if (scriptfile_getsymbol(script,&pal)) break;
								if (scriptfile_getbraces(script,&palend)) break;
								while (script->textptr < palend) {
									switch (getatoken(script, texturetokens_pal)) {
										case TokenType::T_FILE:     scriptfile_getstring(script,&fn); break;
										case TokenType::T_ALPHACUT: scriptfile_getdouble(script,&alphacut); break;
										case TokenType::T_NOCOMPRESS: flags |= 1; break;
										default: break;
									}
								}

								if ((unsigned)tile > (unsigned)MAXTILES) break;	// message is printed later
								if ((unsigned)pal > (unsigned)MAXPALOOKUPS) {
									buildprintf("Error: missing or invalid 'palette number' for texture definition near "
												"line %s:%d\n", script->filename, scriptfile_getlinum(script,paltokptr));
									break;
								}
								if (!fn) {
									buildprintf("Error: missing 'file name' for texture definition near line %s:%d\n",
												script->filename, scriptfile_getlinum(script,paltokptr));
									break;
								}
#if USE_POLYMOST && USE_OPENGL
								hicsetsubsttex(tile,pal,fn,alphacut,flags);
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

					if ((unsigned)tile >= (unsigned)MAXTILES) {
						buildprintf("Error: missing or invalid 'tile number' for texture definition near line %s:%d\n",
									script->filename, scriptfile_getlinum(script,texturetokptr));
						break;
					}
				}
				break;

			// an EDuke32 extension which we quietly parse over
			case TokenType::T_TILEFROMTEXTURE:
				{
				    char *textureend;
				    int tile=-1;
				    if (scriptfile_getsymbol(script,&tile)) break;
				    if (scriptfile_getbraces(script,&textureend)) break;
				    script->textptr = textureend+1;
				}
				break;
			case TokenType::T_SETUPTILERANGE:
				{
				    int t;
				    if (scriptfile_getsymbol(script,&t)) break;
				    if (scriptfile_getsymbol(script,&t)) break;
				    if (scriptfile_getsymbol(script,&t)) break;
				    if (scriptfile_getsymbol(script,&t)) break;
				    if (scriptfile_getsymbol(script,&t)) break;
				    if (scriptfile_getsymbol(script,&t)) break;
				}
				break;

			case TokenType::T_UNDEFMODEL:
			case TokenType::T_UNDEFMODELRANGE:
				{
					int r0,r1;

					if (scriptfile_getsymbol(script,&r0)) break;
					if (tokn == TokenType::T_UNDEFMODELRANGE) {
						if (scriptfile_getsymbol(script,&r1)) break;
						if (r1 < r0) {
							const int t{ r1 };
							r1 = r0;
							r0 = t;
							buildprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						}
						if (r0 < 0 || r1 >= MAXTILES) {
							buildprintf("Error: invalid tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					} else {
						r1 = r0;
						if ((unsigned)r0 >= (unsigned)MAXTILES) {
							buildprintf("Error: invalid tile number on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					}
#if USE_POLYMOST && USE_OPENGL
					for (; r0 <= r1; r0++) md_undefinetile(r0);
#endif
				}
				break;

			case TokenType::T_UNDEFMODELOF:
				{
					int r0;

					if (scriptfile_getsymbol(script, &r0)) {
						break;
					}

					if ((unsigned)r0 >= (unsigned)MAXTILES) {
						buildprintf("Error: invalid tile number on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						break;
					}

#if USE_POLYMOST && USE_OPENGL
					const int mid = md_tilehasmodel(r0);
					if (mid < 0) break;

					md_undefinemodel(mid);
#endif
				}
				break;

			case TokenType::T_UNDEFTEXTURE:
			case TokenType::T_UNDEFTEXTURERANGE:
				{
					int r0,r1;

					if (scriptfile_getsymbol(script,&r0)) break;
					if (tokn == TokenType::T_UNDEFTEXTURERANGE) {
						if (scriptfile_getsymbol(script, &r1)) break;
						if (r1 < r0) {
							const int t{ r1 };
							r1 = r0;
							r0 = t;
							buildprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						}
						if (r0 < 0 || r1 >= MAXTILES) {
							buildprintf("Error: invalid tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					} else {
						r1 = r0;
						if ((unsigned)r0 >= (unsigned)MAXTILES) {
							buildprintf("Error: invalid tile number on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					}

#if USE_POLYMOST && USE_OPENGL
					for (; r0 <= r1; r0++)
						for (int i=MAXPALOOKUPS-1; i>=0; i--)
							hicclearsubst(r0,i);
#endif
				}
				break;

			default:
				buildputs("Unknown token.\n"); break;
		}
	}
	return 0;
}


int loaddefinitionsfile(const char *fn)
{
	scriptfile* script = scriptfile_fromfile(fn);
	
	if (!script) {
		return -1;
	}

	defsparser(script);

	scriptfile_close(script);
	scriptfile_clearsymbols();

	return 0;
}

// vim:ts=4:
