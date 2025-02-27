/**************************************************************************************************
"POLYMOST" code written by Ken Silverman
Ken Silverman's official web site: http://www.advsys.net/ken

Motivation:
When 3D Realms released the Duke Nukem 3D source code, I thought somebody would do a OpenGL or
Direct3D port. Well, after a few months passed, I saw no sign of somebody working on a true
hardware-accelerated port of Build, just people saying it wasn't possible. Eventually, I realized
the only way this was going to happen was for me to do it myself. First, I needed to port Build to
Windows. I could have done it myself, but instead I thought I'd ask my Australian buddy, Jonathon
Fowler, if he would upgrade his Windows port to my favorite compiler (MSVC) - which he did. Once
that was done, I was ready to start the "POLYMOST" project.

About:
This source file is basically a complete rewrite of the entire rendering part of the Build engine.
There are small pieces in ENGINE.C to activate this code, and other minor hacks in other source
files, but most of it is in here. If you're looking for polymost-related code in the other source
files, you should find most of them by searching for either "polymost" or "rendmode". Speaking of
rendmode, there are now 4 rendering modes in Build:

	rendmode 0: The original code I wrote from 1993-1997
	rendmode 1: Solid-color rendering: my debug code before I did texture mapping
	rendmode 2: Software rendering before I started the OpenGL code (Note: this is just a quick
						hack to make testing easier - it's not optimized to my usual standards!)
	rendmode 3: The OpenGL code

The original Build engine did hidden surface removal by using a vertical span buffer on the tops
and bottoms of walls. This worked nice back in the day, but it it's not suitable for a polygon
engine. So I decided to write a brand new hidden surface removal algorithm - using the same idea
as the original Build - but one that worked with vectors instead of already rasterized data.

Brief history:
06/20/2000: I release Build Source code
04/01/2003: 3D Realms releases Duke Nukem 3D source code
10/04/2003: Jonathon Fowler gets his Windows port working in Visual C
10/04/2003: I start writing POLYMOST.BAS, a new hidden surface removal algorithm for Build that
					works on a polygon level instead of spans.
10/16/2003: Ported POLYMOST.BAS to C inside JonoF KenBuild's ENGINE.C; later this code was split
					out of ENGINE.C and put in this file, POLYMOST.C.
12/10/2003: Started OpenGL code for POLYMOST (rendmode 3)
12/23/2003: 1st public release
01/01/2004: 2nd public release: fixed stray lines, status bar, mirrors, sky, and lots of other bugs.

----------------------------------------------------------------------------------------------------

Todo list (in approximate chronological order):

High priority:
	*   BOTH: Do accurate software sorting/chopping for sprites: drawing in wrong order is bad :/
	*   BOTH: Fix hall of mirrors near "zenith". Call polymost_drawrooms twice?
	* OPENGL: drawmapview()

Low priority:
	* SOFT6D: Do back-face culling of sprites during up/down/tilt transformation (top of drawpoly)
	* SOFT6D: Fix depth shading: use saturation&LUT
	* SOFT6D: Optimize using hyperbolic mapping (similar to KUBE algo)
	* SOFT6D: Slab6-style voxel sprites. How to accelerate? :/
	* OPENGL: KENBUILD: Write flipping code for floor mirrors
	*   BOTH: KENBUILD: Parallaxing sky modes 1&2
	*   BOTH: Masked/1-way walls don't clip correctly to sectors of intersecting ceiling/floor slopes
	*   BOTH: Editart x-center is not working correctly with Duke's camera/turret sprites
	*   BOTH: Get rid of horizontal line above Duke full-screen status bar
	*   BOTH: Combine ceilings/floors into a single triangle strip (should lower poly count by 2x)
	*   BOTH: Optimize/clean up texture-map setup equations

**************************************************************************************************/

#include "build.hpp"

#if USE_POLYMOST

#include "glbuild.hpp"
#include "pragmas.hpp"
#include "baselayer.hpp"
#include "baselayer_priv.hpp"
#include "osd.hpp"
#include "engine_priv.hpp"
#include "polymost_priv.hpp"
#include "polymost_fs_vs_aux.hpp"
#include "string_utils.hpp"

#if USE_OPENGL
# include "glbuild_fs_vs.hpp"
# include "hightile_priv.hpp"
# include "polymosttex_priv.hpp"
# include "polymosttexcache.hpp"
# include "mdsprite_priv.hpp"
#endif

#include <charconv>
#include <cmath>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

int usegoodalpha{0};

struct vsptyp {
	float x;
	std::array<float, 2> cy;
	std::array<float, 2> fy;
	int n;
	int p;
	int tag;
	int ctag;
	int ftag;
};

double gxyaspect;
double grhalfxdown10x;
double gcosang;
double gsinang;
double gcosang2;
double gsinang2;
double gchang;
double gshang;
double gctang;
double gstang;
float gtang{0.0F};

namespace {

constexpr auto VSPMAX{4096}; //<- careful!
std::array<vsptyp, VSPMAX> vsp;
int vcnt;
int gtag;

constexpr auto SCISDIST{1.0}; //1.0: Close plane clipping distance
#define USEZBUFFER 1 //1:use zbuffer (slow, nice sprite rendering), 0:no zbuffer (fast, bad sprite rendering)
constexpr auto LINTERPSIZ{4}; //log2 of interpolation size. 4:pretty fast&acceptable quality, 0:best quality/slow!
#define DEPTHDEBUG 0 //1:render distance instead of texture, for debugging only!, 0:default
constexpr auto FOGSCALE{0.0000384};

double gyxscale;
double gviewxrange;
double ghalfx;
double grhalfxdown10;
double ghoriz;
double gvisibility;

} // namespace

//Screen-based texture mapping parameters
double guo;
double gux;
double guy;
double gvo;
double gvx;
double gvy;
double gdo;
double gdx;
double gdy;

#if (USEZBUFFER != 0)
intptr_t zbufmem{0};
int zbufysiz{0};
int zbufbpl{0};
int* zbufoff{nullptr};
#endif

#if USE_OPENGL
int gfogpalnum{0};
float gfogdensity{0.F};
int glpolygonmode{0};     // 0:GL_FILL,1:GL_LINE,2:GL_POINT,3:clear+GL_FILL

GLfloat gdrawroomsprojmat[4][4];      // Proj. matrix for drawrooms() calls.
GLfloat grotatespriteprojmat[4][4];   // Proj. matrix for rotatesprite() calls.
GLfloat gorthoprojmat[4][4];          // Proj. matrix for 2D (aux) calls.

namespace {

int lastglredbluemode{0};
int redblueclearcnt{0};

int lastglpolygonmode{0};

GLuint texttexture{0};
GLuint nulltexture{0};

#define SHADERDEV 1
struct {
	GLuint vao;					// Vertex array object.
	GLuint program;             // GLSL program object.
	GLuint elementbuffer;
	GLint attrib_vertex;		// Vertex (vec3)
	GLint attrib_texcoord;		// Texture coordinate (vec2)
	GLint uniform_modelview;	// Modelview matrix (mat4)
	GLint uniform_projection;	// Projection matrix (mat4)
	GLint uniform_texture;      // Base texture (sampler2D)
	GLint uniform_glowtexture;  // Glow texture (sampler2D)
	GLint uniform_alphacut;     // Alpha test cutoff (float)
	GLint uniform_colour;		// Colour (vec4)
	GLint uniform_fogcolour;    // Fog colour   (vec4)
	GLint uniform_fogdensity;   // Fog density  (float)
} polymostglsl;

struct {
	GLuint vao;					// Vertex array object.
	GLuint program;
	GLuint elementbuffer;
	GLuint indexbuffer;
	GLint attrib_vertex;		// Vertex (vec3)
	GLint attrib_texcoord;		// Texture coordinate (vec2)
	GLint uniform_projection;	// Projection matrix (mat4)
	GLint uniform_texture;      // Character bitmap (sampler2D)
	GLint uniform_colour;		// Colour (vec4)
	GLint uniform_bgcolour;     // Background colour (vec4)
	GLint uniform_mode;	        // Operation mode (int)
		// 0 = texture is mask, render vertex colour/bgcolour.
		// 1 = texture is image, blend with bgcolour.
		// 2 = draw solid colour.
} polymostauxglsl;

GLuint elementindexbuffer{0};
GLuint elementindexbuffersize{0};

int polymost_preparetext();

} // namespace 

#endif //USE_OPENGL

namespace {

#ifdef DEBUGGINGAIDS
int polymostshowcallcounts = 0;
struct polymostcallcounts polymostcallcounts;
#endif

#if defined(_MSC_VER) && defined(_M_IX86) && USE_ASM
inline void dtol (double d, int *a)
{
	_asm
	{
		mov eax, a
		fld d
		fistp dword ptr [eax]
	}
}
#elif defined(__WATCOMC__) && USE_ASM

#pragma aux dtol =\
	"fistp dword ptr [eax]",\
	parm [eax 8087]

#elif defined(__GNUC__) && defined(__i386__) && USE_ASM

inline void dtol (double d, int *a)
{
	__asm__ __volatile__ (
#if 0 //(__GNUC__ >= 3)
			"fldl %1; fistpl %0;"
#else
			"fldl %1; fistpl (%0);"
#endif
			: "=r" (a) : "m" (d) : "memory","cc");
}

#else

inline void dtol(double d, int *a)
{
	*a = static_cast<int>(d);
}
#endif

inline int imod(int a, int b)
{
	if (a >= 0) {
		return a % b;
	}

	return ((a + 1) % b) + b - 1;
}

void drawline2d (float x0, float y0, float x1, float y1, unsigned char col)
{
	float f;
	int e;
	int inc;
	int x;
	int y;
	unsigned int up16;

	const float dx = x1 - x0;
	const float dy = y1 - y0;

	if ((dx == 0) && (dy == 0)) {
		return;
	}

	const auto fxres = static_cast<float>(xdimen);
	const auto fyres = static_cast<float>(ydimen);

	if (x0 >= fxres) { if (x1 >= fxres) return; y0 += (fxres-x0)*dy/dx; x0 = fxres; }
	else if (x0 <      0) { if (x1 <      0) return; y0 += (    0-x0)*dy/dx; x0 =     0; }
		  if (x1 >= fxres) {                          y1 += (fxres-x1)*dy/dx; x1 = fxres; }
	else if (x1 <      0) {                          y1 += (    0-x1)*dy/dx; x1 =     0; }
		  if (y0 >= fyres) { if (y1 >= fyres) return; x0 += (fyres-y0)*dx/dy; y0 = fyres; }
	else if (y0 <      0) { if (y1 <      0) return; x0 += (    0-y0)*dx/dy; y0 =     0; }
		  if (y1 >= fyres) {                          x1 += (fyres-y1)*dx/dy; y1 = fyres; }
	else if (y1 <      0) {                          x1 += (    0-y1)*dx/dy; y1 =     0; }

	if (std::fabs(dx) > std::fabs(dy))
	{
		if (x0 > x1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}

		y = (int)(y0 * 65536.F) + 32768;
		inc = (int)(dy/dx*65536.F + .5F);
		x = (int)(x0+.5); if (x < 0) { y -= inc*x; x = 0; } //if for safety
		e = (int)(x1+.5); if (e > xdimen) e = xdimen;       //if for safety
		up16 = (ydimen<<16);
		for(;x<e;x++,y+=inc) if ((unsigned int)y < up16) *(unsigned char *)(ylookup[y>>16]+x+frameoffset) = col;
	}
	else
	{
		if (y0 > y1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}

		x = (int)(x0 * 65536.F) + 32768;
		inc = (int)(dx / dy * 65536.F + .5F);
		y = (int)(y0+.5); if (y < 0) { x -= inc*y; y = 0; } //if for safety
		e = (int)(y1+.5); if (e > ydimen) e = ydimen;       //if for safety
		up16 = (xdimen<<16);
		
		for(; y < e; ++y, x += inc)
			if ((unsigned int)x < up16)
				*(unsigned char *)(ylookup[y] + (x >> 16) + frameoffset) = col;
	}
}

} // namespace

#if USE_OPENGL

namespace {

int drawingskybox{0};

} // namespace

bool polymost_texmayhavealpha (int dapicnum, int dapalnum)
{
	const PTHead* pth = PT_GetHead(dapicnum, dapalnum, 0, 1);

	if (!pth) {
		return true;
	}

	if (!pth->pic[PTHPIC_BASE]) {
		// we haven't got a PTMHead reference yet for the base layer, so we
		// don't know if the texture actually does have alpha, so err on
		// the side of caution
		return true;
	}

	return (pth->pic[PTHPIC_BASE]->flags & PTH_HASALPHA) == PTH_HASALPHA;
}

void polymost_texinvalidate (int dapicnum, int dapalnum, int dameth)
{
	PTHead * pth; // FIXME: Move to while loop?

	PTIter iter = PTIterNewMatch(
		PTITER_PICNUM | PTITER_PALNUM | PTITER_FLAGS,
		dapicnum, dapalnum, PTH_CLAMPED, (dameth & METH_CLAMPED) ? PTH_CLAMPED : 0
		);
	while ((pth = PTIterNext(iter)) != nullptr) {
		if (pth->pic[PTHPIC_BASE]) {
			pth->pic[PTHPIC_BASE]->flags |= PTH_DIRTY;
		}
		/*buildprintf("invalidating picnum:{} palnum:{} effects:{} skyface:{} flags:{04X} repldef:{}\n",
			   pth->picnum, pth->palnum, pth->effects, pth->skyface,
			   pth->flags, pth->repldef);*/
	}
	//buildprintf("done\n");
	PTIterFree(iter);
}

	//Make all textures "dirty" so they reload, but not re-allocate
	//This should be much faster than polymost_glreset()
	//Use this for palette effects ... but not ones that change every frame!
void polymost_texinvalidateall()
{
	PTHead * pth; // FIXME: Move to while loop?

	PTIter iter = PTIterNew();
	while ((pth = PTIterNext(iter)) != nullptr) {
		if (pth->pic[PTHPIC_BASE]) {
			pth->pic[PTHPIC_BASE]->flags |= PTH_DIRTY;
		}
	}
	PTIterFree(iter);
	clearskins();
	//buildprintf("gltexinvalidateall()\n");
}


void gltexapplyprops ()
{
	if (glinfo.maxanisotropy > 1.0)
	{
		if (glanisotropy <= 0 || glanisotropy > glinfo.maxanisotropy) {
			glanisotropy = (int)glinfo.maxanisotropy;
		}
	}

	if (gltexfiltermode < 0) {
		gltexfiltermode = 0;
	}
	else if (gltexfiltermode >= numglfiltermodes) {
		gltexfiltermode = numglfiltermodes - 1;
	}

	PTIter iter = PTIterNew();
	PTHead * pth; // FIXME: Move to while loop?
	while ((pth = PTIterNext(iter)) != nullptr) {
		for (const auto& aPic : pth->pic) {
			if (aPic == nullptr || aPic->glpic == 0) {
				continue;
			}
			glfunc.glBindTexture(GL_TEXTURE_2D, aPic->glpic);
			glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, glfiltermodes[gltexfiltermode].mag);
			glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, glfiltermodes[gltexfiltermode].min);
#ifdef GL_EXT_texture_filter_anisotropic
			if (glinfo.maxanisotropy > 1.0) {
				glfunc.glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,glanisotropy);
			}
#endif
		}
	}

	PTIterFree(iter);

	{
		for(int i{0}; i < nextmodelid; i++) {
			auto* m = (md2model *)models[i]; // FIXME: C-style cast requires conversion

			if (m->mdnum < 2) {
				continue;
			}

			for (int j{0};j < m->numskins * (HICEFFECTMASK + 1); ++j)
			{
				if (!m->tex[j] || !m->tex[j]->glpic) continue;
				glfunc.glBindTexture(GL_TEXTURE_2D,m->tex[j]->glpic);
				glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,glfiltermodes[gltexfiltermode].mag);
				glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glfiltermodes[gltexfiltermode].min);
#ifdef GL_EXT_texture_filter_anisotropic
				if (glinfo.maxanisotropy > 1.0)
					glfunc.glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,glanisotropy);
#endif
			}

			for (const auto& sk : m->skinmap)
				for (const auto& aTex : sk.tex)
				{
					if (!aTex || !aTex->glpic) continue;
					glfunc.glBindTexture(GL_TEXTURE_2D, aTex->glpic);
					glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,glfiltermodes[gltexfiltermode].mag);
					glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glfiltermodes[gltexfiltermode].min);
#ifdef GL_EXT_texture_filter_anisotropic
					if (glinfo.maxanisotropy > 1.0)
						glfunc.glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,glanisotropy);
#endif
				}
			}
		}
}

//--------------------------------------------------------------------------------------------------

float glox1;
float gloy1;
float glox2;
float gloy2;

namespace {

	//Use this for both initialization and uninitialization of OpenGL.
int gltexcacnum{-1};

} // namespace

void polymost_glreset ()
{
	//Reset if this is -1 (meaning 1st texture call ever), or > 0 (textures in memory)
	if (gltexcacnum < 0)
	{
		gltexcacnum = 0;

			//Hack for polymost_dorotatesprite calls before 1st polymost_drawrooms()
		gcosang = gcosang2 = ((double)16384)/262144.0;
		gsinang = gsinang2 = ((double)    0)/262144.0;
	}
	else
	{
		PTReset();
		clearskins();
	}

	glox1 = -1;
	lastglpolygonmode = -1;
	lastglredbluemode = -1;

	if (glfunc.glUseProgram) {
		glfunc.glUseProgram(0);
#if (USE_OPENGL == USE_GL3)
		glfunc.glBindVertexArray(0);
#endif
		glfunc.glBindBuffer(GL_ARRAY_BUFFER, 0);
		glfunc.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	if (elementindexbuffer) {
		glfunc.glDeleteBuffers(1, &elementindexbuffer);
		elementindexbuffersize = 0;
		elementindexbuffer = 0;
	}

#if (USE_OPENGL == USE_GL3)
	if (polymostglsl.vao) {
		glfunc.glDeleteVertexArrays(1, &polymostglsl.vao);
		polymostglsl.vao = 0;
	}
#endif
	if (polymostglsl.elementbuffer) {
		glfunc.glDeleteBuffers(1, &polymostglsl.elementbuffer);
		polymostglsl.elementbuffer = 0;
	}
	if (polymostglsl.program) {
		glfunc.glDeleteProgram(polymostglsl.program);
		polymostglsl.program = 0;
	}

#if (USE_OPENGL == USE_GL3)
	if (polymostauxglsl.vao) {
		glfunc.glDeleteVertexArrays(1, &polymostauxglsl.vao);
		polymostauxglsl.vao = 0;
	}
#endif
	if (polymostauxglsl.indexbuffer) {
		glfunc.glDeleteBuffers(1, &polymostauxglsl.indexbuffer);
		polymostauxglsl.indexbuffer = 0;
	}
	if (polymostauxglsl.elementbuffer) {
		glfunc.glDeleteBuffers(1, &polymostauxglsl.elementbuffer);
		polymostauxglsl.elementbuffer = 0;
	}
	if (polymostauxglsl.program) {
		glfunc.glDeleteProgram(polymostauxglsl.program);
		polymostauxglsl.program = 0;
	}

	if (texttexture) {
		glfunc.glDeleteTextures(1, &texttexture);
		texttexture = 0;
	}

	if (nulltexture) {
		glfunc.glDeleteTextures(1, &nulltexture);
		nulltexture = 0;
	}
}

namespace {

GLint polymost_get_attrib(GLuint program, const GLchar *name)
{
	const GLint attribloc = glfunc.glGetAttribLocation(program, name);
	
	if (attribloc < 0) {
		buildprintf("polymost_get_attrib: could not get location of attribute \"{}\"\n", name);
	}

	return attribloc;
}

GLint polymost_get_uniform(GLuint program, const GLchar *name)
{
	const GLint uniformloc = glfunc.glGetUniformLocation(program, name);
	
	if (uniformloc < 0) {
		buildprintf("polymost_get_uniform: could not get location of uniform \"{}\"\n", name);
	}

	return uniformloc;
}

GLuint polymost_load_shader(GLuint shadertype, const std::string& defaultsrc, const std::string& filename)
{
	std::string shadersrc{defaultsrc};

#ifdef SHADERDEV
	std::FILE* shaderfh = std::fopen(filename.c_str(), "rb");
	
	if (shaderfh) {
		std::fseek(shaderfh, 0, SEEK_END);
		auto shadersrclen = ftell(shaderfh);
		std::fseek(shaderfh, 0, SEEK_SET);

		std::string fileshadersrc(shadersrclen + 1, 0);
		shadersrclen = std::fread(&fileshadersrc[0], 1, shadersrclen, shaderfh);
		fileshadersrc[shadersrclen] = 0;

		std::fclose(shaderfh);
		shaderfh = nullptr;

		buildprintf("polymost_load_shader: loaded {} ({} bytes)\n", filename, shadersrclen);
		shadersrc = fileshadersrc;
	}
#endif

	return glbuild_compile_shader(shadertype, &shadersrc[0]);
}

void checkindexbuffer(unsigned int size)
{
	if (size <= elementindexbuffersize)
		return;

	std::vector<GLushort> indexes(size);

	std::iota(indexes.begin(), indexes.end(), 0);

	glfunc.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementindexbuffer);
	glfunc.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort)*size, &indexes[0], GL_STATIC_DRAW);

	elementindexbuffersize = size;
}

void polymost_loadshaders()
{
	// General texture rendering shader.
	if (polymostglsl.program) {
		glfunc.glDeleteProgram(polymostglsl.program);
		polymostglsl.program = 0;
	}

	std::array<GLuint, 2> shader = {
		polymost_load_shader(GL_VERTEX_SHADER, default_polymost_vs_glsl, "polymost_vs.glsl"),
		polymost_load_shader(GL_FRAGMENT_SHADER, default_polymost_fs_glsl, "polymost_fs.glsl")
	};
	
	if (shader[0] && shader[1]) {
		polymostglsl.program = glbuild_link_program(2, &shader[0]);
	}
	if (shader[0] != 0) glfunc.glDeleteShader(shader[0]);
	if (shader[1] != 0) glfunc.glDeleteShader(shader[1]);

	if (polymostglsl.program) {
		polymostglsl.attrib_vertex       = polymost_get_attrib(polymostglsl.program,  "a_vertex");
		polymostglsl.attrib_texcoord     = polymost_get_attrib(polymostglsl.program,  "a_texcoord");
		polymostglsl.uniform_modelview   = polymost_get_uniform(polymostglsl.program, "u_modelview");
		polymostglsl.uniform_projection  = polymost_get_uniform(polymostglsl.program, "u_projection");
		polymostglsl.uniform_texture     = polymost_get_uniform(polymostglsl.program, "u_texture");
		polymostglsl.uniform_glowtexture = polymost_get_uniform(polymostglsl.program, "u_glowtexture");
		polymostglsl.uniform_alphacut    = polymost_get_uniform(polymostglsl.program, "u_alphacut");
		polymostglsl.uniform_colour      = polymost_get_uniform(polymostglsl.program, "u_colour");
		polymostglsl.uniform_fogcolour   = polymost_get_uniform(polymostglsl.program, "u_fogcolour");
		polymostglsl.uniform_fogdensity  = polymost_get_uniform(polymostglsl.program, "u_fogdensity");

#if (USE_OPENGL == USE_GL3)
		glfunc.glGenVertexArrays(1, &polymostglsl.vao);
        glfunc.glBindVertexArray(polymostglsl.vao);
        glfunc.glEnableVertexAttribArray(polymostglsl.attrib_vertex);
        glfunc.glEnableVertexAttribArray(polymostglsl.attrib_texcoord);
#endif

		glfunc.glUseProgram(polymostglsl.program);
		glfunc.glUniform1i(polymostglsl.uniform_texture, 0);		//GL_TEXTURE0
		glfunc.glUniform1i(polymostglsl.uniform_glowtexture, 1);	//GL_TEXTURE1

		// Generate a buffer object for vertex/colour elements.
		glfunc.glGenBuffers(1, &polymostglsl.elementbuffer);
	}

	// A fully transparent texture for the case when a glow texture is not needed.
	if (!nulltexture) {
		const std::array<const char, 4> pix = {0, 0, 0, 0};
		glfunc.glGenTextures(1, &nulltexture);
		glfunc.glBindTexture(GL_TEXTURE_2D, nulltexture);
		glfunc.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,(GLvoid*)&pix);
		glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	}

	// Engine auxiliary shader.
	if (polymostauxglsl.program) {
		glfunc.glDeleteProgram(polymostauxglsl.program);
		polymostauxglsl.program = 0;
	}

	shader[0] = polymost_load_shader(GL_VERTEX_SHADER, default_polymostaux_vs_glsl, "polymostaux_vs.glsl");
	shader[1] = polymost_load_shader(GL_FRAGMENT_SHADER, default_polymostaux_fs_glsl, "polymostaux_fs.glsl");
	if (shader[0] && shader[1]) {
		polymostauxglsl.program = glbuild_link_program(2, &shader[0]);
	}
	if (shader[0]) glfunc.glDeleteShader(shader[0]);
	if (shader[1]) glfunc.glDeleteShader(shader[1]);

	if (polymostauxglsl.program) {
		polymostauxglsl.attrib_vertex    = polymost_get_attrib(polymostauxglsl.program,    "a_vertex");
		polymostauxglsl.attrib_texcoord  = polymost_get_attrib(polymostauxglsl.program,    "a_texcoord");
		polymostauxglsl.uniform_projection = polymost_get_uniform(polymostauxglsl.program, "u_projection");
		polymostauxglsl.uniform_texture  = polymost_get_uniform(polymostauxglsl.program,   "u_texture");
		polymostauxglsl.uniform_colour   = polymost_get_uniform(polymostauxglsl.program,   "u_colour");
		polymostauxglsl.uniform_bgcolour = polymost_get_uniform(polymostauxglsl.program,   "u_bgcolour");
		polymostauxglsl.uniform_mode     = polymost_get_uniform(polymostauxglsl.program,   "u_mode");

#if (USE_OPENGL == USE_GL3)
		glfunc.glGenVertexArrays(1, &polymostauxglsl.vao);
        glfunc.glBindVertexArray(polymostauxglsl.vao);
        glfunc.glEnableVertexAttribArray(polymostauxglsl.attrib_vertex);
        glfunc.glEnableVertexAttribArray(polymostauxglsl.attrib_texcoord);
#endif

		glfunc.glUseProgram(polymostauxglsl.program);
		glfunc.glUniform1i(polymostauxglsl.uniform_texture, 0);	//GL_TEXTURE0

		// Generate a buffer object for vertex/colour elements and pre-allocate its memory.
		glfunc.glGenBuffers(1, &polymostauxglsl.elementbuffer);

		// Generate a buffer object for ad-hoc indexes.
		glfunc.glGenBuffers(1, &polymostauxglsl.indexbuffer);
	}

	// Generate a buffer object for element indexes and populate it with an
	// initial set of ascending indices, the way drawpoly() will generate points.
	glfunc.glGenBuffers(1, &elementindexbuffer);
	checkindexbuffer(MINVBOINDEXES);
}

} // namespace

// one-time initialisation of OpenGL for polymost
void polymost_glinit()
{
	glfunc.glClearColor(0,0,0,0.5); //Black Background
	glfunc.glDisable(GL_DITHER);

	glfunc.glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	glfunc.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if (glmultisample > 0 && glinfo.multisample) {
#if (USE_OPENGL != USE_GLES2)
#ifdef GL_NV_multisample_filter_hint
		if (glinfo.nvmultisamplehint)
			glfunc.glHint(GL_MULTISAMPLE_FILTER_HINT_NV, glnvmultisamplehint ? GL_NICEST:GL_FASTEST);
#endif

		glfunc.glEnable(GL_MULTISAMPLE);

		if (glsampleshading > 0 && glinfo.sampleshading) {
			glfunc.glMinSampleShadingARB(1.F);
			glfunc.glEnable(GL_SAMPLE_SHADING_ARB);
		}
#endif
	}

	polymost_loadshaders();
}

void resizeglcheck ()
{
	if (glredbluemode < lastglredbluemode) {
		glox1 = -1;
		glfunc.glColorMask(1,1,1,1);
	}
	else if (glredbluemode != lastglredbluemode) {
		redblueclearcnt = 0;
	}

	lastglredbluemode = glredbluemode;

#if (USE_OPENGL != USE_GLES2)
	if (lastglpolygonmode != glpolygonmode)
	{
		lastglpolygonmode = glpolygonmode;
		switch(glpolygonmode)
		{
			default:
			case 0: glfunc.glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); break;
			case 1: glfunc.glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); break;
			case 2: glfunc.glPolygonMode(GL_FRONT_AND_BACK,GL_POINT); break;
		}
	}
#endif

	// FIXME: Float comparisons
	if ((glox1 != windowx1) || (gloy1 != windowy1) || (glox2 != windowx2) || (gloy2 != windowy2))
	{
		glox1 = windowx1;
		gloy1 = windowy1;
		glox2 = windowx2;
		gloy2 = windowy2;

		glfunc.glViewport(windowx1,yres-(windowy2+1),windowx2-windowx1+1,windowy2-windowy1+1);
	}
}

void polymost_aftershowframe()
{
#if (USE_OPENGL != USE_GLES2)
	if (glpolygonmode)
	{
		glfunc.glClearColor(1.0, 1.0, 1.0, 0.0);
		glfunc.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
#endif
}

void polymost_setview()
{
	std::memset(gdrawroomsprojmat, 0, sizeof(gdrawroomsprojmat));
	gdrawroomsprojmat[0][0] = (float)ydimen;
	gdrawroomsprojmat[0][2] = 1.0;
	gdrawroomsprojmat[1][1] = (float)xdimen;
	gdrawroomsprojmat[1][2] = 1.0;
	gdrawroomsprojmat[2][2] = 1.0;
	gdrawroomsprojmat[2][3] = (float)ydimen;
	gdrawroomsprojmat[3][2] =-1.0;

	std::memset(grotatespriteprojmat, 0, sizeof(grotatespriteprojmat));
	grotatespriteprojmat[0][0] = 1.0;
	grotatespriteprojmat[2][3] = 1.0;
	grotatespriteprojmat[1][1] = ((float)xdim) / ((float)ydim);
	grotatespriteprojmat[2][2] = 1.0001;
	grotatespriteprojmat[3][2] = 1 - grotatespriteprojmat[2][2];

	std::memset(gorthoprojmat, 0, sizeof(gorthoprojmat));
	gorthoprojmat[0][0] = 2 / (float)xdim;
	gorthoprojmat[1][1] = -2 / (float)ydim;
	gorthoprojmat[2][2] = -1.0;
	gorthoprojmat[3][3] = 1.0;
	gorthoprojmat[3][0] = -1.0;
	gorthoprojmat[3][1] = 1.0;
}

void polymost_drawpoly_glcall(GLenum mode, struct polymostdrawpolycall const *draw)
{
#ifdef DEBUGGINGAIDS
	polymostcallcounts.drawpoly_glcall++;
#endif

	glfunc.glUseProgram(polymostglsl.program);

#if (USE_OPENGL == USE_GL3)
    glfunc.glBindVertexArray(polymostglsl.vao);
#else
    glfunc.glEnableVertexAttribArray(polymostglsl.attrib_vertex);
    glfunc.glEnableVertexAttribArray(polymostglsl.attrib_texcoord);
#endif

	if (draw->elementbuffer > 0) {
		glfunc.glBindBuffer(GL_ARRAY_BUFFER, draw->elementbuffer);
	} else {
		// Drawing from the passed elementvbo items.
		glfunc.glBindBuffer(GL_ARRAY_BUFFER, polymostglsl.elementbuffer);
		glfunc.glBufferData(GL_ARRAY_BUFFER, draw->elementcount * sizeof(polymostvboitem), draw->elementvbo, GL_STREAM_DRAW);
	}

	if (draw->indexbuffer > 0) {
		glfunc.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw->indexbuffer);
	} else {
		glfunc.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementindexbuffer);
		checkindexbuffer(draw->indexcount);
	}

	glfunc.glVertexAttribPointer(polymostglsl.attrib_vertex, 3, GL_FLOAT, GL_FALSE,
		sizeof(polymostvboitem), (const GLvoid *)offsetof(polymostvboitem, v));
	glfunc.glVertexAttribPointer(polymostglsl.attrib_texcoord, 2, GL_FLOAT, GL_FALSE,
		sizeof(polymostvboitem), (const GLvoid *)offsetof(polymostvboitem, t));

	glfunc.glActiveTexture(GL_TEXTURE0);
	glfunc.glBindTexture(GL_TEXTURE_2D, draw->texture0);

	glfunc.glActiveTexture(GL_TEXTURE1);
	glfunc.glBindTexture(GL_TEXTURE_2D, draw->texture1 ? draw->texture1 : nulltexture);

	glfunc.glUniform1f(polymostglsl.uniform_alphacut, draw->alphacut);

	glfunc.glUniform4f(
		polymostglsl.uniform_colour,
		draw->colour.r, draw->colour.g, draw->colour.b, draw->colour.a
	);
	glfunc.glUniform4f(
		polymostglsl.uniform_fogcolour,
		draw->fogcolour.r, draw->fogcolour.g, draw->fogcolour.b, draw->fogcolour.a
	);
	glfunc.glUniform1f(
		polymostglsl.uniform_fogdensity,
		draw->fogdensity
	);

	glfunc.glUniformMatrix4fv(polymostglsl.uniform_modelview, 1, GL_FALSE, draw->modelview);
	glfunc.glUniformMatrix4fv(polymostglsl.uniform_projection, 1, GL_FALSE, draw->projection);

	glfunc.glDrawElements(mode, draw->indexcount, GL_UNSIGNED_SHORT, nullptr);

#if (USE_OPENGL == USE_GL3)
    glfunc.glBindVertexArray(0);
#else
	glfunc.glDisableVertexAttribArray(polymostglsl.attrib_vertex);
	glfunc.glDisableVertexAttribArray(polymostglsl.attrib_texcoord);
#endif
}

namespace {

void polymost_drawaux_glcall(GLenum mode, struct polymostdrawauxcall const *draw)
{
#ifdef DEBUGGINGAIDS
	polymostcallcounts.drawaux_glcall++;
#endif

	glfunc.glUseProgram(polymostauxglsl.program);

#if (USE_OPENGL == USE_GL3)
    glfunc.glBindVertexArray(polymostauxglsl.vao);
#else
    glfunc.glEnableVertexAttribArray(polymostauxglsl.attrib_vertex);
	glfunc.glEnableVertexAttribArray(polymostauxglsl.attrib_texcoord);
#endif

	glfunc.glBindBuffer(GL_ARRAY_BUFFER, polymostauxglsl.elementbuffer);
	glfunc.glBufferData(GL_ARRAY_BUFFER, draw->elementcount * sizeof(polymostvboitem), draw->elementvbo, GL_STREAM_DRAW);

	if (draw->indexes) {
		glfunc.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, polymostauxglsl.indexbuffer);
		glfunc.glBufferData(GL_ELEMENT_ARRAY_BUFFER, draw->indexcount * sizeof(GLushort), draw->indexes, GL_STREAM_DRAW);
	} else {
		glfunc.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementindexbuffer);
		checkindexbuffer(draw->indexcount);
	}

	glfunc.glVertexAttribPointer(polymostauxglsl.attrib_vertex, 3, GL_FLOAT, GL_FALSE,
		sizeof(polymostvboitem), (const GLvoid *)offsetof(polymostvboitem, v));
	glfunc.glVertexAttribPointer(polymostauxglsl.attrib_texcoord, 2, GL_FLOAT, GL_FALSE,
		sizeof(polymostvboitem), (const GLvoid *)offsetof(polymostvboitem, t));

	glfunc.glActiveTexture(GL_TEXTURE0);
	glfunc.glBindTexture(GL_TEXTURE_2D, draw->texture0);

	glfunc.glUniform4f(
		polymostauxglsl.uniform_colour,
		draw->colour.r, draw->colour.g, draw->colour.b, draw->colour.a
	);
	glfunc.glUniform4f(
		polymostauxglsl.uniform_bgcolour,
		draw->bgcolour.r, draw->bgcolour.g, draw->bgcolour.b, draw->bgcolour.a
	);
	glfunc.glUniform1i(polymostauxglsl.uniform_mode, draw->mode);

	glfunc.glUniformMatrix4fv(polymostauxglsl.uniform_projection, 1, GL_FALSE, &gorthoprojmat[0][0]);

	glfunc.glDrawElements(mode, draw->indexcount, GL_UNSIGNED_SHORT, nullptr);

#if (USE_OPENGL == USE_GL3)
    glfunc.glBindVertexArray(0);
#else
	glfunc.glDisableVertexAttribArray(polymostauxglsl.attrib_vertex);
	glfunc.glDisableVertexAttribArray(polymostauxglsl.attrib_texcoord);
#endif
}

void polymost_palfade()
{
	struct polymostdrawauxcall draw;
	std::array<polymostvboitem, 4> vboitem;

	if ((rendmode != rendmode_t::OpenGL) || (qsetmode != 200)) return;
	if (palfadedelta == 0) return;

	draw.mode = 2;	// Solid colour.

	draw.colour.r = palfadergb.r / 255.F;
	draw.colour.g = palfadergb.g / 255.F;
	draw.colour.b = palfadergb.b / 255.F;
	draw.colour.a = palfadedelta / 255.F;

	vboitem[0].v.x = 0.F;
	vboitem[0].v.y = 0.F;
	vboitem[0].v.z = 0.F;

	vboitem[1].v.x = (float)xdim;
	vboitem[1].v.y = 0.F;
	vboitem[1].v.z = 0.F;

	vboitem[2].v.x = (float)xdim;
	vboitem[2].v.y = (float)ydim;
	vboitem[2].v.z = 0.F;

	vboitem[3].v.x = 0.F;
	vboitem[3].v.y = (float)ydim;
	vboitem[3].v.z = 0.F;

	draw.indexcount = 4;
	draw.indexes = nullptr;
	draw.elementcount = 4;
	draw.elementvbo = &vboitem[0];

	glfunc.glDisable(GL_DEPTH_TEST);
	glfunc.glEnable(GL_BLEND);

	polymost_drawaux_glcall(GL_TRIANGLE_FAN, &draw);
}

} // namespace

#endif //USE_OPENGL

void polymost_nextpage()
{
#if USE_OPENGL
	polymost_palfade();
#endif

#ifdef DEBUGGINGAIDS
	if (polymostshowcallcounts) {
		char buf[1024];
		std::sprintf(buf,
			"drawpoly_gl({}) drawaux_gl({}) drawpoly({}) "
			"domost({}) drawalls({}) drawmaskwall({}) drawsprite({})",
	    		polymostcallcounts.drawpoly_glcall,
	    		polymostcallcounts.drawaux_glcall,
	    		polymostcallcounts.drawpoly,
	    		polymostcallcounts.domost,
	    		polymostcallcounts.drawalls,
	    		polymostcallcounts.drawmaskwall,
	    		polymostcallcounts.drawsprite
		);
		printext256(0, 8, 31, -1, buf, 0);
	}
	std::memset(&polymostcallcounts, 0, sizeof(polymostcallcounts));
#endif
}

	//(dpx,dpy) specifies an n-sided polygon. The polygon must be a convex clockwise loop.
	//    n must be <= 8 (assume clipping can double number of vertices)
	//method: 0:solid, 1:masked(255 is transparent), 2:transluscent #1, 3:transluscent #2
	//    +4 means it's a sprite, so wraparound isn't needed
void drawpoly (std::span<const double> dpx, std::span<const double> dpy, int n, int method)
{
	double ngdx = 0.0;
	double ngdy = 0.0;
	double ngdo = 0.0;
	double ngux = 0.0;
	double nguy = 0.0;
	double nguo = 0.0;
	double ngvx = 0.0;
	double ngvy = 0.0;
	double ngvo = 0.0;
	double dp;
	double up;
	double vp;
	double rdp;
	double ngdx2;
	double ngux2;
	double ngvx2;
	double f;
	double r;
	double ox;
	double oy;
	double oz;
	double ox2;
	double oy2;
	double oz2;
	std::array<double, 16> dd;
	std::array<double, 16> uu;
	std::array<double, 16> vv;
	std::array<double, 16> px;
	std::array<double, 16> py;
	int i;
	int j;
	int k;
	int x;
	int y;
	int z;
	int ix0;
	int ix1;
	int mini;
	int maxi;
	int tsizx;
	int tsizy;
	int tsizxm1 = 0;
	int tsizym1 = 0;
	int ltsizy = 0;
	int xx;
	int yy;
	int xi;
	int d0;
	int u0;
	int v0;
	int d1;
	int u1;
	int v1;
	bool xmodnice{false};
	bool ymulnice{false};
	unsigned char dacol = 0, *walptr, *palptr = nullptr, *vidp, *vide;

#ifdef DEBUGGINGAIDS
	polymostcallcounts.drawpoly++;
#endif

	if (method == -1) {
		return;
	}

	if (n == 3) {
		if ((dpx[0]-dpx[1])*(dpy[2]-dpy[1]) >= (dpx[2]-dpx[1])*(dpy[0]-dpy[1])) {
			return; //for triangle
		}
	}
	else {
		f = 0; //f is area of polygon / 2
		for(i=n-2,j=n-1,k=0;k<n;i=j,j=k,k++) f += (dpx[i]-dpx[k])*dpy[j];
		
		if (f <= 0) {
			return;
		}
	}

		//Load texture (globalpicnum)
	if ((unsigned int)globalpicnum >= MAXTILES) globalpicnum = 0;
	setgotpic(globalpicnum);
	tsizx = tilesizx[globalpicnum];
	tsizy = tilesizy[globalpicnum];
	if (palookup[globalpal].empty()) globalpal = 0;
	if (!waloff[globalpicnum])
	{
		loadtile(globalpicnum);
		if (!waloff[globalpicnum])
		{
			if (rendmode != rendmode_t::OpenGL) return;
			tsizx = tsizy = 1; method = METH_MASKED; //Hack to update Z-buffer for invalid mirror textures
		}
	}
	walptr = (unsigned char *)waloff[globalpicnum];

	j = 0;
	bool dorot = ((gchang != 1.0) || (gctang != 1.0));

	if (dorot)
	{
		for(i=0;i<n;i++)
		{
			ox = dpx[i]-ghalfx;
			oy = dpy[i]-ghoriz;
			oz = ghalfx;

				//Up/down rotation
			ox2 = ox;
			oy2 = oy*gchang - oz*gshang;
			oz2 = oy*gshang + oz*gchang;

				//Tilt rotation
			ox = ox2*gctang - oy2*gstang;
			oy = ox2*gstang + oy2*gctang;
			oz = oz2;

			if ((oz < SCISDIST) && (rendmode != rendmode_t::OpenGL)) return; //annoying hack to avoid bugs in software rendering

			r = ghalfx / oz;

			dd[j] = (dpx[i]*gdx + dpy[i]*gdy + gdo)*r;
			uu[j] = (dpx[i]*gux + dpy[i]*guy + guo)*r;
			vv[j] = (dpx[i]*gvx + dpy[i]*gvy + gvo)*r;

			px[j] = ox*r + ghalfx;
			py[j] = oy*r + ghoriz;
			if ((!j) || (px[j] != px[j-1]) || (py[j] != py[j-1])) j++;
		}
	}
	else
	{
		for(i=0;i<n;i++)
		{
			px[j] = dpx[i];
			py[j] = dpy[i];
			if ((!j) || (px[j] != px[j-1]) || (py[j] != py[j-1])) j++;
		}
	}
	while ((j >= 3) && (px[j-1] == px[0]) && (py[j-1] == py[0])) j--;
	if (j < 3) return;
	n = j;

#if USE_OPENGL
	int nn;
	double uoffs;
	double du0 = 0.0;
	double du1 = 0.0;
	double dui;
	double duj;

	if (rendmode == rendmode_t::OpenGL)
	{
		unsigned short ptflags{0};
		int picidx{PTHPIC_BASE};
		PTHead * pth{nullptr};
		struct polymostdrawpolycall draw;
		std::array<polymostvboitem, MINVBOINDEXES> vboitem;

		if (usehightile)
			ptflags |= PTH_HIGHTILE;
		
		if (method & METH_CLAMPED) ptflags |= PTH_CLAMPED;
		if (drawingskybox) ptflags |= PTH_SKYBOX;

		pth = PT_GetHead(globalpicnum, globalpal, ptflags, 0);

		// Base texture.
		draw.texture0 = 0;

		if (pth) {
			if (drawingskybox) {
				picidx = drawingskybox - 1;
			}
			if (pth->pic[picidx]) {
				draw.texture0 = pth->pic[picidx]->glpic;
			}
		}

		// Glow texture.
		draw.texture1 = nulltexture;

		if ((method & METH_LAYERS) && pth && !drawingskybox) {
			if (pth->pic[ PTHPIC_GLOW ]) {
				draw.texture1 = pth->pic[ PTHPIC_GLOW ]->glpic;
			}
		}

		if (!(method & (METH_MASKED | METH_TRANS))) {
			glfunc.glDisable(GL_BLEND);
			draw.alphacut = 0.F;
		}
		else {
			float alphac = 0.32;
			if (pth && pth->repldef && pth->repldef->alphacut >= 0.0) {
				alphac = pth->repldef->alphacut;
			}

			if (usegoodalpha) {
				alphac = 0.0;
			}

			if (!waloff[globalpicnum]) {
				alphac = 0.0;	// invalid textures ignore the alpha cutoff settings
			}

			glfunc.glEnable(GL_BLEND);
			draw.alphacut = alphac;
		}

		draw.fogcolour.r = static_cast<float>(palookupfog[gfogpalnum].r) / 63.F;
		draw.fogcolour.g = static_cast<float>(palookupfog[gfogpalnum].g) / 63.F;
		draw.fogcolour.b = static_cast<float>(palookupfog[gfogpalnum].b) / 63.F;
		draw.fogcolour.a = 1.F;

		draw.fogdensity = gfogdensity;

		const float hackscx = pth->scalex;
		const float hackscy = pth->scaley;
		tsizx   = pth->pic[picidx]->tsizx;
		tsizy   = pth->pic[picidx]->tsizy;
		xx      = pth->pic[picidx]->sizx;
		yy      = pth->pic[picidx]->sizy;
		ox2     = (double)1.0/(double)xx;
		oy2     = (double)1.0/(double)yy;

		if (!dorot)
		{
			for(i=n-1;i>=0;i--)
			{
				dd[i] = px[i]*gdx + py[i]*gdy + gdo;
				uu[i] = px[i]*gux + py[i]*guy + guo;
				vv[i] = px[i]*gvx + py[i]*gvy + gvo;
			}
		}

		draw.colour.r = draw.colour.g = draw.colour.b =
			((float)(numpalookups - std::min(std::max(globalshade, 0), static_cast<int>(numpalookups))))/((float)numpalookups);
		switch(method & (METH_MASKED | METH_TRANS))
		{
			case METH_SOLID:   draw.colour.a = 1.0; break;
			case METH_MASKED:  draw.colour.a = 1.0; break;
			case METH_TRANS:   draw.colour.a = 0.66; break;
			case METH_INTRANS: draw.colour.a = 0.33; break;
		}
		// tinting happens only to hightile textures, and only if the texture we're
		// rendering isn't for the same palette as what we asked for
		if (pth && (pth->flags & PTH_HIGHTILE) && (globalpal != pth->repldef->palnum)) {
			// apply tinting for replaced textures
			draw.colour.r *= (float)hictinting[globalpal].r / 255.0;
			draw.colour.g *= (float)hictinting[globalpal].g / 255.0;
			draw.colour.b *= (float)hictinting[globalpal].b / 255.0;
		}

		draw.modelview = &gidentitymat[0][0];
		if (method & METH_ROTATESPRITE) {
			draw.projection = &grotatespriteprojmat[0][0];
		} else {
			draw.projection = &gdrawroomsprojmat[0][0];
		}

		draw.indexbuffer = 0;
		draw.elementbuffer = 0;
		draw.elementvbo = &vboitem[0];

			//Hack for walls&masked walls which use textures that are not a power of 2
		if ((method & METH_POW2XSPLIT) && (tsizx != xx))
		{
			if (!dorot)
			{
				ngdx = gdx;
				ngdy = gdy;
				ngdo = gdo+(ngdx+ngdy)*.5;
				ngux = gux;
				nguy = guy;
				nguo = guo+(ngux+nguy)*.5;
				ngvx = gvx;
				ngvy = gvy;
				ngvo = gvo+(ngvx+ngvy)*.5;
			}
			else
			{
				ox = py[1]-py[2];
				oy = py[2]-py[0];
				oz = py[0]-py[1];
				r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
				ngdx = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				ngux = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				ngvx = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				ox = px[2]-px[1]; oy = px[0]-px[2]; oz = px[1]-px[0];
				ngdy = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				nguy = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				ngvy = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				ox = px[0]-.5; oy = py[0]-.5; //.5 centers texture nicely
				ngdo = dd[0] - ox*ngdx - oy*ngdy;
				nguo = uu[0] - ox*ngux - oy*nguy;
				ngvo = vv[0] - ox*ngvx - oy*ngvy;
			}

			ngux *= hackscx; nguy *= hackscx; nguo *= hackscx;
			ngvx *= hackscy; ngvy *= hackscy; ngvo *= hackscy;
			uoffs = ((double)(xx-tsizx)*.5);
			ngux -= ngdx*uoffs;
			nguy -= ngdy*uoffs;
			nguo -= ngdo*uoffs;

				//Find min&max u coordinates (du0...du1)
			for(i=0;i<n;i++)
			{
				ox = px[i]; oy = py[i];
				f = (ox*ngux + oy*nguy + nguo) / (ox*ngdx + oy*ngdy + ngdo);
				if (!i) { du0 = du1 = f; continue; }
					  if (f < du0) du0 = f;
				else if (f > du1) du1 = f;
			}

			f = 1.0/(double)tsizx;
			ix0 = (int)floor(du0*f);
			ix1 = (int)floor(du1*f);
			for(;ix0<=ix1;ix0++)
			{
				du0 = (double)((ix0  )*tsizx); // + uoffs;
				du1 = (double)((ix0+1)*tsizx); // + uoffs;

				i = 0; nn = 0;
				duj = (px[i]*ngux + py[i]*nguy + nguo) / (px[i]*ngdx + py[i]*ngdy + ngdo);
				do
				{
					j = i+1; if (j == n) j = 0;

					dui = duj;
					duj = (px[j]*ngux + py[j]*nguy + nguo) / (px[j]*ngdx + py[j]*ngdy + ngdo);

					if ((du0 <= dui) && (dui <= du1)) {
						uu[nn] = px[i];
						vv[nn] = py[i];
						nn++;
					}

					if (duj <= dui)
					{
						if ((du1 < duj) != (du1 < dui))
						{
								//ox*(ngux-ngdx*du1) + oy*(nguy-ngdy*du1) + (nguo-ngdo*du1) = 0
								//(px[j]-px[i])*f + px[i] = ox
								//(py[j]-py[i])*f + py[i] = oy

								///Solve for f
								//((px[j]-px[i])*f + px[i])*(ngux-ngdx*du1) +
								//((py[j]-py[i])*f + py[i])*(nguy-ngdy*du1) + (nguo-ngdo*du1) = 0

							f = -(       px[i] *(ngux-ngdx*du1) +  py[i]       *(nguy-ngdy*du1) + (nguo-ngdo*du1)) /
								  ((px[j]-px[i])*(ngux-ngdx*du1) + (py[j]-py[i])*(nguy-ngdy*du1));
							uu[nn] = (px[j]-px[i])*f + px[i];
							vv[nn] = (py[j]-py[i])*f + py[i]; nn++;
						}
						if ((du0 < duj) != (du0 < dui))
						{
							f = -(       px[i] *(ngux-ngdx*du0) +        py[i] *(nguy-ngdy*du0) + (nguo-ngdo*du0)) /
								  ((px[j]-px[i])*(ngux-ngdx*du0) + (py[j]-py[i])*(nguy-ngdy*du0));
							uu[nn] = (px[j]-px[i])*f + px[i];
							vv[nn] = (py[j]-py[i])*f + py[i]; nn++;
						}
					}
					else
					{
						if ((du0 < duj) != (du0 < dui))
						{
							f = -(       px[i] *(ngux-ngdx*du0) +        py[i] *(nguy-ngdy*du0) + (nguo-ngdo*du0)) /
								  ((px[j]-px[i])*(ngux-ngdx*du0) + (py[j]-py[i])*(nguy-ngdy*du0));
							uu[nn] = (px[j]-px[i])*f + px[i];
							vv[nn] = (py[j]-py[i])*f + py[i]; nn++;
						}
						if ((du1 < duj) != (du1 < dui))
						{
							f = -(       px[i] *(ngux-ngdx*du1) +  py[i]       *(nguy-ngdy*du1) + (nguo-ngdo*du1)) /
								  ((px[j]-px[i])*(ngux-ngdx*du1) + (py[j]-py[i])*(nguy-ngdy*du1));
							uu[nn] = (px[j]-px[i])*f + px[i];
							vv[nn] = (py[j]-py[i])*f + py[i]; nn++;
						}
					}
					i = j;
				} while (i);
				if (nn < 3) continue;

				for(i=0;i<nn;i++)
				{
					ox = uu[i];
					oy = vv[i];
					dp = ox*ngdx + oy*ngdy + ngdo;
					up = ox*ngux + oy*nguy + nguo;
					vp = ox*ngvx + oy*ngvy + ngvo;
					r = 1.0/dp;

					vboitem[i].v.x = (ox-ghalfx)*r*grhalfxdown10x;
					vboitem[i].v.y = (ghoriz-oy)*r*grhalfxdown10;
					vboitem[i].v.z = r*(1.0/1024.0);
					vboitem[i].t.s = (up*r-du0+uoffs)*ox2;
					vboitem[i].t.t = vp*r*oy2;
				}
				draw.indexcount = nn;
				draw.elementcount = nn;

				glfunc.glDepthMask(GL_TRUE);
				polymost_drawpoly_glcall(GL_TRIANGLE_FAN, &draw);
			}
		}
		else if (n > 0)
		{
			ox2 *= hackscx;
			oy2 *= hackscy;

			for(i=0;i<n;i++)
			{
				r = 1.0/dd[i];

				vboitem[i].v.x = (px[i]-ghalfx)*r*grhalfxdown10x;
				vboitem[i].v.y = (ghoriz-py[i])*r*grhalfxdown10;
				vboitem[i].v.z = r*(1.0/1024.0);
				vboitem[i].t.s = uu[i]*r*ox2;
				vboitem[i].t.t = vv[i]*r*oy2;
			}
			draw.indexcount = n;
			draw.elementcount = n;

			glfunc.glDepthMask(GL_TRUE);
			polymost_drawpoly_glcall(GL_TRIANGLE_FAN, &draw);
		}

		return;
	}
#endif

	if (rendmode == rendmode_t::Software)
	{
#if (USEZBUFFER != 0)
		if ((!zbufmem) || (zbufbpl != bytesperline) || (zbufysiz != ydim))
		{
			zbufbpl = bytesperline;
			zbufysiz = ydim;
			zbufmem = (intptr_t)realloc((void *)zbufmem,zbufbpl*zbufysiz*4);
		}
		zbufoff = (int *)(zbufmem-(frameplace<<2));
#endif
		// TODO: Translucency should always exist, yes?
		// if ((!transluc)) method = (method & ~(METH_MASKED | METH_TRANS)) + METH_MASKED; //In case translucent table doesn't exist

		if (!dorot)
		{
			ngdx = gdx;
			ngdy = gdy;
			ngdo = gdo + (ngdx + ngdy) * .5;
			ngux = gux;
			nguy = guy;
			nguo = guo + (ngux + nguy) * .5;
			ngvx = gvx;
			ngvy = gvy;
			ngvo = gvo + (ngvx + ngvy) * .5;
		}
		else
		{
				//General equations:
				//dd[i] = (px[i]*gdx + py[i]*gdy + gdo)
				//uu[i] = (px[i]*gux + py[i]*guy + guo)/dd[i]
				//vv[i] = (px[i]*gvx + py[i]*gvy + gvo)/dd[i]
				//
				//px[0]*gdx + py[0]*gdy + 1*gdo = dd[0]
				//px[1]*gdx + py[1]*gdy + 1*gdo = dd[1]
				//px[2]*gdx + py[2]*gdy + 1*gdo = dd[2]
				//
				//px[0]*gux + py[0]*guy + 1*guo = uu[0]*dd[0] (uu[i] premultiplied by dd[i] above)
				//px[1]*gux + py[1]*guy + 1*guo = uu[1]*dd[1]
				//px[2]*gux + py[2]*guy + 1*guo = uu[2]*dd[2]
				//
				//px[0]*gvx + py[0]*gvy + 1*gvo = vv[0]*dd[0] (vv[i] premultiplied by dd[i] above)
				//px[1]*gvx + py[1]*gvy + 1*gvo = vv[1]*dd[1]
				//px[2]*gvx + py[2]*gvy + 1*gvo = vv[2]*dd[2]
			ox = py[1] - py[2];
			oy = py[2] - py[0];
			oz = py[0] - py[1];
			r = 1.0 / (ox * px[0] + oy * px[1] + oz * px[2]);
			ngdx = (ox * dd[0] + oy * dd[1] + oz * dd[2]) * r;
			ngux = (ox * uu[0] + oy * uu[1] + oz * uu[2]) * r;
			ngvx = (ox * vv[0] + oy * vv[1] + oz * vv[2]) * r;
			ox = px[2] - px[1];
			oy = px[0] - px[2];
			oz = px[1] - px[0];
			ngdy = (ox * dd[0] + oy * dd[1] + oz * dd[2]) * r;
			nguy = (ox * uu[0] + oy * uu[1] + oz * uu[2]) * r;
			ngvy = (ox * vv[0] + oy * vv[1] + oz * vv[2]) * r;
			ox = px[0] - .5;
			oy = py[0] - .5; //.5 centers texture nicely
			ngdo = dd[0] - ox * ngdx - oy * ngdy;
			nguo = uu[0] - ox * ngux - oy * nguy;
			ngvo = vv[0] - ox * ngvx - oy * ngvy;
		}
		palptr = &palookup[globalpal][std::min(std::max(globalshade, 0), static_cast<int>(numpalookups) - 1) << 8]; //<-need to make shade not static!

		tsizxm1 = tsizx-1;
		xmodnice = !(tsizxm1 & tsizx);
		tsizym1 = tsizy-1;
		ymulnice = !(tsizym1 & tsizy);
		if ((method & METH_CLAMPED) && !xmodnice) //Sprites don't need a mod on texture coordinates
			{ xmodnice = true; for(tsizxm1=1;tsizxm1<tsizx;tsizxm1=(tsizxm1<<1)+1); }
		if (!ymulnice) { for(tsizym1=1;tsizym1+1<tsizy;tsizym1=(tsizym1<<1)+1); }
		ltsizy = (picsiz[globalpicnum]>>4);
	}
	else
	{
		dacol = palookup[0][(int)(*(unsigned char *)(waloff[globalpicnum]))+(std::min(std::max(globalshade, 0), static_cast<int>(numpalookups) - 1) << 8)];
	}

	if (grhalfxdown10x < 0) //Hack for mirrors
	{
		for(i=((n-1)>>1);i>=0;i--)
		{
			r = px[i];
			px[i] = ((double)xdimen)-px[n-1-i];
			px[n-1-i] = ((double)xdimen)-r;
			r = py[i];
			py[i] = py[n-1-i];
			py[n-1-i] = r;
		}
		ngdo += ((double)xdimen)*ngdx; ngdx = -ngdx;
		nguo += ((double)xdimen)*ngux; ngux = -ngux;
		ngvo += ((double)xdimen)*ngvx; ngvx = -ngvx;
	}

	ngdx2 = ngdx * (1 << LINTERPSIZ);
	ngux2 = ngux * (1 << LINTERPSIZ);
	ngvx2 = ngvx * (1 << LINTERPSIZ);

	mini = (py[0] >= py[1]);
	maxi = 1-mini;

	for(z=2;z<n;z++) {
		if (py[z] < py[mini])
			mini = z;

		if (py[z] > py[maxi])
			maxi = z;
	}

	i = maxi;
	dtol(py[i],&yy); if (yy > ydimen) yy = ydimen;
	do
	{
		j = i+1; if (j == n) j = 0;
		dtol(py[j],&y); if (y < 0) y = 0;
		if (y < yy)
		{
			f = (px[j]-px[i])/(py[j]-py[i]); dtol(f*16384.0,&xi);
			dtol((((double)yy-.5-py[j])*f + px[j])*16384.0+8192.0,&x);
			for(;yy>y;yy--,x-=xi) lastx[yy-1] = (x>>14);
		}
		i = j;
	} while (i != mini);
	do
	{
		j = i+1; if (j == n) j = 0;
		dtol(py[j],&yy); if (yy > ydimen) yy = ydimen;
		if (y < yy)
		{
			f = (px[j]-px[i])/(py[j]-py[i]); dtol(f*16384.0,&xi);
			dtol((((double)y+.5-py[j])*f + px[j])*16384.0+8192.0,&x);
			for(;y<yy;y++,x+=xi)
			{
				ix0 = lastx[y]; if (ix0 < 0) ix0 = 0;
				ix1 = (x>>14); if (ix1 > xdimen) ix1 = xdimen;
				if (ix0 < ix1)
				{
					if (rendmode == rendmode_t::SolidColor)
						std::memset((void *)(ylookup[y]+ix0+frameoffset),dacol,ix1-ix0);
					else
					{
						vidp = (unsigned char *)(ylookup[y]+frameoffset+ix0);
						dp = ngdx*(double)ix0 + ngdy*(double)y + ngdo;
						up = ngux*(double)ix0 + nguy*(double)y + nguo;
						vp = ngvx*(double)ix0 + ngvy*(double)y + ngvo;
						rdp = 65536.0/dp; dp += ngdx2;
						dtol(   rdp,&d0);
						dtol(up*rdp,&u0); up += ngux2;
						dtol(vp*rdp,&v0); vp += ngvx2;
						rdp = 65536.0/dp;

						switch (method & (METH_MASKED | METH_TRANS))
						{
							case METH_SOLID:
								if (xmodnice && ymulnice) //both u&v texture sizes are powers of 2 :)
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[std::min(ix1 - xx, 1 << LINTERPSIZ)];
										while (vidp < vide)
										{
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											zbufoff[(intptr_t)vidp] = d0+16384; //+ hack so wall&floor sprites don't disappear
#endif
											vidp[0] = palptr[walptr[(((u0>>16)&tsizxm1)<<ltsizy) + ((v0>>16)&tsizym1)]]; //+((d0>>13)&0x3f00)];
#else
											vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										}
									}
								}
								else
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[std::min(ix1 - xx, 1 << LINTERPSIZ)];
										while (vidp < vide)
										{
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											zbufoff[(intptr_t)vidp] = d0;
#endif
											vidp[0] = palptr[walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)]]; //+((d0>>13)&0x3f00)];
#else
											vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										}
									}
								}
								break;
							case METH_MASKED:
								if (xmodnice) //both u&v texture sizes are powers of 2 :)
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[std::min(ix1 - xx, 1 << LINTERPSIZ)];
										while (vidp < vide)
										{
											dacol = walptr[(((u0>>16)&tsizxm1)*tsizy) + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											if ((dacol != 255) && (d0 <= zbufoff[(intptr_t)vidp]))
											{
												zbufoff[(intptr_t)vidp] = d0;
												vidp[0] = palptr[((int)dacol)]; //+((d0>>13)&0x3f00)];
											}
#else
											if (dacol != 255) vidp[0] = palptr[((int)dacol)]; //+((d0>>13)&0x3f00)];
#endif
#else
											if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										}
									}
								}
								else
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[std::min(ix1 - xx, 1 << LINTERPSIZ)];
										while (vidp < vide)
										{
											dacol = walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											if ((dacol != 255) && (d0 <= zbufoff[(intptr_t)vidp]))
											{
												zbufoff[(intptr_t)vidp] = d0;
												vidp[0] = palptr[((int)dacol)]; //+((d0>>13)&0x3f00)];
											}
#else
											if (dacol != 255) vidp[0] = palptr[((int)dacol)]; //+((d0>>13)&0x3f00)];
#endif
#else
											if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										}
									}
								}
								break;
							case METH_TRANS: //Transluscence #1
								for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
								{
									dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
									dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
									dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
									rdp = 65536.0/dp; vide = &vidp[std::min(ix1 - xx, 1 << LINTERPSIZ)];
									while (vidp < vide)
									{
										dacol = walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)];
										//dacol = walptr[(((u0>>16)&tsizxm1)<<ltsizy) + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
										if ((dacol != 255) && (d0 <= zbufoff[(intptr_t)vidp]))
										{
											zbufoff[(intptr_t)vidp] = d0;
											vidp[0] = transluc[(((int)vidp[0])<<8)+((int)palptr[((int)dacol)])]; //+((d0>>13)&0x3f00)])];
										}
#else
										if (dacol != 255)
											vidp[0] = transluc[(((int)vidp[0])<<8)+((int)palptr[((int)dacol)])]; //+((d0>>13)&0x3f00)])];
#endif
#else
										if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
										d0 += d1; u0 += u1; v0 += v1; vidp++;
									}
								}
								break;
							case METH_INTRANS: //Transluscence #2
								for(xx = ix0; xx < ix1; xx += (1 << LINTERPSIZ))
								{
									dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
									dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
									dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
									rdp = 65536.0/dp; vide = &vidp[std::min(ix1 - xx, 1 << LINTERPSIZ)];
									while (vidp < vide)
									{
										dacol = walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)];
										//dacol = walptr[(((u0>>16)&tsizxm1)<<ltsizy) + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
										if ((dacol != 255) && (d0 <= zbufoff[(intptr_t)vidp]))
										{
											zbufoff[(intptr_t)vidp] = d0;
											vidp[0] = transluc[((int)vidp[0])+(((int)palptr[((int)dacol)/*+((d0>>13)&0x3f00)*/])<<8)];
										}
#else
										if (dacol != 255)
											vidp[0] = transluc[((int)vidp[0])+(((int)palptr[((int)dacol)/*+((d0>>13)&0x3f00)*/])<<8)];
#endif
#else
										if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
										d0 += d1; u0 += u1; v0 += v1; vidp++;
									}
								}
								break;
						}
					}
				}
			}
		}
		i = j;
	} while (i != maxi);

	if (rendmode == rendmode_t::SolidColor)
	{
		if (method & (METH_MASKED | METH_TRANS)) //Only draw border around sprites/maskwalls
		{
			for(i=0,j=n-1;i<n;j=i,i++)
				drawline2d(px[i],py[i],px[j],py[j],31); //hopefully color index 31 is white
		}

		//ox = 0; oy = 0;
		//for(i=0;i<n;i++) { ox += px[i]; oy += py[i]; }
		//ox /= (double)n; oy /= (double)n;
		//for(i=0,j=n-1;i<n;j=i,i++) drawline2d(px[i]+(ox-px[i])*.125,py[i]+(oy-py[i])*.125,px[j]+(ox-px[j])*.125,py[j]+(oy-py[j])*.125,31);
	}
}

namespace {

	/*Init viewport boundary (must be 4 point convex loop):
	//      (px[0],py[0]).----.(px[1],py[1])
	//                  /      \
	//                /          \
	// (px[3],py[3]).--------------.(px[2],py[2])
	*/
void initmosts (const double *px, const double *py, int n)
{
	int i;
	int j;
	int k;
	int imin;

	vcnt = 1; //0 is dummy solid node

	if (n < 3)
		return;
	imin = (px[1] < px[0]);
	for(i=n-1;i>=2;i--) if (px[i] < px[imin]) imin = i;


	vsp[vcnt].x = px[imin];
	vsp[vcnt].cy[0] = vsp[vcnt].fy[0] = py[imin];
	vcnt++;
	i = imin+1; if (i >= n) i = 0;
	j = imin-1; if (j < 0) j = n-1;
	do
	{
		if (px[i] < px[j])
		{
			if ((vcnt > 1) && (px[i] <= vsp[vcnt-1].x)) vcnt--;
			vsp[vcnt].x = px[i];
			vsp[vcnt].cy[0] = py[i];
			k = j+1; if (k >= n) k = 0;
				//(px[k],py[k])
				//(px[i],?)
				//(px[j],py[j])
			vsp[vcnt].fy[0] = (px[i]-px[k])*(py[j]-py[k])/(px[j]-px[k]) + py[k];
			vcnt++;
			i++; if (i >= n) i = 0;
		}
		else if (px[j] < px[i])
		{
			if ((vcnt > 1) && (px[j] <= vsp[vcnt-1].x)) vcnt--;
			vsp[vcnt].x = px[j];
			vsp[vcnt].fy[0] = py[j];
			k = i-1; if (k < 0) k = n-1;
				//(px[k],py[k])
				//(px[j],?)
				//(px[i],py[i])
			vsp[vcnt].cy[0] = (px[j]-px[k])*(py[i]-py[k])/(px[i]-px[k]) + py[k];
			vcnt++;
			j--; if (j < 0) j = n-1;
		}
		else
		{
			if ((vcnt > 1) && (px[i] <= vsp[vcnt-1].x)) vcnt--;
			vsp[vcnt].x = px[i];
			vsp[vcnt].cy[0] = py[i];
			vsp[vcnt].fy[0] = py[j];
			vcnt++;
			i++; if (i >= n) i = 0; if (i == j) break;
			j--; if (j < 0) j = n-1;
		}
	} while (i != j);
	if (px[i] > vsp[vcnt-1].x)
	{
		vsp[vcnt].x = px[i];
		vsp[vcnt].cy[0] = vsp[vcnt].fy[0] = py[i];
		vcnt++;
	}


	for(i=0;i<vcnt;i++)
	{
		vsp[i].cy[1] = vsp[i+1].cy[0]; vsp[i].ctag = i;
		vsp[i].fy[1] = vsp[i+1].fy[0]; vsp[i].ftag = i;
		vsp[i].n = i+1; vsp[i].p = i-1;
	}
	vsp[vcnt-1].n = 0; vsp[0].p = vcnt-1;
	gtag = vcnt;

		//VSPMAX-1 is dummy empty node
	for(i=vcnt;i<VSPMAX;i++) { vsp[i].n = i+1; vsp[i].p = i-1; }
	vsp[VSPMAX-1].n = vcnt; vsp[vcnt].p = VSPMAX-1;
}

void vsdel(int i) {
	//Delete i
	const int pi = vsp[i].p;
	const int ni = vsp[i].n;
	vsp[ni].p = pi;
	vsp[pi].n = ni;

	//Add i to empty list
	vsp[i].n = vsp[VSPMAX - 1].n;
	vsp[i].p = VSPMAX - 1;
	vsp[vsp[VSPMAX - 1].n].p = i;
	vsp[VSPMAX - 1].n = i;
}

int vsinsaft (int i)
{
	//i = next element from empty list
	const int r = vsp[VSPMAX-1].n;
	vsp[vsp[r].n].p = VSPMAX - 1;
	vsp[VSPMAX-1].n = vsp[r].n;

	vsp[r] = vsp[i]; //copy i to r

	//insert r after i
	vsp[r].p = i;
	vsp[r].n = vsp[i].n;
	vsp[vsp[i].n].p = r;
	vsp[i].n = r;

	return r;
}

int testvisiblemost (float x0, float x1)
{
	int newi;

	for(int i = vsp[0].n; i; i = newi) { // TODO: Is this right?
		newi = vsp[i].n;

		if ((x0 < vsp[newi].x) && (vsp[i].x < x1) && (vsp[i].ctag >= 0)) {
			return 1;
		}
	}

	return 0;
}

void domost (float x0, float y0, float x1, float y1, int polymethod)
{
	std::array<double, 4> dpx;
	std::array<double, 4> dpy;
	float d;
	float f;
	float n;
	float t;
	float slop;
	float dx;
	float dx0;
	float dx1;
	float nx;
	float nx0;
	float ny0;
	float nx1;
	float ny1;
	std::array<float, 4> spx;
	std::array<float, 4> spy;
	std::array<float, 2> cy;
	std::array<float, 2> cv;
	int i;
	int j;
	int k;
	int z;
	int ni;
	int vcnt{ 0 };
	int scnt;
	int newi;
	int dir;
	std::array<int, 4> spt;

#ifdef DEBUGGINGAIDS
	polymostcallcounts.domost++;
#endif
	polymethod |= METH_LAYERS;

	if (x0 < x1)
	{
		dir = 1; //clip dmost (floor)
		y0 -= .01; y1 -= .01;
	}
	else
	{
		if (x0 == x1)
			return;
		
		std::swap(x0, x1);
		std::swap(y0, y1);
		dir = 0; //clip umost (ceiling)
		//y0 += .01; y1 += .01; //necessary?
	}

	slop = (y1-y0)/(x1-x0);
	
	for(i=vsp[0].n;i;i=newi)
	{
		newi = vsp[i].n;
		nx0 = vsp[i].x;
		nx1 = vsp[newi].x;
		if ((x0 >= nx1) || (nx0 >= x1) || (vsp[i].ctag <= 0)) continue;
		dx = nx1-nx0;
		cy[0] = vsp[i].cy[0];
		cv[0] = vsp[i].cy[1]-cy[0];
		cy[1] = vsp[i].fy[0];
		cv[1] = vsp[i].fy[1]-cy[1];

		scnt = 0;

			//Test if left edge requires split (x0,y0) (nx0,cy(0)),<dx,cv(0)>
		if ((x0 > nx0) && (x0 < nx1))
		{
			t = (x0-nx0)*cv[dir] - (y0-cy[dir])*dx;
			if (((!dir) && (t < 0)) || ((dir) && (t > 0))) {
				spx[scnt] = x0;
				spy[scnt] = y0;
				spt[scnt] = -1; 
				scnt++;
			}
		}

			//Test for intersection on umost (j == 0) and dmost (j == 1)
		for(j=0;j<2;j++)
		{
			d = (y0-y1)*dx - (x0-x1)*cv[j];
			n = (y0-cy[j])*dx - (x0-nx0)*cv[j];
			if ((fabs(n) <= fabs(d)) && (d*n >= 0) && (d != 0))
			{
				t = n/d;
				nx = (x1-x0)*t + x0;
				if ((nx > nx0) && (nx < nx1))
				{
					spx[scnt] = nx;
					spy[scnt] = (y1-y0)*t + y0;
					spt[scnt] = j;
					scnt++;
				}
			}
		}

			//Nice hack to avoid full sort later :)
		if ((scnt >= 2) && (spx[scnt-1] < spx[scnt-2]))
		{
			std::swap(spx[scnt - 1], spx[scnt - 2]);
			std::swap(spy[scnt - 1], spy[scnt - 2]);
			std::swap(spt[scnt - 1], spt[scnt - 2]);
		}

			//Test if right edge requires split
		if ((x1 > nx0) && (x1 < nx1))
		{
			t = (x1-nx0)*cv[dir] - (y1-cy[dir])*dx;
			if (((!dir) && (t < 0)) || ((dir) && (t > 0))) {
				spx[scnt] = x1;
				spy[scnt] = y1;
				spt[scnt] = -1;
				scnt++;
			}
		}

		vsp[i].tag = vsp[newi].tag = -1;
		for(z=0;z<=scnt;z++,i=vcnt)
		{
			if (z < scnt)
			{
				vcnt = vsinsaft(i);
				t = (spx[z]-nx0)/dx;
				vsp[i].cy[1] = t*cv[0] + cy[0];
				vsp[i].fy[1] = t*cv[1] + cy[1];
				vsp[vcnt].x = spx[z];
				vsp[vcnt].cy[0] = vsp[i].cy[1];
				vsp[vcnt].fy[0] = vsp[i].fy[1];
				vsp[vcnt].tag = spt[z];
			}

			ni = vsp[i].n; if (!ni) continue; //this 'if' fixes many bugs!
			dx0 = vsp[i].x; if (x0 > dx0) continue;
			dx1 = vsp[ni].x; if (x1 < dx1) continue;
			ny0 = (dx0-x0)*slop + y0;
			ny1 = (dx1-x0)*slop + y0;

				//      dx0           dx1
				//       ~             ~
				//----------------------------
				//     t0+=0         t1+=0
				//   vsp[i].cy[0]  vsp[i].cy[1]
				//============================
				//     t0+=1         t1+=3
				//============================
				//   vsp[i].fy[0]    vsp[i].fy[1]
				//     t0+=2         t1+=6
				//
				//     ny0 ?         ny1 ?

			k = 1+3;
			if ((vsp[i].tag == 0) || (ny0 <= vsp[i].cy[0]+.01)) k--;
			if ((vsp[i].tag == 1) || (ny0 >= vsp[i].fy[0]-.01)) k++;
			if ((vsp[ni].tag == 0) || (ny1 <= vsp[i].cy[1]+.01)) k -= 3;
			if ((vsp[ni].tag == 1) || (ny1 >= vsp[i].fy[1]-.01)) k += 3;

			if (!dir)
			{
				switch(k)
				{
					case 1:
					case 2:
						dpx[0] = dx0;
						dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1;
						dpy[1] = vsp[i].cy[1];
						dpx[2] = dx0;
						dpy[2] = ny0;
						drawpoly(dpx, dpy, 3, polymethod);
						vsp[i].cy[0] = ny0;
						vsp[i].ctag = gtag;
						break;
					case 3:
					case 6:
						dpx[0] = dx0;
						dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1;
						dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1;
						dpy[2] = ny1;
						drawpoly(dpx, dpy, 3, polymethod);
						vsp[i].cy[1] = ny1;
						vsp[i].ctag = gtag;
						break;
					case 4:
					case 5:
					case 7:
						dpx[0] = dx0;
						dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1;
						dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1;
						dpy[2] = ny1;
						dpx[3] = dx0;
						dpy[3] = ny0;
						drawpoly(dpx, dpy, 4, polymethod);
						vsp[i].cy[0] = ny0;
						vsp[i].cy[1] = ny1;
						vsp[i].ctag = gtag;
						break;
					case 8:
						dpx[0] = dx0;
						dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1;
						dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1;
						dpy[2] = vsp[i].fy[1];
						dpx[3] = dx0;
						dpy[3] = vsp[i].fy[0];
						drawpoly(dpx, dpy, 4, polymethod);
						vsp[i].ctag = vsp[i].ftag = -1;
						break;
					default:
						break;
				}
			}
			else
			{
				switch(k)
				{
					case 7:
					case 6:
						dpx[0] = dx0;
						dpy[0] = ny0;
						dpx[1] = dx1;
						dpy[1] = vsp[i].fy[1];
						dpx[2] = dx0;
						dpy[2] = vsp[i].fy[0];
						drawpoly(dpx, dpy, 3, polymethod);
						vsp[i].fy[0] = ny0;
						vsp[i].ftag = gtag;
						break;
					case 5:
					case 2:
						dpx[0] = dx0;
						dpy[0] = vsp[i].fy[0];
						dpx[1] = dx1;
						dpy[1] = ny1;
						dpx[2] = dx1;
						dpy[2] = vsp[i].fy[1];
						drawpoly(dpx, dpy, 3, polymethod);
						vsp[i].fy[1] = ny1;
						vsp[i].ftag = gtag;
						break;
					case 4:
					case 3:
					case 1:
						dpx[0] = dx0;
						dpy[0] = ny0;
						dpx[1] = dx1;
						dpy[1] = ny1;
						dpx[2] = dx1;
						dpy[2] = vsp[i].fy[1];
						dpx[3] = dx0;
						dpy[3] = vsp[i].fy[0];
						drawpoly(dpx, dpy, 4, polymethod);
						vsp[i].fy[0] = ny0;
						vsp[i].fy[1] = ny1;
						vsp[i].ftag = gtag;
						break;
					case 0:
						dpx[0] = dx0;
						dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1;
						dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1;
						dpy[2] = vsp[i].fy[1];
						dpx[3] = dx0;
						dpy[3] = vsp[i].fy[0];
						drawpoly(dpx, dpy, 4, polymethod);
						vsp[i].ctag = vsp[i].ftag = -1;
						break;
					default:
						break;
				}
			}
		}
	}

	gtag++;

		//Combine neighboring vertical strips with matching collinear top&bottom edges
		//This prevents x-splits from propagating through the entire scan
	i = vsp[0].n;
	
	while (i) {
		ni = vsp[i].n;
		if ((vsp[i].cy[0] >= vsp[i].fy[0]) && (vsp[i].cy[1] >= vsp[i].fy[1])) { vsp[i].ctag = vsp[i].ftag = -1; }
		if ((vsp[i].ctag == vsp[ni].ctag) && (vsp[i].ftag == vsp[ni].ftag)) {
			vsp[i].cy[1] = vsp[ni].cy[1];
			vsp[i].fy[1] = vsp[ni].fy[1];
			vsdel(ni);
		}
		else
			i = ni;
	}
}

void polymost_scansector (int sectnum);

void polymost_drawalls (int bunch)
{
	sectortype* nextsec;
	walltype* wal;
	walltype* wal2;
	walltype* nwal;
	double ox;
	double oy;
	double oz;
	double ox2;
	double oy2;
	std::array<double, 3> px;
	std::array<double, 3> py;
	std::array<double, 3> dd;
	std::array<double, 3> uu;
	std::array<double, 3> vv;
	double fx;
	double fy;
	double x0;
	double x1;
	double cy0;
	double cy1;
	double fy0;
	double fy1;
	double xp0;
	double yp0;
	double xp1;
	double yp1;
	double ryp0;
	double ryp1;
	double nx0;
	double ny0;
	double nx1;
	double ny1;
	double t;
	double r;
	double t0;
	double t1;
	double ocy0;
	double ocy1;
	double ofy0;
	double ofy1;
	double oxp0;
	double oyp0;
	std::array<double, 4> ft;
	double oguo;
	double ogux;
	double oguy;
	int i;
	int x;
	int y;
	int z;
	int wallnum;
	int nextsectnum;
	int domostmethod;

#ifdef DEBUGGINGAIDS
	polymostcallcounts.drawalls++;
#endif

	const int sectnum = thesector[bunchfirst[bunch]];
	sectortype* sec = &g_sector[sectnum];

#if USE_OPENGL
	gfogpalnum = sec->floorpal;
	gfogdensity = gvisibility*((float)((unsigned char)(sec->visibility+16)) / 255.F);
#endif

		//DRAW WALLS SECTION!
	for(z=bunchfirst[bunch];z>=0;z=p2[z])
	{
		wallnum = thewall[z]; wal = &wall[wallnum]; wal2 = &wall[wal->point2];
		nextsectnum = wal->nextsector; nextsec = &g_sector[nextsectnum];

			//Offset&Rotate 3D coordinates to screen 3D space
		x = wal->pt.x-globalpos.x;
		y = wal->pt.y-globalpos.y;
		xp0 = (double)y*gcosang  - (double)x*gsinang;
		yp0 = (double)x*gcosang2 + (double)y*gsinang2;
		x = wal2->pt.x-globalpos.x;
		y = wal2->pt.y-globalpos.y;
		xp1 = (double)y*gcosang  - (double)x*gsinang;
		yp1 = (double)x*gcosang2 + (double)y*gsinang2;

		oxp0 = xp0;
		oyp0 = yp0;

			//Clip to close parallel-screen plane
		if (yp0 < SCISDIST)
		{
			if (yp1 < SCISDIST)
				continue;
			
			t0 = (SCISDIST-yp0)/(yp1-yp0);
			xp0 = (xp1-xp0)*t0+xp0;
			yp0 = SCISDIST;
			nx0 = (wal2->pt.x-wal->pt.x)*t0+wal->pt.x;
			ny0 = (wal2->pt.y-wal->pt.y)*t0+wal->pt.y;
		}
		else {
			t0 = 0.F;
			nx0 = wal->pt.x;
			ny0 = wal->pt.y;
		}

		if (yp1 < SCISDIST)
		{
			t1 = (SCISDIST-oyp0)/(yp1-oyp0);
			xp1 = (xp1-oxp0)*t1+oxp0;
			yp1 = SCISDIST;
			nx1 = (wal2->pt.x-wal->pt.x)*t1+wal->pt.x;
			ny1 = (wal2->pt.y-wal->pt.y)*t1+wal->pt.y;
		}
		else {
			t1 = 1.F;
			nx1 = wal2->pt.x;
			ny1 = wal2->pt.y;
		}

		ryp0 = 1.F/yp0;
		ryp1 = 1.F/yp1;

			//Generate screen coordinates for front side of wall
		x0 = ghalfx*xp0*ryp0 + ghalfx;
		x1 = ghalfx*xp1*ryp1 + ghalfx;
		
		if (x1 <= x0)
			continue;

		ryp0 *= gyxscale;
		ryp1 *= gyxscale;

		auto cfz = getzsofslope(sectnum,(int)nx0,(int)ny0);
		cy0 = ((float)(cfz.ceilz - globalpos.z))*ryp0 + ghoriz;
		fy0 = ((float)(cfz.floorz - globalpos.z))*ryp0 + ghoriz;
		cfz = getzsofslope(sectnum,(int)nx1,(int)ny1);
		cy1 = ((float)(cfz.ceilz - globalpos.z))*ryp1 + ghoriz;
		fy1 = ((float)(cfz.floorz - globalpos.z))*ryp1 + ghoriz;

		domostmethod = 0;
		globalpicnum = sec->floorpicnum;
		globalshade = sec->floorshade;
		globalpal = (int)((unsigned char)sec->floorpal);
		globalorientation = sec->floorstat;
		if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,sectnum);
		if (!(globalorientation&1))
		{
				//(singlobalang/-16384*(sx-ghalfx) + 0*(sy-ghoriz) + (cosviewingrangeglobalang/16384)*ghalfx)*d + globalpos.x    = u*16
				//(cosglobalang/ 16384*(sx-ghalfx) + 0*(sy-ghoriz) + (sinviewingrangeglobalang/16384)*ghalfx)*d + globalpos.y    = v*16
				//(                  0*(sx-ghalfx) + 1*(sy-ghoriz) + (                             0)*ghalfx)*d + globalpos.z/16 = (sec->floorz/16)
			if (!(globalorientation&64))
				{ ft[0] = globalpos.x; ft[1] = globalpos.y; ft[2] = cosglobalang; ft[3] = singlobalang; }
			else
			{
					//relative alignment
				fx = (double)(wall[wall[sec->wallptr].point2].pt.x-wall[sec->wallptr].pt.x);
				fy = (double)(wall[wall[sec->wallptr].point2].pt.y-wall[sec->wallptr].pt.y);
				r = 1.0/std::hypot(fx, fy);
				fx *= r;
				fy *= r;
				ft[2] = cosglobalang*fx + singlobalang*fy;
				ft[3] = singlobalang*fx - cosglobalang*fy;
				ft[0] = ((double)(globalpos.x-wall[sec->wallptr].pt.x))*fx + ((double)(globalpos.y-wall[sec->wallptr].pt.y))*fy;
				ft[1] = ((double)(globalpos.y-wall[sec->wallptr].pt.y))*fx - ((double)(globalpos.x-wall[sec->wallptr].pt.x))*fy;
				if (!(globalorientation&4)) globalorientation ^= 32; else globalorientation ^= 16;
			}
			gdx = 0;
			gdy = gxyaspect; if (!(globalorientation&2)) gdy /= (double)(sec->floorz-globalpos.z);
			gdo = -ghoriz*gdy;
			if (globalorientation&8) { ft[0] /= 8; ft[1] /= -8; ft[2] /= 2097152; ft[3] /= 2097152; }
									  else { ft[0] /= 16; ft[1] /= -16; ft[2] /= 4194304; ft[3] /= 4194304; }
			gux = (double)ft[3]*((double)viewingrange)/-65536.0;
			gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
			guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
			guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
			guo += (double)(ft[2]-gux)*ghalfx;
			gvo -= (double)(ft[3]+gvx)*ghalfx;

				//Texture flipping
			if (globalorientation&4)
			{
				std::swap(gux, gvx);
				std::swap(guy, gvy);
				std::swap(guo, gvo);
			}
			if (globalorientation&16) {
				gux = -gux;
				guy = -guy;
				guo = -guo;
			}

			if (globalorientation&32) {
				gvx = -gvx;
				gvy = -gvy;
				gvo = -gvo;
			}

				//Texture panning
			fx = (float)sec->floorxpanning*((float)(1<<(picsiz[globalpicnum]&15)))/256.0;
			fy = (float)sec->floorypanning*((float)(1<<(picsiz[globalpicnum]>>4)))/256.0;
			if ((globalorientation&(2+64)) == (2+64)) //Hack for panning for slopes w/ relative alignment
			{
				r = (float)sec->floorheinum / 4096.0; r = 1.0/std::hypot(r, 1);
				if (!(globalorientation&4)) fy *= r; else fx *= r;
			}
			guy += gdy*fx; guo += gdo*fx;
			gvy += gdy*fy; gvo += gdo*fy;

			if (globalorientation&2) //slopes
			{
				px[0] = x0;
				py[0] = ryp0 + ghoriz;
				px[1] = x1;
				py[1] = ryp1 + ghoriz;

					//Pick some point guaranteed to be not collinear to the 1st two points
				ox = nx0 + (ny1 - ny0);
				oy = ny0 + (nx0 - nx1);
				ox2 = (double)(oy - globalpos.y) * gcosang  - (double)(ox - globalpos.x) * gsinang;
				oy2 = (double)(ox - globalpos.x) * gcosang2 + (double)(oy - globalpos.y) * gsinang2;
				oy2 = 1.0 / oy2;
				px[2] = ghalfx * ox2 * oy2 + ghalfx;
				oy2 *= gyxscale;
				py[2] = oy2 + ghoriz;

				for(i=0;i<3;i++)
				{
					dd[i] = px[i] * gdx + py[i] * gdy + gdo;
					uu[i] = px[i] * gux + py[i] * guy + guo;
					vv[i] = px[i] * gvx + py[i] * gvy + gvo;
				}

				py[0] = fy0;
				py[1] = fy1;
				py[2] = (getflorzofslope(sectnum,(int)ox,(int)oy)-globalpos.z)*oy2 + ghoriz;

				ox = py[1]-py[2];
				oy = py[2]-py[0];
				oz = py[0]-py[1];
				r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
				gdx = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				gux = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvx = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				ox = px[2]-px[1];
				oy = px[0]-px[2];
				oz = px[1]-px[0];
				gdy = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				guy = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvy = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				gdo = dd[0] - px[0]*gdx - py[0]*gdy;
				guo = uu[0] - px[0]*gux - py[0]*guy;
				gvo = vv[0] - px[0]*gvx - py[0]*gvy;

				if (globalorientation&64) //Hack for relative alignment on slopes
				{
					r = (float)sec->floorheinum / 4096.0;
					r = std::hypot(r, 1);
					if (!(globalorientation&4)) {
						gvx *= r;
						gvy *= r;
						gvo *= r;
					}
					else {
						gux *= r;
						guy *= r;
						guo *= r;
					}
				}
			}
			domostmethod = (globalorientation>>7)&3;
			if (globalpos.z >= getflorzofslope(sectnum,globalpos.x,globalpos.y)) domostmethod = -1; //Back-face culling
			domost(x0,fy0,x1,fy1,domostmethod); //flor
		}
		else if ((nextsectnum < 0) || (!(g_sector[nextsectnum].floorstat&1)))
		{
				//Parallaxing sky... hacked for Ken's mountain texture; paper-sky only :/
#if USE_OPENGL
			const float tempfogdensity{ gfogdensity };
			if (rendmode == rendmode_t::OpenGL)
			{
				gfogdensity = 0.F;

					//Use clamping for tiled sky textures
				for(i=(1<<pskybits)-1;i>0;i--)
					if (pskyoff[i] != pskyoff[i-1])
						{ domostmethod = METH_CLAMPED; break; }
			}
			if (bpp == 8 || !usehightile || !hicfindsubst(globalpicnum,globalpal,1))
#endif
			{
				dd[0] = (float)xdimen*.0000001; //Adjust sky depth based on screen size!
				t = (double)((1<<(picsiz[globalpicnum]&15))<<pskybits);
				vv[1] = dd[0]*((double)xdimscale*(double)viewingrange)/(65536.0*65536.0);
				vv[0] = dd[0]*((double)((tilesizy[globalpicnum]>>1)+parallaxyoffs)) - vv[1]*ghoriz;
				i = (1<<(picsiz[globalpicnum]>>4)); if (i != tilesizy[globalpicnum]) i += i;
				vv[0] += dd[0]*((double)sec->floorypanning)*((double)i)/256.0;


					//Hack to draw black rectangle below sky when looking down...
				gdx = 0;
				gdy = gxyaspect / 262144.0;
				gdo = -ghoriz*gdy;
				gux = 0;
				guy = 0;
				guo = 0;
				gvx = 0;
				gvy = (double)(tilesizy[globalpicnum]-1)*gdy;
				gvo = (double)(tilesizy[globalpicnum-1])*gdo;
				oy = (((double)tilesizy[globalpicnum])*dd[0]-vv[0])/vv[1];
				if ((oy > fy0) && (oy > fy1)) domost(x0,oy,x1,oy,domostmethod);
				else if ((oy > fy0) != (oy > fy1))
				{     //  fy0                      fy1
						//     \                    /
						//oy----------      oy----------
						//        \              /
						//         fy1        fy0
					ox = (oy-fy0)*(x1-x0)/(fy1-fy0) + x0;
					if (oy > fy0) { domost(x0,oy,ox,oy,domostmethod); domost(ox,oy,x1,fy1,domostmethod); }
							  else { domost(x0,fy0,ox,oy,domostmethod); domost(ox,oy,x1,oy,domostmethod); }
				} else domost(x0,fy0,x1,fy1,domostmethod);


				gdx = 0;
				gdy = 0;
				gdo = dd[0];
				gux = gdo*(t*((double)xdimscale)*((double)yxaspect)*((double)viewingrange))/(16384.0*65536.0*65536.0*5.0*1024.0);
				guy = 0; //guo calculated later
				gvx = 0;
				gvy = vv[1];
				gvo = vv[0];

				i = globalpicnum;
				r = (fy1-fy0)/(x1-x0); //slope of line
				oy = ((double)viewingrange)/(ghalfx*256.0);
				oz = 1/oy;

				y = ((((int)((x0-ghalfx)*oy))+globalang)>>(11-pskybits));
				fx = x0;
				do
				{
					globalpicnum = pskyoff[y&((1<<pskybits)-1)]+i;
					guo = gdo*(t*((double)(globalang-(y<<(11-pskybits))))/2048.0 + (double)sec->floorxpanning) - gux*ghalfx;
					y++;
					ox = fx;
					fx = ((double)((y<<(11-pskybits))-globalang))*oz+ghalfx;
					if (fx > x1) {
						fx = x1;
						i = -1;
					}

					domost(ox,(ox-x0)*r+fy0,fx,(fx-x0)*r+fy0,domostmethod); //flor
				} while (i >= 0);

			}
#if USE_OPENGL
			else  //NOTE: code copied from ceiling code... lots of duplicated stuff :/
			{     //Skybox code for parallax ceiling!
				double _xp0;
				double _yp0;
				double _xp1;
				double _yp1;
				double _oxp0;
				double _oyp0;
				double _t0;
				double _t1;
				double _nx0;
				double _ny0;
				double _nx1;
				double _ny1;
				double _ryp0;
				double _ryp1;
				double _x0;
				double _x1;
				double _cy0;
				double _fy0;
				double _cy1;
				double _fy1;
				double _ox0;
				double _ox1;
				double nfy0;
				double nfy1;
				static constexpr int skywalx[4] = { -512,512,512,-512 };
				static constexpr int skywaly[4] = { -512,-512,512,512 };

				std::ignore = _nx0;
				std::ignore = _ny0;
				std::ignore = _nx1;
				std::ignore = _ny1;
				domostmethod = METH_CLAMPED;

				for(i=0;i<4;i++)
				{
					x = skywalx[i&3]; y = skywaly[i&3];
					_xp0 = (double)y*gcosang  - (double)x*gsinang;
					_yp0 = (double)x*gcosang2 + (double)y*gsinang2;
					x = skywalx[(i+1)&3]; y = skywaly[(i+1)&3];
					_xp1 = (double)y*gcosang  - (double)x*gsinang;
					_yp1 = (double)x*gcosang2 + (double)y*gsinang2;

					_oxp0 = _xp0; _oyp0 = _yp0;

						//Clip to close parallel-screen plane
					if (_yp0 < SCISDIST)
					{
						if (_yp1 < SCISDIST) continue;
						_t0 = (SCISDIST-_yp0)/(_yp1-_yp0);
						_xp0 = (_xp1-_xp0)*_t0+_xp0;
						_yp0 = SCISDIST;
						_nx0 = (skywalx[(i+1)&3]-skywalx[i&3])*_t0+skywalx[i&3];
						_ny0 = (skywaly[(i+1)&3]-skywaly[i&3])*_t0+skywaly[i&3];
					}
					else { _t0 = 0.F; _nx0 = skywalx[i&3]; _ny0 = skywaly[i&3]; }
					if (_yp1 < SCISDIST)
					{
						_t1 = (SCISDIST-_oyp0)/(_yp1-_oyp0); _xp1 = (_xp1-_oxp0)*_t1+_oxp0; _yp1 = SCISDIST;
						_nx1 = (skywalx[(i+1)&3]-skywalx[i&3])*_t1+skywalx[i&3];
						_ny1 = (skywaly[(i+1)&3]-skywaly[i&3])*_t1+skywaly[i&3];
					}
					else { _t1 = 1.F; _nx1 = skywalx[(i+1)&3]; _ny1 = skywaly[(i+1)&3]; }

					_ryp0 = 1.F/_yp0; _ryp1 = 1.F/_yp1;

						//Generate screen coordinates for front side of wall
					_x0 = ghalfx*_xp0*_ryp0 + ghalfx;
					_x1 = ghalfx*_xp1*_ryp1 + ghalfx;
					if (_x1 <= _x0) continue;
					if ((_x0 >= x1) || (x0 >= _x1)) continue;

					_ryp0 *= gyxscale; _ryp1 *= gyxscale;

					_cy0 = -8192.F*_ryp0 + ghoriz;
					_fy0 =  8192.F*_ryp0 + ghoriz;
					_cy1 = -8192.F*_ryp1 + ghoriz;
					_fy1 =  8192.F*_ryp1 + ghoriz;

					_ox0 = _x0; _ox1 = _x1;

						//Make sure: x0<=_x0<_x1<=_x1
					nfy0 = fy0; nfy1 = fy1;
					if (_x0 < x0)
					{
						t = (x0-_x0)/(_x1-_x0);
						_cy0 += (_cy1-_cy0)*t;
						_fy0 += (_fy1-_fy0)*t;
						_x0 = x0;
					}
					else if (_x0 > x0) nfy0 += (_x0-x0)*(fy1-fy0)/(x1-x0);
					if (_x1 > x1)
					{
						t = (x1-_x1)/(_x1-_x0);
						_cy1 += (_cy1-_cy0)*t;
						_fy1 += (_fy1-_fy0)*t;
						_x1 = x1;
					}
					else if (_x1 < x1) nfy1 += (_x1-x1)*(fy1-fy0)/(x1-x0);

						//   (skybox floor)
						//(_x0,_fy0)-(_x1,_fy1)
						//   (skybox wall)
						//(_x0,_cy0)-(_x1,_cy1)
						//   (skybox ceiling)
						//(_x0,nfy0)-(_x1,nfy1)

						//ceiling of skybox
					ft[0] = 512/16; ft[1] = 512/-16;
					ft[2] = ((float)cosglobalang)*(1.F/2147483648.F);
					ft[3] = ((float)singlobalang)*(1.F/2147483648.F);
					gdx = 0;
					gdy = gxyaspect*(1.F/4194304.F);
					gdo = -ghoriz*gdy;
					gux = (double)ft[3]*((double)viewingrange)/-65536.0;
					gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
					guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
					guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
					guo += (double)(ft[2]-gux)*ghalfx;
					gvo -= (double)(ft[3]+gvx)*ghalfx;
					gvx = -gvx;
					gvy = -gvy;
					gvo = -gvo; //y-flip skybox floor

					drawingskybox = 6; //ceiling/5th texture/index 4 of skybox
					if ((_fy0 > nfy0) && (_fy1 > nfy1)) domost(_x0,_fy0,_x1,_fy1,domostmethod);
					else if ((_fy0 > nfy0) != (_fy1 > nfy1))
					{
							//(ox,oy) is intersection of: (_x0,_cy0)-(_x1,_cy1)
							//                            (_x0,nfy0)-(_x1,nfy1)
							//ox = _x0 + (_x1-_x0)*t
							//oy = _cy0 + (_cy1-_cy0)*t
							//oy = nfy0 + (nfy1-nfy0)*t
						t = (_fy0-nfy0)/(nfy1-nfy0-_fy1+_fy0);
						ox = _x0 + (_x1-_x0)*t;
						oy = _fy0 + (_fy1-_fy0)*t;
						if (nfy0 > _fy0) { domost(_x0,nfy0,ox,oy,domostmethod); domost(ox,oy,_x1,_fy1,domostmethod); }
										else { domost(_x0,_fy0,ox,oy,domostmethod); domost(ox,oy,_x1,nfy1,domostmethod); }
					} else domost(_x0,nfy0,_x1,nfy1,domostmethod);

						//wall of skybox
					drawingskybox = i+1; //i+1th texture/index i of skybox
					gdx = (_ryp0-_ryp1)*gxyaspect*(1.F/512.F) / (_ox0-_ox1);
					gdy = 0;
					gdo = _ryp0*gxyaspect*(1.F/512.F) - gdx*_ox0;
					gux = (_t0*_ryp0 - _t1*_ryp1)*gxyaspect*(64.F/512.F) / (_ox0-_ox1);
					guo = _t0*_ryp0*gxyaspect*(64.F/512.F) - gux*_ox0;
					guy = 0;
					_t0 = -8192.0*_ryp0 + ghoriz;
					_t1 = -8192.0*_ryp1 + ghoriz;
					t = ((gdx*_ox0 + gdo)*8.F) / ((_ox1-_ox0) * _ryp0 * 2048.F);
					gvx = (_t0-_t1)*t;
					gvy = (_ox1-_ox0)*t;
					gvo = -gvx*_ox0 - gvy*_t0;
					if ((_cy0 > nfy0) && (_cy1 > nfy1)) domost(_x0,_cy0,_x1,_cy1,domostmethod);
					else if ((_cy0 > nfy0) != (_cy1 > nfy1))
					{
							//(ox,oy) is intersection of: (_x0,_fy0)-(_x1,_fy1)
							//                            (_x0,nfy0)-(_x1,nfy1)
							//ox = _x0 + (_x1-_x0)*t
							//oy = _fy0 + (_fy1-_fy0)*t
							//oy = nfy0 + (nfy1-nfy0)*t
						t = (_cy0-nfy0)/(nfy1-nfy0-_cy1+_cy0);
						ox = _x0 + (_x1-_x0)*t;
						oy = _cy0 + (_cy1-_cy0)*t;
						if (nfy0 > _cy0) { domost(_x0,nfy0,ox,oy,domostmethod); domost(ox,oy,_x1,_cy1,domostmethod); }
										else { domost(_x0,_cy0,ox,oy,domostmethod); domost(ox,oy,_x1,nfy1,domostmethod); }
					} else domost(_x0,nfy0,_x1,nfy1,domostmethod);
				}

					//Floor of skybox
				drawingskybox = 5; //floor/6th texture/index 5 of skybox
				ft[0] = 512/16; ft[1] = -512/-16;
				ft[2] = ((float)cosglobalang)*(1.F/2147483648.F);
				ft[3] = ((float)singlobalang)*(1.F/2147483648.F);
				gdx = 0;
				gdy = gxyaspect*(-1.F/4194304.F);
				gdo = -ghoriz*gdy;
				gux = (double)ft[3]*((double)viewingrange)/-65536.0;
				gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
				guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
				guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
				guo += (double)(ft[2]-gux)*ghalfx;
				gvo -= (double)(ft[3]+gvx)*ghalfx;
				domost(x0,fy0,x1,fy1,domostmethod);

				drawingskybox = 0;
			}
			if (rendmode == rendmode_t::OpenGL)
			{
				gfogdensity = tempfogdensity;
			}
#endif
		}

		domostmethod = 0;
		globalpicnum = sec->ceilingpicnum; globalshade = sec->ceilingshade; globalpal = (int)((unsigned char)sec->ceilingpal);
		globalorientation = sec->ceilingstat;
		if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,sectnum);
		if (!(globalorientation&1))
		{
			if (!(globalorientation&64))
				{ ft[0] = globalpos.x; ft[1] = globalpos.y; ft[2] = cosglobalang; ft[3] = singlobalang; }
			else
			{
					//relative alignment
				fx = (double)(wall[wall[sec->wallptr].point2].pt.x-wall[sec->wallptr].pt.x);
				fy = (double)(wall[wall[sec->wallptr].point2].pt.y-wall[sec->wallptr].pt.y);
				r = 1.0/std::hypot(fx, fy);
				fx *= r;
				fy *= r;
				ft[2] = cosglobalang*fx + singlobalang*fy;
				ft[3] = singlobalang*fx - cosglobalang*fy;
				ft[0] = ((double)(globalpos.x-wall[sec->wallptr].pt.x))*fx + ((double)(globalpos.y-wall[sec->wallptr].pt.y))*fy;
				ft[1] = ((double)(globalpos.y-wall[sec->wallptr].pt.y))*fx - ((double)(globalpos.x-wall[sec->wallptr].pt.x))*fy;
				if (!(globalorientation&4)) globalorientation ^= 32; else globalorientation ^= 16;
			}
			gdx = 0;
			gdy = gxyaspect;
			if (!(globalorientation&2)) gdy /= (double)(sec->ceilingz-globalpos.z);
			gdo = -ghoriz*gdy;
			if (globalorientation&8) { ft[0] /= 8; ft[1] /= -8; ft[2] /= 2097152; ft[3] /= 2097152; }
									  else { ft[0] /= 16; ft[1] /= -16; ft[2] /= 4194304; ft[3] /= 4194304; }
			gux = (double)ft[3]*((double)viewingrange)/-65536.0;
			gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
			guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
			guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
			guo += (double)(ft[2]-gux)*ghalfx;
			gvo -= (double)(ft[3]+gvx)*ghalfx;

				//Texture flipping
			if (globalorientation&4)
			{
				std::swap(gux, gvx);
				std::swap(guy, gvy);
				std::swap(guo, gvo);
			}

			if (globalorientation&16) {
				gux = -gux;
				guy = -guy;
				guo = -guo;
			}

			if (globalorientation&32) {
				gvx = -gvx;
				gvy = -gvy;
				gvo = -gvo;
			}

				//Texture panning
			fx = (float)sec->ceilingxpanning*((float)(1<<(picsiz[globalpicnum]&15)))/256.0;
			fy = (float)sec->ceilingypanning*((float)(1<<(picsiz[globalpicnum]>>4)))/256.0;
			if ((globalorientation&(2+64)) == (2+64)) //Hack for panning for slopes w/ relative alignment
			{
				r = (float)sec->ceilingheinum / 4096.0;
				r = 1.0/std::hypot(r, 1);
				if (!(globalorientation&4)) fy *= r; else fx *= r;
			}
			guy += gdy*fx;
			guo += gdo*fx;
			gvy += gdy*fy;
			gvo += gdo*fy;

			if (globalorientation&2) //slopes
			{
				px[0] = x0;
				py[0] = ryp0 + ghoriz;
				px[1] = x1;
				py[1] = ryp1 + ghoriz;

					//Pick some point guaranteed to be not collinear to the 1st two points
				ox = nx0 + (ny1-ny0);
				oy = ny0 + (nx0-nx1);
				ox2 = (double)(oy-globalpos.y)*gcosang  - (double)(ox-globalpos.x)*gsinang ;
				oy2 = (double)(ox-globalpos.x)*gcosang2 + (double)(oy-globalpos.y)*gsinang2;
				oy2 = 1.0/oy2;
				px[2] = ghalfx*ox2*oy2 + ghalfx; oy2 *= gyxscale;
				py[2] = oy2 + ghoriz;

				for(i=0;i<3;i++)
				{
					dd[i] = px[i]*gdx + py[i]*gdy + gdo;
					uu[i] = px[i]*gux + py[i]*guy + guo;
					vv[i] = px[i]*gvx + py[i]*gvy + gvo;
				}

				py[0] = cy0;
				py[1] = cy1;
				py[2] = (getceilzofslope(sectnum,(int)ox,(int)oy)-globalpos.z)*oy2 + ghoriz;

				ox = py[1]-py[2];
				oy = py[2]-py[0];
				oz = py[0]-py[1];
				r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
				gdx = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				gux = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvx = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				ox = px[2]-px[1];
				oy = px[0]-px[2];
				oz = px[1]-px[0];
				gdy = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				guy = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvy = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				gdo = dd[0] - px[0]*gdx - py[0]*gdy;
				guo = uu[0] - px[0]*gux - py[0]*guy;
				gvo = vv[0] - px[0]*gvx - py[0]*gvy;

				if (globalorientation&64) //Hack for relative alignment on slopes
				{
					r = (float)sec->ceilingheinum / 4096.0;
					r = std::hypot(r, 1);
					if (!(globalorientation&4)) {
						gvx *= r;
						gvy *= r;
						gvo *= r;
					}
					else {
						gux *= r;
						guy *= r;
						guo *= r;
					}
				}
			}
			domostmethod = (globalorientation>>7)&3;
			if (globalpos.z <= getceilzofslope(sectnum,globalpos.x,globalpos.y)) domostmethod = -1; //Back-face culling
			domost(x1,cy1,x0,cy0,domostmethod); //ceil
		}
		else if ((nextsectnum < 0) || (!(g_sector[nextsectnum].ceilingstat&1)))
		{
#if USE_OPENGL
			const float tempfogdensity{ gfogdensity };
			if (rendmode == rendmode_t::OpenGL)
			{
				gfogdensity = 0.F;

					//Use clamping for tiled sky textures
				for(i=(1<<pskybits)-1;i>0;i--)
					if (pskyoff[i] != pskyoff[i-1])
						{ domostmethod = METH_CLAMPED; break; }
			}
				//Parallaxing sky...
			if (bpp == 8 || !usehightile || !hicfindsubst(globalpicnum,globalpal,1))
#endif
			{
					//Render for parallaxtype == 0 / paper-sky
				dd[0] = (float)xdimen*.0000001; //Adjust sky depth based on screen size!
				t = (double)((1<<(picsiz[globalpicnum]&15))<<pskybits);
				vv[1] = dd[0]*((double)xdimscale*(double)viewingrange)/(65536.0*65536.0);
				vv[0] = dd[0]*((double)((tilesizy[globalpicnum]>>1)+parallaxyoffs)) - vv[1]*ghoriz;
				i = (1<<(picsiz[globalpicnum]>>4)); if (i != tilesizy[globalpicnum]) i += i;
				vv[0] += dd[0]*((double)sec->ceilingypanning)*((double)i)/256.0;

					//Hack to draw black rectangle below sky when looking down...
				gdx = 0;
				gdy = gxyaspect / -262144.0;
				gdo = -ghoriz*gdy;
				gux = 0;
				guy = 0;
				guo = 0;
				gvx = 0;
				gvy = 0;
				gvo = 0;
				oy = -vv[0]/vv[1];
				if ((oy < cy0) && (oy < cy1)) domost(x1,oy,x0,oy,domostmethod);
				else if ((oy < cy0) != (oy < cy1))
				{      /*         cy1        cy0
						//        /             \
						//oy----------      oy---------
						//    /                    \
						//  cy0                     cy1
						*/
					ox = (oy-cy0)*(x1-x0)/(cy1-cy0) + x0;
					if (oy < cy0) { domost(ox,oy,x0,oy,domostmethod); domost(x1,cy1,ox,oy,domostmethod); }
								else { domost(ox,oy,x0,cy0,domostmethod); domost(x1,oy,ox,oy,domostmethod); }
				} else domost(x1,cy1,x0,cy0,domostmethod);

				gdx = 0;
				gdy = 0;
				gdo = dd[0];
				gux = gdo*(t*((double)xdimscale)*((double)yxaspect)*((double)viewingrange))/(16384.0*65536.0*65536.0*5.0*1024.0);
				guy = 0; //guo calculated later
				gvx = 0;
				gvy = vv[1];
				gvo = vv[0];

				i = globalpicnum;
				r = (cy1-cy0)/(x1-x0); //slope of line
				oy = ((double)viewingrange)/(ghalfx*256.0);
				oz = 1/oy;

				y = ((((int)((x0-ghalfx)*oy))+globalang)>>(11-pskybits));
				fx = x0;
				do
				{
					globalpicnum = pskyoff[y&((1<<pskybits)-1)]+i;
					guo = gdo*(t*((double)(globalang-(y<<(11-pskybits))))/2048.0 + (double)sec->ceilingxpanning) - gux*ghalfx;
					y++;
					ox = fx;
					fx = ((double)((y<<(11-pskybits))-globalang))*oz+ghalfx;
					
					if (fx > x1) {
						fx = x1;
						i = -1;
					}

					domost(fx,(fx-x0)*r+cy0,ox,(ox-x0)*r+cy0,domostmethod); //ceil
				} while (i >= 0);
			}
#if USE_OPENGL
			else
			{     //Skybox code for parallax ceiling!
				double _xp0;
				double _yp0;
				double _xp1;
				double _yp1;
				double _oxp0;
				double _oyp0;
				double _t0;
				double _t1;
				double _nx0;
				double _ny0;
				double _nx1;
				double _ny1;
				double _ryp0;
				double _ryp1;
				double _x0;
				double _x1;
				double _cy0;
				double _fy0;
				double _cy1;
				double _fy1;
				double _ox0;
				double _ox1;
				double ncy0;
				double ncy1;
				static constexpr int skywalx[4] = { -512,  512, 512, -512 };
				static constexpr int skywaly[4] = { -512, -512, 512,  512 };

				std::ignore = _nx0;
				std::ignore = _ny0;
				std::ignore = _nx1;
				std::ignore = _ny1;
				domostmethod = METH_CLAMPED;

				for(i=0;i<4;i++)
				{
					x = skywalx[i&3];
					y = skywaly[i&3];
					_xp0 = (double)y*gcosang  - (double)x*gsinang;
					_yp0 = (double)x*gcosang2 + (double)y*gsinang2;
					x = skywalx[(i+1)&3]; y = skywaly[(i+1)&3];
					_xp1 = (double)y*gcosang  - (double)x*gsinang;
					_yp1 = (double)x*gcosang2 + (double)y*gsinang2;

					_oxp0 = _xp0;
					_oyp0 = _yp0;

						//Clip to close parallel-screen plane
					if (_yp0 < SCISDIST)
					{
						if (_yp1 < SCISDIST) continue;
						_t0 = (SCISDIST-_yp0)/(_yp1-_yp0); _xp0 = (_xp1-_xp0)*_t0+_xp0; _yp0 = SCISDIST;
						_nx0 = (skywalx[(i+1)&3]-skywalx[i&3])*_t0+skywalx[i&3];
						_ny0 = (skywaly[(i+1)&3]-skywaly[i&3])*_t0+skywaly[i&3];
					}
					else { _t0 = 0.F; _nx0 = skywalx[i&3]; _ny0 = skywaly[i&3]; }
					if (_yp1 < SCISDIST)
					{
						_t1 = (SCISDIST-_oyp0)/(_yp1-_oyp0); _xp1 = (_xp1-_oxp0)*_t1+_oxp0; _yp1 = SCISDIST;
						_nx1 = (skywalx[(i+1)&3]-skywalx[i&3])*_t1+skywalx[i&3];
						_ny1 = (skywaly[(i+1)&3]-skywaly[i&3])*_t1+skywaly[i&3];
					}
					else { _t1 = 1.F; _nx1 = skywalx[(i+1)&3]; _ny1 = skywaly[(i+1)&3]; }

					_ryp0 = 1.F/_yp0; _ryp1 = 1.F/_yp1;

						//Generate screen coordinates for front side of wall
					_x0 = ghalfx*_xp0*_ryp0 + ghalfx;
					_x1 = ghalfx*_xp1*_ryp1 + ghalfx;
					if (_x1 <= _x0) continue;
					if ((_x0 >= x1) || (x0 >= _x1)) continue;

					_ryp0 *= gyxscale; _ryp1 *= gyxscale;

					_cy0 = -8192.F*_ryp0 + ghoriz;
					_fy0 =  8192.F*_ryp0 + ghoriz;
					_cy1 = -8192.F*_ryp1 + ghoriz;
					_fy1 =  8192.F*_ryp1 + ghoriz;

					_ox0 = _x0; _ox1 = _x1;

						//Make sure: x0<=_x0<_x1<=_x1
					ncy0 = cy0; ncy1 = cy1;
					if (_x0 < x0)
					{
						t = (x0-_x0)/(_x1-_x0);
						_cy0 += (_cy1-_cy0)*t;
						_fy0 += (_fy1-_fy0)*t;
						_x0 = x0;
					}
					else if (_x0 > x0) ncy0 += (_x0-x0)*(cy1-cy0)/(x1-x0);
					if (_x1 > x1)
					{
						t = (x1-_x1)/(_x1-_x0);
						_cy1 += (_cy1-_cy0)*t;
						_fy1 += (_fy1-_fy0)*t;
						_x1 = x1;
					}
					else if (_x1 < x1) ncy1 += (_x1-x1)*(cy1-cy0)/(x1-x0);

						//   (skybox ceiling)
						//(_x0,_cy0)-(_x1,_cy1)
						//   (skybox wall)
						//(_x0,_fy0)-(_x1,_fy1)
						//   (skybox floor)
						//(_x0,ncy0)-(_x1,ncy1)

						//ceiling of skybox

					drawingskybox = 5; //ceiling/5th texture/index 4 of skybox
					// FIXME: Just say 32?
					ft[0] = 512/16; ft[1] = -512/-16;
					ft[2] = ((float)cosglobalang)*(1.F/2147483648.F);
					ft[3] = ((float)singlobalang)*(1.F/2147483648.F);
					gdx = 0;
					gdy = gxyaspect*-(1.F/4194304.F);
					gdo = -ghoriz*gdy;
					gux = (double)ft[3]*((double)viewingrange)/-65536.0;
					gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
					guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
					guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
					guo += (double)(ft[2]-gux)*ghalfx;
					gvo -= (double)(ft[3]+gvx)*ghalfx;
					if ((_cy0 < ncy0) && (_cy1 < ncy1)) domost(_x1,_cy1,_x0,_cy0,domostmethod);
					else if ((_cy0 < ncy0) != (_cy1 < ncy1))
					{
							//(ox,oy) is intersection of: (_x0,_cy0)-(_x1,_cy1)
							//                            (_x0,ncy0)-(_x1,ncy1)
							//ox = _x0 + (_x1-_x0)*t
							//oy = _cy0 + (_cy1-_cy0)*t
							//oy = ncy0 + (ncy1-ncy0)*t
						t = (_cy0-ncy0)/(ncy1-ncy0-_cy1+_cy0);
						ox = _x0 + (_x1-_x0)*t;
						oy = _cy0 + (_cy1-_cy0)*t;
						if (ncy0 < _cy0) { domost(ox,oy,_x0,ncy0,domostmethod); domost(_x1,_cy1,ox,oy,domostmethod); }
										else { domost(ox,oy,_x0,_cy0,domostmethod); domost(_x1,ncy1,ox,oy,domostmethod); }
					} else domost(_x1,ncy1,_x0,ncy0,domostmethod);

						//wall of skybox
					drawingskybox = i+1; //i+1th texture/index i of skybox
					gdx = (_ryp0-_ryp1)*gxyaspect*(1.F/512.F) / (_ox0-_ox1);
					gdy = 0;
					gdo = _ryp0*gxyaspect*(1.F/512.F) - gdx*_ox0;
					gux = (_t0*_ryp0 - _t1*_ryp1)*gxyaspect*(64.F/512.F) / (_ox0-_ox1);
					guo = _t0*_ryp0*gxyaspect*(64.F/512.F) - gux*_ox0;
					guy = 0;
					_t0 = -8192.0*_ryp0 + ghoriz;
					_t1 = -8192.0*_ryp1 + ghoriz;
					t = ((gdx*_ox0 + gdo)*8.F) / ((_ox1-_ox0) * _ryp0 * 2048.F);
					gvx = (_t0-_t1)*t;
					gvy = (_ox1-_ox0)*t;
					gvo = -gvx*_ox0 - gvy*_t0;
					if ((_fy0 < ncy0) && (_fy1 < ncy1)) domost(_x1,_fy1,_x0,_fy0,domostmethod);
					else if ((_fy0 < ncy0) != (_fy1 < ncy1))
					{
							//(ox,oy) is intersection of: (_x0,_fy0)-(_x1,_fy1)
							//                            (_x0,ncy0)-(_x1,ncy1)
							//ox = _x0 + (_x1-_x0)*t
							//oy = _fy0 + (_fy1-_fy0)*t
							//oy = ncy0 + (ncy1-ncy0)*t
						t = (_fy0-ncy0)/(ncy1-ncy0-_fy1+_fy0);
						ox = _x0 + (_x1-_x0)*t;
						oy = _fy0 + (_fy1-_fy0)*t;
						if (ncy0 < _fy0) { domost(ox,oy,_x0,ncy0,domostmethod); domost(_x1,_fy1,ox,oy,domostmethod); }
										else { domost(ox,oy,_x0,_fy0,domostmethod); domost(_x1,ncy1,ox,oy,domostmethod); }
					} else domost(_x1,ncy1,_x0,ncy0,domostmethod);
				}

					//Floor of skybox
				drawingskybox = 6; //floor/6th texture/index 5 of skybox
				ft[0] = 512/16; ft[1] = 512/-16;
				ft[2] = ((float)cosglobalang)*(1.F/2147483648.F);
				ft[3] = ((float)singlobalang)*(1.F/2147483648.F);
				gdx = 0;
				gdy = gxyaspect*(1.F/4194304.F);
				gdo = -ghoriz*gdy;
				gux = (double)ft[3]*((double)viewingrange)/-65536.0;
				gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
				guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
				guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
				guo += (double)(ft[2]-gux)*ghalfx;
				gvo -= (double)(ft[3]+gvx)*ghalfx;
				gvx = -gvx; gvy = -gvy; gvo = -gvo; //y-flip skybox floor
				domost(x1,cy1,x0,cy0,domostmethod);

				drawingskybox = 0;
			}
			if (rendmode == rendmode_t::OpenGL)
			{
				gfogdensity = tempfogdensity;
			}
#endif
		}

			//(x0,cy0) == (u=             0,v=0,d=)
			//(x1,cy0) == (u=wal->xrepeat*8,v=0)
			//(x0,fy0) == (u=             0,v=v)
			//             u = (gux*sx + guy*sy + guo) / (gdx*sx + gdy*sy + gdo)
			//             v = (gvx*sx + gvy*sy + gvo) / (gdx*sx + gdy*sy + gdo)
			//             0 = (gux*x0 + guy*cy0 + guo) / (gdx*x0 + gdy*cy0 + gdo)
			//wal->xrepeat*8 = (gux*x1 + guy*cy0 + guo) / (gdx*x1 + gdy*cy0 + gdo)
			//             0 = (gvx*x0 + gvy*cy0 + gvo) / (gdx*x0 + gdy*cy0 + gdo)
			//             v = (gvx*x0 + gvy*fy0 + gvo) / (gdx*x0 + gdy*fy0 + gdo)
			//sx = x0, u = t0*wal->xrepeat*8, d = yp0;
			//sx = x1, u = t1*wal->xrepeat*8, d = yp1;
			//d = gdx*sx + gdo
			//u = (gux*sx + guo) / (gdx*sx + gdo)
			//yp0 = gdx*x0 + gdo
			//yp1 = gdx*x1 + gdo
			//t0*wal->xrepeat*8 = (gux*x0 + guo) / (gdx*x0 + gdo)
			//t1*wal->xrepeat*8 = (gux*x1 + guo) / (gdx*x1 + gdo)
			//gdx*x0 + gdo = yp0
			//gdx*x1 + gdo = yp1
		gdx = (ryp0-ryp1)*gxyaspect / (x0-x1);
		gdy = 0;
		gdo = ryp0*gxyaspect - gdx*x0;

			//gux*x0 + guo = t0*wal->xrepeat*8*yp0
			//gux*x1 + guo = t1*wal->xrepeat*8*yp1
		gux = (t0*ryp0 - t1*ryp1)*gxyaspect*(float)wal->xrepeat*8.F / (x0-x1);
		guo = t0*ryp0*gxyaspect*(float)wal->xrepeat*8.F - gux*x0;
		guo += (float)wal->xpanning*gdo;
		gux += (float)wal->xpanning*gdx;
		guy = 0;
			//Derivation for u:
			//   (gvx*x0 + gvy*cy0 + gvo) / (gdx*x0 + gdy*cy0 + gdo) = 0
			//   (gvx*x1 + gvy*cy1 + gvo) / (gdx*x1 + gdy*cy1 + gdo) = 0
			//   (gvx*x0 + gvy*fy0 + gvo) / (gdx*x0 + gdy*fy0 + gdo) = v
			//   (gvx*x1 + gvy*fy1 + gvo) / (gdx*x1 + gdy*fy1 + gdo) = v
			//   (gvx*x0 + gvy*cy0 + gvo*1) = 0
			//   (gvx*x1 + gvy*cy1 + gvo*1) = 0
			//   (gvx*x0 + gvy*fy0 + gvo*1) = t
		ogux = gux;
		oguy = guy;
		oguo = guo;

		if (nextsectnum >= 0)
		{
			auto cfz = getzsofslope(nextsectnum,(int)nx0,(int)ny0);
			ocy0 = ((float)(cfz.ceilz - globalpos.z))*ryp0 + ghoriz;
			ofy0 = ((float)(cfz.floorz - globalpos.z))*ryp0 + ghoriz;
			cfz = getzsofslope(nextsectnum,(int)nx1,(int)ny1);
			ocy1 = ((float)(cfz.ceilz - globalpos.z))*ryp1 + ghoriz;
			ofy1 = ((float)(cfz.floorz - globalpos.z))*ryp1 + ghoriz;

			if ((wal->cstat&48) == 16) maskwall[maskwallcnt++] = z;

			if (((cy0 < ocy0) || (cy1 < ocy1)) && (!((sec->ceilingstat&g_sector[nextsectnum].ceilingstat)&1)))
			{
				globalpicnum = wal->picnum; globalshade = wal->shade; globalpal = (int)((unsigned char)wal->pal);
				if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,wallnum+16384);

				if (!(wal->cstat&4)) i = g_sector[nextsectnum].ceilingz; else i = sec->ceilingz;
				t0 = ((float)(i-globalpos.z))*ryp0 + ghoriz;
				t1 = ((float)(i-globalpos.z))*ryp1 + ghoriz;
				t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.F);
				i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
				fy = (float)wal->ypanning * ((float)i) / 256.0;
				gvx = (t0-t1)*t;
				gvy = (x1-x0)*t;
				gvo = -gvx*x0 - gvy*t0 + fy*gdo; gvx += fy*gdx; gvy += fy*gdy;

				if (wal->cstat&8) //xflip
				{
					t = (float)(wal->xrepeat*8 + wal->xpanning*2);
					gux = gdx*t - gux;
					guy = gdy*t - guy;
					guo = gdo*t - guo;
				}

				if (wal->cstat&256) {
					gvx = -gvx;
					gvy = -gvy;
					gvo = -gvo;
				} //yflip

				domost(x1,ocy1,x0,ocy0,METH_POW2XSPLIT);

				if (wal->cstat&8) {
					gux = ogux;
					guy = oguy;
					guo = oguo;
				}
			}
			if (((ofy0 < fy0) || (ofy1 < fy1)) && (!((sec->floorstat&g_sector[nextsectnum].floorstat)&1)))
			{
				if (!(wal->cstat&2)) nwal = wal;
				else
				{
					nwal = &wall[wal->nextwall];
					guo += (float)(nwal->xpanning-wal->xpanning)*gdo;
					gux += (float)(nwal->xpanning-wal->xpanning)*gdx;
					guy += (float)(nwal->xpanning-wal->xpanning)*gdy;
				}
				globalpicnum = nwal->picnum;
				globalshade = nwal->shade;
				globalpal = (int)((unsigned char)nwal->pal);
				if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,wallnum+16384);

				if (!(nwal->cstat&4)) i = g_sector[nextsectnum].floorz; else i = sec->ceilingz;
				t0 = ((float)(i-globalpos.z))*ryp0 + ghoriz;
				t1 = ((float)(i-globalpos.z))*ryp1 + ghoriz;
				t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.F);
				i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
				fy = (float)nwal->ypanning * ((float)i) / 256.0;
				gvx = (t0-t1)*t;
				gvy = (x1-x0)*t;
				gvo = -gvx*x0 - gvy*t0 + fy*gdo; gvx += fy*gdx; gvy += fy*gdy;

				if (wal->cstat&8) //xflip
				{
					t = (float)(wal->xrepeat*8 + nwal->xpanning*2);
					gux = gdx*t - gux;
					guy = gdy*t - guy;
					guo = gdo*t - guo;
				}
				if (nwal->cstat&256) {
					gvx = -gvx;
					gvy = -gvy;
					gvo = -gvo;
				} //yflip

				domost(x0,ofy0,x1,ofy1,METH_POW2XSPLIT);

				if (wal->cstat&(2+8)) {
					guo = oguo;
					gux = ogux;
					guy = oguy;
				}
			}
		}

		if ((nextsectnum < 0) || (wal->cstat&32))   //White/1-way wall
		{
			if (nextsectnum < 0) globalpicnum = wal->picnum; else globalpicnum = wal->overpicnum;
			globalshade = wal->shade; globalpal = (int)((unsigned char)wal->pal);
			if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,wallnum+16384);

			if (nextsectnum >= 0) { if (!(wal->cstat&4)) i = nextsec->ceilingz; else i = sec->ceilingz; }
								  else { if (!(wal->cstat&4)) i = sec->ceilingz;     else i = sec->floorz; }
			t0 = ((float)(i-globalpos.z))*ryp0 + ghoriz;
			t1 = ((float)(i-globalpos.z))*ryp1 + ghoriz;
			t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.F);
			i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
			fy = (float)wal->ypanning * ((float)i) / 256.0;
			gvx = (t0-t1)*t;
			gvy = (x1-x0)*t;
			gvo = -gvx*x0 - gvy*t0 + fy*gdo;
			gvx += fy*gdx;
			gvy += fy*gdy;

			if (wal->cstat&8) //xflip
			{
				t = (float)(wal->xrepeat*8 + wal->xpanning*2);
				gux = gdx*t - gux;
				guy = gdy*t - guy;
				guo = gdo*t - guo;
			}
			if (wal->cstat&256) {
				gvx = -gvx;
				gvy = -gvy;
				gvo = -gvo;
			} //yflip
			
			domost(x0,-10000,x1,-10000,METH_POW2XSPLIT);
		}

		if (nextsectnum >= 0)
			if ((!(gotsector[nextsectnum >> 3] & pow2char[nextsectnum & 7])) && (testvisiblemost(x0,x1)))
				polymost_scansector(nextsectnum);
	}
}

int polymost_bunchfront (int b1, int b2)
{
	const int b1f = bunchfirst[b1];
	const double x1b1 = dxb1[b1f];
	const double x2b2 = dxb2[bunchlast[b2]];

	if (x1b1 >= x2b2) {
		return -1;
	}

	const int b2f = bunchfirst[b2];
	const double x1b2 = dxb1[b2f];
	const double x2b1 = dxb2[bunchlast[b1]];
	
	if (x1b2 >= x2b1) {
		return(-1);
	}

	if (x1b1 >= x1b2)
	{
		int i{b2f};
		for(; dxb2[i] <= x1b1; i = p2[i]);
		
		return wallfront(b1f, i);
	}

	int i{b1f};
	for(; dxb2[i] <= x1b2; i = p2[i]);

	return wallfront(i, b2f);
}

void polymost_scansector (int sectnum)
{
	double d;
	double xp1;
	double yp1;
	double xp2;
	double yp2;
	walltype *wal;
	walltype *wal2;
	spritetype *spr;
	int z;
	int zz;
	int startwall;
	int endwall;
	int numscansbefore;
	int scanfirst;
	int bunchfrst;
	int nextsectnum;
	int xs;
	int ys;
	int x1;
	int y1;
	int x2;
	int y2;

	if (sectnum < 0)
		return;
	
	if (automapping)
		show2dsector[sectnum >> 3] |= pow2char[sectnum & 7];

	sectorborder[0] = sectnum;
	sectorbordercnt = 1;
	
	do
	{
		sectnum = sectorborder[--sectorbordercnt];

		for(z=headspritesect[sectnum];z>=0;z=nextspritesect[z])
		{
			spr = &sprite[z];
			if ((((spr->cstat&0x8000) == 0) || (showinvisibility)) &&
				  (spr->xrepeat > 0) && (spr->yrepeat > 0) &&
				  (spritesortcnt < MAXSPRITESONSCREEN))
			{
				xs = spr->x-globalpos.x; ys = spr->y-globalpos.y;
				if ((spr->cstat&48) || (xs*gcosang+ys*gsinang > 0))
				{
					copybufbyte(spr,&tsprite[spritesortcnt],sizeof(spritetype));
					tsprite[spritesortcnt++].owner = z;
				}
			}
		}

		gotsector[sectnum >> 3] |= pow2char[sectnum & 7];

		bunchfrst = numbunches;
		numscansbefore = numscans;

		startwall = g_sector[sectnum].wallptr;
		endwall = g_sector[sectnum].wallnum+startwall;
		scanfirst = numscans;
		xp2 = 0;
		yp2 = 0;
		for(z=startwall,wal=&wall[z];z<endwall;z++,wal++)
		{
			wal2 = &wall[wal->point2];
			x1 = wal->pt.x-globalpos.x;
			y1 = wal->pt.y-globalpos.y;
			x2 = wal2->pt.x-globalpos.x;
			y2 = wal2->pt.y-globalpos.y;

			nextsectnum = wal->nextsector; //Scan close sectors
			if ((nextsectnum >= 0) && (!(wal->cstat&32)) && (!(gotsector[nextsectnum >> 3] & pow2char[nextsectnum & 7])))
			{
				d = (double)x1*(double)y2 - (double)x2*(double)y1; xp1 = (double)(x2-x1); yp1 = (double)(y2-y1);
				if (d*d <= (xp1*xp1 + yp1*yp1)*(SCISDIST*SCISDIST*260.0))
					sectorborder[sectorbordercnt++] = nextsectnum;
			}

			if ((z == startwall) || (wall[z-1].point2 != z))
			{
				xp1 = ((double)y1*(double)cosglobalang             - (double)x1*(double)singlobalang            )/64.0;
				yp1 = ((double)x1*(double)cosviewingrangeglobalang + (double)y1*(double)sinviewingrangeglobalang)/64.0;
			}
			else {
				xp1 = xp2;
				yp1 = yp2;
			}

			xp2 = ((double)y2*(double)cosglobalang             - (double)x2*(double)singlobalang            )/64.0;
			yp2 = ((double)x2*(double)cosviewingrangeglobalang + (double)y2*(double)sinviewingrangeglobalang)/64.0;
			
			if ((yp1 >= SCISDIST) || (yp2 >= SCISDIST))
				if ((double)xp1*(double)yp2 < (double)xp2*(double)yp1) //if wall is facing you...
				{
					if (yp1 >= SCISDIST)
						  dxb1[numscans] = (double)xp1*ghalfx/(double)yp1 + ghalfx;
					else
						dxb1[numscans] = -1e32;

					if (yp2 >= SCISDIST)
						dxb2[numscans] = (double)xp2*ghalfx/(double)yp2 + ghalfx;
					else
						dxb2[numscans] = 1e32;

					if (dxb1[numscans] < dxb2[numscans]) {
						thesector[numscans] = sectnum;
						thewall[numscans] = z;
						p2[numscans] = numscans+1;
						numscans++;
					}
				}

			if ((wall[z].point2 < z) && (scanfirst < numscans)) {
				p2[numscans-1] = scanfirst;
				scanfirst = numscans;
			}
		}

		for(z=numscansbefore;z<numscans;z++)
			if ((wall[thewall[z]].point2 != thewall[p2[z]]) || (dxb2[z] > dxb1[p2[z]]))
				{ bunchfirst[numbunches++] = p2[z]; p2[z] = -1; }

		for(z=bunchfrst;z<numbunches;z++)
		{
			for(zz=bunchfirst[z];p2[zz]>=0;zz=p2[zz]);
			bunchlast[z] = zz;
		}
	} while (sectorbordercnt > 0);
}

} // namespace

void polymost_drawrooms()
{
	int i;
	int j;
	int n;
	int n2;
	int closest;
	double ox;
	double oy;
	double oz;
	double ox2;
	double oy2;
	double oz2;
	double r;
	double px[6];
	double py[6];
	double pz[6];
	double px2[6];
	double py2[6];
	double pz2[6];
	double sx[6];
	double sy[6];
	static unsigned char tempbuf[MAXWALLS];

	if (rendmode == rendmode_t::Classic)
		return;

	frameoffset = frameplace + windowy1*bytesperline + windowx1;

#if USE_OPENGL
	if (rendmode == rendmode_t::OpenGL)
	{
		resizeglcheck();

		//glfunc.glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		glfunc.glEnable(GL_DEPTH_TEST);
		glfunc.glDepthFunc(GL_ALWAYS); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS

		//glfunc.glPolygonOffset(1,1); //Supposed to make sprites pasted on walls or floors not disappear
#if (USE_OPENGL == USE_GLES2)
		glfunc.glDepthRangef(0.00001f,1.f); //<- this is more widely supported than glPolygonOffset
#else
		glfunc.glDepthRange(0.00001,1.0); //<- this is more widely supported than glPolygonOffset
#endif

		 //Enable this for OpenGL red-blue glasses mode :)
		if (glredbluemode)
		{
			static int grbfcnt = 0; grbfcnt++;
			if (redblueclearcnt < numpages) { redblueclearcnt++; glfunc.glColorMask(1,1,1,1); glfunc.glClear(GL_COLOR_BUFFER_BIT); }
			if (grbfcnt&1)
			{
				glfunc.glViewport(windowx1-16,yres-(windowy2+1),windowx2-(windowx1-16)+1,windowy2-windowy1+1);
				glfunc.glColorMask(1,0,0,1);
				globalpos.x += singlobalang/1024;
				globalpos.y -= cosglobalang/1024;
			}
			else
			{
				glfunc.glViewport(windowx1,yres-(windowy2+1),windowx2+16-windowx1+1,windowy2-windowy1+1);
				glfunc.glColorMask(0,1,1,1);
				globalpos.x -= singlobalang/1024;
				globalpos.y += cosglobalang/1024;
			}
		}
	}
#endif

		//Polymost supports true look up/down :) Here, we convert horizon to angle.
		//gchang&gshang are cos&sin of this angle (respectively)
	gyxscale = ((double)xdimenscale)/131072.0;
	gxyaspect = ((double)xyaspect*(double)viewingrange)*(5.0/(65536.0*262144.0));
	gviewxrange = ((double)viewingrange)*((double)xdimen)/(32768.0*1024.0);
	gcosang = ((double)cosglobalang)/262144.0;
	gsinang = ((double)singlobalang)/262144.0;
	gcosang2 = gcosang*((double)viewingrange)/65536.0;
	gsinang2 = gsinang*((double)viewingrange)/65536.0;
	ghalfx = (double)halfxdimen; grhalfxdown10 = 1.0/(((double)ghalfx)*1024);
	ghoriz = (double)globalhoriz;

	gvisibility = ((float)globalvisibility)*gxyaspect*FOGSCALE;

		//global cos/sin height angle
	r = (double)((ydimen>>1)-ghoriz);
	gshang = r/std::hypot(r, ghalfx);
	gchang = std::sqrt(1.0 - gshang * gshang);
	ghoriz = (double)(ydimen>>1);

	  //global cos/sin tilt angle
	gctang = std::cos(gtang);
	gstang = std::sin(gtang);
	if (fabs(gstang) < .001) //This hack avoids nasty precision bugs in domost()
		{ gstang = 0; if (gctang > 0) gctang = 1.0; else gctang = -1.0; }

	if (inpreparemirror)
		gstang = -gstang;

		//Generate viewport trapezoid (for handling screen up/down)
	px[0] = px[3] = 0-1;
	px[1] = px[2] = windowx2+1-windowx1+2;
	py[0] = py[1] = 0-1;
	py[2] = py[3] = windowy2+1-windowy1+2; n = 4;
	for(i=0;i<n;i++)
	{
		ox = px[i]-ghalfx; oy = py[i]-ghoriz; oz = ghalfx;

			//Tilt rotation (backwards)
		ox2 = ox*gctang + oy*gstang;
		oy2 = oy*gctang - ox*gstang;
		oz2 = oz;

			//Up/down rotation (backwards)
		px[i] = ox2;
		py[i] = oy2*gchang + oz2*gshang;
		pz[i] = oz2*gchang - oy2*gshang;
	}

		//Clip to SCISDIST plane
	n2 = 0;
	for(i=0;i<n;i++)
	{
		j = i+1; if (j >= n) j = 0;
		if (pz[i] >= SCISDIST) {
			px2[n2] = px[i];
			py2[n2] = py[i];
			pz2[n2] = pz[i];
			n2++;
		}

		if ((pz[i] >= SCISDIST) != (pz[j] >= SCISDIST))
		{
			r = (SCISDIST-pz[i])/(pz[j]-pz[i]);
			px2[n2] = (px[j]-px[i])*r + px[i];
			py2[n2] = (py[j]-py[i])*r + py[i];
			pz2[n2] = SCISDIST; n2++;
		}
	}
	if (n2 < 3) return;
	for(i=0;i<n2;i++)
	{
		r = ghalfx / pz2[i];
		sx[i] = px2[i]*r + ghalfx;
		sy[i] = py2[i]*r + ghoriz;
	}
	initmosts(sx,sy,n2);

	if (searchit == 2)
	{
		short hitsect;
		short hitwall;
		short hitsprite;
		int vx;
		int vy;
		int vz;
		int hitx;
		int hity;
		int hitz;

		ox2 = searchx - ghalfx;
		oy2 = searchy - ghoriz;
		oz2 = ghalfx;

			//Tilt rotation
		ox = ox2*gctang + oy2*gstang;
		oy = oy2*gctang - ox2*gstang;
		oz = oz2;

			//Up/down rotation
		ox2 = oz*gchang - oy*gshang;
		oy2 = ox;
		oz2 = oy*gchang + oz*gshang;

			//Standard Left/right rotation
		vx = (int)(ox2*((float)cosglobalang) - oy2*((float)singlobalang));
		vy = (int)(ox2*((float)singlobalang) + oy2*((float)cosglobalang));
		vz = (int)(oz2*16384.0);

		hitallsprites = true;
		hitscan(globalpos.x,globalpos.y,globalpos.z,globalcursectnum, //Start position
			vx>>12,vy>>12,vz>>8,&hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,0xffff0030);
		hitallsprites = false;

		searchsector = hitsect;
		if (hitwall >= 0)
		{
			searchwall = hitwall; searchstat = 0;
			if (wall[hitwall].nextwall >= 0)
			{
				auto cfz = getzsofslope(wall[hitwall].nextsector,hitx,hity);
				if (hitz > cfz.floorz)
				{
					if (wall[hitwall].cstat&2) //'2' bottoms of walls
						searchwall = wall[hitwall].nextwall;
				}
				else if ((hitz > cfz.ceilz) && (wall[hitwall].cstat&(16+32))) //masking or 1-way
					searchstat = 4;
			}
		}
		else if (hitsprite >= 0) { searchwall = hitsprite; searchstat = 3; }
		else
		{
			auto cfz = getzsofslope(hitsect,hitx,hity);
			if ((hitz<<1) < cfz.ceilz + cfz.floorz)
				searchstat = 1;
			else
				searchstat = 2;
			//if (vz < 0) searchstat = 1; else searchstat = 2; //Won't work for slopes :/
		}
		searchit = 0;
	}

	numscans = numbunches = 0;

	if (globalcursectnum >= MAXSECTORS)
		globalcursectnum -= MAXSECTORS;
	else
	{
		i = globalcursectnum;
		updatesector(globalpos.x,globalpos.y,&globalcursectnum);
		if (globalcursectnum < 0) globalcursectnum = i;
	}

	polymost_scansector(globalcursectnum);

	if (inpreparemirror)
	{
		grhalfxdown10x = -grhalfxdown10;
		inpreparemirror = 0;
		polymost_drawalls(0);
		numbunches--;
		bunchfirst[0] = bunchfirst[numbunches];
		bunchlast[0] = bunchlast[numbunches];
	}
	else
		grhalfxdown10x = grhalfxdown10;

	while (numbunches > 0)
	{
		std::memset(tempbuf,0,numbunches+3); tempbuf[0] = 1;

		closest = 0;              //Almost works, but not quite :(
		for(i=1;i<numbunches;i++)
		{
			j = polymost_bunchfront(i,closest); if (j < 0) continue;
			tempbuf[i] = 1;
			if (!j) { tempbuf[closest] = 1; closest = i; }
		}
		for(i=0;i<numbunches;i++) //Double-check
		{
			if (tempbuf[i]) continue;
			j = polymost_bunchfront(i,closest); if (j < 0) continue;
			tempbuf[i] = 1;
			if (!j) { tempbuf[closest] = 1; closest = i; i = 0; }
		}

		polymost_drawalls(closest);

		if (automapping)
		{
			for(j=bunchfirst[closest];j>=0;j=p2[j])
				show2dwall[thewall[j] >> 3] |= pow2char[thewall[j] & 7];
		}

		numbunches--;
		bunchfirst[closest] = bunchfirst[numbunches];
		bunchlast[closest] = bunchlast[numbunches];
	}
#if USE_OPENGL
	if (rendmode == rendmode_t::OpenGL)
	{
		glfunc.glDepthFunc(GL_LEQUAL); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS

		//glfunc.glPolygonOffset(0,0);
#if (USE_OPENGL == USE_GLES2)
		glfunc.glDepthRangef(0.f,0.99999f); //<- this is more widely supported than glPolygonOffset
#else
		glfunc.glDepthRange(0.0,0.99999); //<- this is more widely supported than glPolygonOffset
#endif
	}
#endif
}

void polymost_drawmaskwall (int damaskwallcnt)
{
	double dpx[8];
	double dpy[8];
	double dpx2[8];
	double dpy2[8];
	float fy;
	float x0;
	float x1;
	float sx0;
	float sy0;
	float sx1;
	float sy1;
	float xp0;
	float yp0;
	float xp1;
	float yp1;
	float oxp0;
	float oyp0;
	float ryp0;
	float ryp1;
	float r;
	float t;
	float t0;
	float t1;
	float csy[4];
	float fsy[4];
	int i, j, n, n2, method;

#ifdef DEBUGGINGAIDS
	polymostcallcounts.drawmaskwall++;
#endif

	const int z = maskwall[damaskwallcnt];
	const walltype* wal = &wall[thewall[z]];
	const walltype* wal2 = &wall[wal->point2];
	const int sectnum = thesector[z];
	const sectortype* sec = &g_sector[sectnum];
	const sectortype* nsec = &g_sector[wal->nextsector];
	const int z1 = std::max(nsec->ceilingz, sec->ceilingz);
	const int z2 = std::min(nsec->floorz, sec->floorz);

	globalpicnum = wal->overpicnum;
	
	if ((unsigned int)globalpicnum >= MAXTILES) {
		globalpicnum = 0;
	}
	
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)thewall[z]+16384);
	globalshade = (int)wal->shade;
	globalpal = (int)((unsigned char)wal->pal);
	globalorientation = (int)wal->cstat;

	sx0 = (float)(wal->pt.x-globalpos.x); sx1 = (float)(wal2->pt.x-globalpos.x);
	sy0 = (float)(wal->pt.y-globalpos.y); sy1 = (float)(wal2->pt.y-globalpos.y);
	yp0 = sx0*gcosang2 + sy0*gsinang2;
	yp1 = sx1*gcosang2 + sy1*gsinang2;
	if ((yp0 < SCISDIST) && (yp1 < SCISDIST)) return;
	xp0 = sy0*gcosang - sx0*gsinang;
	xp1 = sy1*gcosang - sx1*gsinang;

		//Clip to close parallel-screen plane
	oxp0 = xp0; oyp0 = yp0;
	if (yp0 < SCISDIST) { t0 = (SCISDIST-yp0)/(yp1-yp0); xp0 = (xp1-xp0)*t0+xp0; yp0 = SCISDIST; }
						  else t0 = 0.F;
	if (yp1 < SCISDIST) { t1 = (SCISDIST-oyp0)/(yp1-oyp0); xp1 = (xp1-oxp0)*t1+oxp0; yp1 = SCISDIST; }
						else { t1 = 1.F; }

	std::array<ceilfloorz, 4> cfz;
	cfz[0] = getzsofslope(sectnum,(int)((wal2->pt.x-wal->pt.x)*t0+wal->pt.x),(int)((wal2->pt.y-wal->pt.y)*t0+wal->pt.y));
	cfz[1] = getzsofslope(wal->nextsector,(int)((wal2->pt.x-wal->pt.x)*t0+wal->pt.x),(int)((wal2->pt.y-wal->pt.y)*t0+wal->pt.y));
	cfz[2] = getzsofslope(sectnum,(int)((wal2->pt.x-wal->pt.x)*t1+wal->pt.x),(int)((wal2->pt.y-wal->pt.y)*t1+wal->pt.y));
	cfz[3] = getzsofslope(wal->nextsector,(int)((wal2->pt.x-wal->pt.x)*t1+wal->pt.x),(int)((wal2->pt.y-wal->pt.y)*t1+wal->pt.y));

	ryp0 = 1.F/yp0; ryp1 = 1.F/yp1;

		//Generate screen coordinates for front side of wall
	x0 = ghalfx*xp0*ryp0 + ghalfx;
	x1 = ghalfx*xp1*ryp1 + ghalfx;
	if (x1 <= x0) return;

	ryp0 *= gyxscale;
	ryp1 *= gyxscale;

	gdx = (ryp0-ryp1)*gxyaspect / (x0-x1);
	gdy = 0;
	gdo = ryp0*gxyaspect - gdx*x0;

		//gux*x0 + guo = t0*wal->xrepeat*8*yp0
		//gux*x1 + guo = t1*wal->xrepeat*8*yp1
	gux = (t0*ryp0 - t1*ryp1)*gxyaspect*(float)wal->xrepeat*8.F / (x0-x1);
	guo = t0*ryp0*gxyaspect*(float)wal->xrepeat*8.F - gux*x0;
	guo += (float)wal->xpanning*gdo;
	gux += (float)wal->xpanning*gdx;
	guy = 0;

	if (!(wal->cstat&4)) i = z1; else i = z2;
	i -= globalpos.z;
	t0 = ((float)i)*ryp0 + ghoriz;
	t1 = ((float)i)*ryp1 + ghoriz;
	t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.F);
	i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
	fy = (float)wal->ypanning * ((float)i) / 256.0;
	gvx = (t0-t1)*t;
	gvy = (x1-x0)*t;
	gvo = -gvx*x0 - gvy*t0 + fy*gdo;
	gvx += fy*gdx; gvy += fy*gdy;

	if (wal->cstat&8) //xflip
	{
		t = (float)(wal->xrepeat*8 + wal->xpanning*2);
		gux = gdx*t - gux;
		guy = gdy*t - guy;
		guo = gdo*t - guo;
	}

	if (wal->cstat&256) {
		gvx = -gvx;
		gvy = -gvy;
		gvo = -gvo;
	} //yflip

	method = METH_MASKED;
	if (wal->cstat&128) { if (!(wal->cstat&512)) method = METH_TRANS; else method = METH_INTRANS; }
	method |= METH_POW2XSPLIT | METH_LAYERS;

#if USE_OPENGL
	gfogpalnum = sec->floorpal;
	gfogdensity = gvisibility*((float)((unsigned char)(sec->visibility+16)) / 255.F);
#endif

	for(i=0;i<2;i++)
	{
		csy[i] = ((float)(cfz[i].ceilz - globalpos.z))*ryp0 + ghoriz;
		fsy[i] = ((float)(cfz[i].floorz - globalpos.z))*ryp0 + ghoriz;
		csy[i+2] = ((float)(cfz[i+2].ceilz - globalpos.z))*ryp1 + ghoriz;
		fsy[i+2] = ((float)(cfz[i+2].floorz - globalpos.z))*ryp1 + ghoriz;
	}

		//Clip 2 quadrilaterals
		//               /csy3
		//             /   |
		// csy0------/----csy2
		//   |     /xxxxxxx|
		//   |   /xxxxxxxxx|
		// csy1/xxxxxxxxxxx|
		//   |xxxxxxxxxxx/fsy3
		//   |xxxxxxxxx/   |
		//   |xxxxxxx/     |
		// fsy0----/------fsy2
		//   |   /
		// fsy1/

	dpx[0] = x0;
	dpy[0] = csy[1];
	dpx[1] = x1;
	dpy[1] = csy[3];
	dpx[2] = x1;
	dpy[2] = fsy[3];
	dpx[3] = x0;
	dpy[3] = fsy[1];
	n = 4;

		//Clip to (x0,csy[0])-(x1,csy[2])
	n2 = 0; t1 = -((dpx[0]-x0)*(csy[2]-csy[0]) - (dpy[0]-csy[0])*(x1-x0));
	for(i=0;i<n;i++)
	{
		j = i+1; if (j >= n) j = 0;

		t0 = t1;
		t1 = -((dpx[j]-x0)*(csy[2]-csy[0]) - (dpy[j]-csy[0])*(x1-x0));
		if (t0 >= 0) { dpx2[n2] = dpx[i]; dpy2[n2] = dpy[i]; n2++; }
		if ((t0 >= 0) != (t1 >= 0))
		{
			r = t0/(t0-t1);
			dpx2[n2] = (dpx[j]-dpx[i])*r + dpx[i];
			dpy2[n2] = (dpy[j]-dpy[i])*r + dpy[i];
			n2++;
		}
	}
	if (n2 < 3) return;

		//Clip to (x1,fsy[2])-(x0,fsy[0])
	n = 0; t1 = -((dpx2[0]-x1)*(fsy[0]-fsy[2]) - (dpy2[0]-fsy[2])*(x0-x1));
	for(i=0;i<n2;i++)
	{
		j = i + 1;
		
		if (j >= n2) {
			j = 0;
		}

		t0 = t1; t1 = -((dpx2[j]-x1)*(fsy[0]-fsy[2]) - (dpy2[j]-fsy[2])*(x0-x1));
		if (t0 >= 0) { dpx[n] = dpx2[i]; dpy[n] = dpy2[i]; n++; }
		if ((t0 >= 0) != (t1 >= 0))
		{
			r = t0/(t0-t1);
			dpx[n] = (dpx2[j]-dpx2[i])*r + dpx2[i];
			dpy[n] = (dpy2[j]-dpy2[i])*r + dpy2[i];
			n++;
		}
	}
	if (n < 3) return;

	drawpoly(dpx,dpy,n,method);
}

void polymost_drawsprite (int snum)
{
	double px[6];
	double py[6];
	float f;
	float c;
	float s;
	float fx;
	float fy;
	float sx0;
	float sy0;
	float sx1;
	float xp0;
	float yp0;
	float xp1;
	float yp1;
	float oxp0;
	float oyp0;
	float ryp0;
	float ryp1;
	float ft[4];
	float x0;
	float y0;
	float x1;
	float y1;
	float sc0;
	float sf0;
	float sc1;
	float sf1;
	float px2[6];
	float py2[6];
	float xv;
	float yv;
	float t0;
	float t1;
	int i;
	int j;
	int spritenum;
	int xoff=0;
	int yoff=0;
	int method;
	int npoints;
	spritetype *tspr;

#ifdef DEBUGGINGAIDS
	polymostcallcounts.drawsprite++;
#endif

	tspr = tspriteptr[snum];
	if (tspr->owner < 0 || tspr->picnum < 0) return;

	globalpicnum      = tspr->picnum;
	globalshade       = tspr->shade;
	globalpal         = tspr->pal;
	globalorientation = tspr->cstat;
	spritenum         = tspr->owner;

	if ((globalorientation&48) != 48) {	// only non-voxel sprites should do this
		if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,spritenum+32768);

		xoff = (int)((signed char)((picanm[globalpicnum]>>8)&255))+((int)tspr->xoffset);
		yoff = (int)((signed char)((picanm[globalpicnum]>>16)&255))+((int)tspr->yoffset);
	}

	method = METH_MASKED;
	if (tspr->cstat&2) { if (!(tspr->cstat&512)) method = METH_TRANS; else method = METH_INTRANS; }
	method |= METH_CLAMPED | METH_LAYERS;

#if USE_OPENGL
	gfogpalnum = g_sector[tspr->sectnum].floorpal;
	gfogdensity = gvisibility*((float)((unsigned char)(g_sector[tspr->sectnum].visibility+16)) / 255.F);

	// FIXME: Does rendmode need to be checked every time?
	while (rendmode == rendmode_t::OpenGL && !(spriteext[tspr->owner].flags&SPREXT_NOTMD)) {
		if (usemodels && tile2model[tspr->picnum].modelid >= 0 && tile2model[tspr->picnum].framenum >= 0) {
			if (mddraw(tspr, 0)) {
				if (automapping) show2dsprite[spritenum >> 3] |= pow2char[spritenum & 7];
				return;
			}
			break;	// else, render as flat sprite
		}
		if (usevoxels && (tspr->cstat&48)!=48 && tiletovox[tspr->picnum] >= 0 && voxmodels[ tiletovox[tspr->picnum] ]) {
			if (voxdraw(voxmodels[ tiletovox[tspr->picnum] ].get(), tspr, 0)) {
				if (automapping) show2dsprite[spritenum >> 3] |= pow2char[spritenum & 7];
				return;
			}
			break;	// else, render as flat sprite
		}
		if ((tspr->cstat&48)==48 && voxmodels[ tspr->picnum ]) {
			voxdraw(voxmodels[ tspr->picnum ].get(), tspr, 0);
			if (automapping) show2dsprite[spritenum >> 3] |= pow2char[spritenum & 7];
			return;
		}
		break;
	}
#endif

	switch((globalorientation>>4)&3)
	{
		case 0: //Face sprite

				//Project 3D to 2D
			sx0 = (float)(tspr->x-globalpos.x);
			sy0 = (float)(tspr->y-globalpos.y);
			xp0 = sy0*gcosang  - sx0*gsinang;
			yp0 = sx0*gcosang2 + sy0*gsinang2;
			if (yp0 <= SCISDIST) return;
			ryp0 = 1/yp0;
			sx0 = ghalfx*xp0*ryp0 + ghalfx;
			sy0 = ((float)(tspr->z-globalpos.z))*gyxscale*ryp0 + ghoriz;

			f = ryp0*(float)xdimen/160.0;
			fx = ((float)tspr->xrepeat)*f;
			fy = ((float)tspr->yrepeat)*f*((float)yxaspect/65536.0);
			sx0 -= fx*(float)xoff; if (tilesizx[globalpicnum]&1) sx0 += fx*.5;
			sy0 -= fy*(float)yoff;
			fx *= ((float)tilesizx[globalpicnum]);
			fy *= ((float)tilesizy[globalpicnum]);

			px[0] = px[3] = sx0-fx*.5; px[1] = px[2] = sx0+fx*.5;
			if (!(globalorientation&128)) { py[0] = py[1] = sy0-fy; py[2] = py[3] = sy0; }
											 else { py[0] = py[1] = sy0-fy*.5; py[2] = py[3] = sy0+fy*.5; }

			gdx = gdy = guy = gvx = 0; gdo = ryp0*gviewxrange;
			if (!(globalorientation&4))
				  { gux = (float)tilesizx[globalpicnum]*gdo/(px[1]-px[0]+.002); guo = -gux*(px[0]-.001); }
			else { gux = (float)tilesizx[globalpicnum]*gdo/(px[0]-px[1]-.002); guo = -gux*(px[1]+.001); }
			if (!(globalorientation&8))
				  { gvy = (float)tilesizy[globalpicnum]*gdo/(py[3]-py[0]+.002); gvo = -gvy*(py[0]-.001); }
			else { gvy = (float)tilesizy[globalpicnum]*gdo/(py[0]-py[3]-.002); gvo = -gvy*(py[3]+.001); }

			//Clip sprites to ceilings/floors when no parallaxing and not sloped
			if (!(g_sector[tspr->sectnum].ceilingstat&3))
			{
				sy0 = ((float)(g_sector[tspr->sectnum].ceilingz-globalpos.z))*gyxscale*ryp0 + ghoriz;
				if (py[0] < sy0) py[0] = py[1] = sy0;
			}
			if (!(g_sector[tspr->sectnum].floorstat&3))
			{
				sy0 = ((float)(g_sector[tspr->sectnum].floorz-globalpos.z))*gyxscale*ryp0 + ghoriz;
				if (py[2] > sy0) py[2] = py[3] = sy0;
			}

			drawpoly(px,py,4,method);
			break;
		case 1: //Wall sprite

				//Project 3D to 2D
			if (globalorientation&4) xoff = -xoff;
			if (globalorientation&8) yoff = -yoff;

			xv = (float)tspr->xrepeat * (float)sintable[(tspr->ang     )&2047] / 65536.0;
			yv = (float)tspr->xrepeat * (float)sintable[(tspr->ang+1536)&2047] / 65536.0;
			f = (float)(tilesizx[globalpicnum]>>1) + (float)xoff;
			x0 = (float)(tspr->x-globalpos.x) - xv*f; x1 = xv*(float)tilesizx[globalpicnum] + x0;
			y0 = (float)(tspr->y-globalpos.y) - yv*f; y1 = yv*(float)tilesizx[globalpicnum] + y0;

			yp0 = x0*gcosang2 + y0*gsinang2;
			yp1 = x1*gcosang2 + y1*gsinang2;
			if ((yp0 <= SCISDIST) && (yp1 <= SCISDIST)) return;
			xp0 = y0*gcosang - x0*gsinang;
			xp1 = y1*gcosang - x1*gsinang;

				//Clip to close parallel-screen plane
			oxp0 = xp0; oyp0 = yp0;
			if (yp0 < SCISDIST) { t0 = (SCISDIST-yp0)/(yp1-yp0); xp0 = (xp1-xp0)*t0+xp0; yp0 = SCISDIST; }
								else { t0 = 0.F; }
			if (yp1 < SCISDIST) { t1 = (SCISDIST-oyp0)/(yp1-oyp0); xp1 = (xp1-oxp0)*t1+oxp0; yp1 = SCISDIST; }
								else { t1 = 1.F; }

			f = ((float)tspr->yrepeat) * (float)tilesizy[globalpicnum] * 4;

			ryp0 = 1.0/yp0;
			ryp1 = 1.0/yp1;
			sx0 = ghalfx*xp0*ryp0 + ghalfx;
			sx1 = ghalfx*xp1*ryp1 + ghalfx;
			ryp0 *= gyxscale;
			ryp1 *= gyxscale;

			tspr->z -= ((yoff*tspr->yrepeat)<<2);
			if (globalorientation&128)
			{
				tspr->z += ((tilesizy[globalpicnum]*tspr->yrepeat)<<1);
				if (tilesizy[globalpicnum]&1) tspr->z += (tspr->yrepeat<<1); //Odd yspans
			}

			sc0 = ((float)(tspr->z-globalpos.z-f))*ryp0 + ghoriz;
			sc1 = ((float)(tspr->z-globalpos.z-f))*ryp1 + ghoriz;
			sf0 = ((float)(tspr->z-globalpos.z))*ryp0 + ghoriz;
			sf1 = ((float)(tspr->z-globalpos.z))*ryp1 + ghoriz;

			gdx = (ryp0-ryp1)*gxyaspect / (sx0-sx1);
			gdy = 0;
			gdo = ryp0*gxyaspect - gdx*sx0;

				//Original equations:
				//(gux*sx0 + guo)/(gdx*sx1 + gdo) = tilesizx[globalpicnum]*t0
				//(gux*sx1 + guo)/(gdx*sx1 + gdo) = tilesizx[globalpicnum]*t1
				//
				// gvx*sx0 + gvy*sc0 + gvo = 0
				// gvy*sx1 + gvy*sc1 + gvo = 0
				//(gvx*sx0 + gvy*sf0 + gvo)/(gdx*sx0 + gdo) = tilesizy[globalpicnum]
				//(gvx*sx1 + gvy*sf1 + gvo)/(gdx*sx1 + gdo) = tilesizy[globalpicnum]

				//gux*sx0 + guo = t0*tilesizx[globalpicnum]*yp0
				//gux*sx1 + guo = t1*tilesizx[globalpicnum]*yp1
			if (globalorientation&4) { t0 = 1.F-t0; t1 = 1.F-t1; }
			gux = (t0*ryp0 - t1*ryp1)*gxyaspect*(float)tilesizx[globalpicnum] / (sx0-sx1);
			guy = 0;
			guo = t0*ryp0*gxyaspect*(float)tilesizx[globalpicnum] - gux*sx0;

				//gvx*sx0 + gvy*sc0 + gvo = 0
				//gvx*sx1 + gvy*sc1 + gvo = 0
				//gvx*sx0 + gvy*sf0 + gvo = tilesizy[globalpicnum]*(gdx*sx0 + gdo)
			f = ((float)tilesizy[globalpicnum])*(gdx*sx0 + gdo) / ((sx0-sx1)*(sc0-sf0));
			if (!(globalorientation&8))
			{
				gvx = (sc0-sc1)*f;
				gvy = (sx1-sx0)*f;
				gvo = -gvx*sx0 - gvy*sc0;
			}
			else
			{
				gvx = (sf1-sf0)*f;
				gvy = (sx0-sx1)*f;
				gvo = -gvx*sx0 - gvy*sf0;
			}

			//Clip sprites to ceilings/floors when no parallaxing
			if (!(g_sector[tspr->sectnum].ceilingstat&1))
			{
				f = ((float)tspr->yrepeat) * (float)tilesizy[globalpicnum] * 4;
				if (g_sector[tspr->sectnum].ceilingz > tspr->z-f)
				{
					sc0 = ((float)(g_sector[tspr->sectnum].ceilingz-globalpos.z))*ryp0 + ghoriz;
					sc1 = ((float)(g_sector[tspr->sectnum].ceilingz-globalpos.z))*ryp1 + ghoriz;
				}
			}
			if (!(g_sector[tspr->sectnum].floorstat&1))
			{
				if (g_sector[tspr->sectnum].floorz < tspr->z)
				{
					sf0 = ((float)(g_sector[tspr->sectnum].floorz-globalpos.z))*ryp0 + ghoriz;
					sf1 = ((float)(g_sector[tspr->sectnum].floorz-globalpos.z))*ryp1 + ghoriz;
				}
			}

			if (sx0 > sx1)
			{
				if (globalorientation&64) return; //1-sided sprite
				std::swap(sx0, sx1);
				std::swap(sc0, sc1);
				std::swap(sf0, sf1);
			}

			px[0] = sx0;
			py[0] = sc0;
			px[1] = sx1;
			py[1] = sc1;
			px[2] = sx1;
			py[2] = sf1;
			px[3] = sx0;
			py[3] = sf0;
			
			drawpoly(px,py,4,method);
			break;
		case 2: //Floor sprite

			if ((globalorientation&64) != 0)
				if ((globalpos.z > tspr->z) == (!(globalorientation&8)))
					return;
			if ((globalorientation&4) > 0) xoff = -xoff;
			if ((globalorientation&8) > 0) yoff = -yoff;

			i = (tspr->ang&2047);
			c = sintable[(i+512)&2047]/65536.0;
			s = sintable[i]/65536.0;
			x0 = ((tilesizx[globalpicnum]>>1)-xoff)*tspr->xrepeat;
			y0 = ((tilesizy[globalpicnum]>>1)-yoff)*tspr->yrepeat;
			x1 = ((tilesizx[globalpicnum]>>1)+xoff)*tspr->xrepeat;
			y1 = ((tilesizy[globalpicnum]>>1)+yoff)*tspr->yrepeat;

				//Project 3D to 2D
			for(j=0;j<4;j++)
			{
				sx0 = (float)(tspr->x-globalpos.x);
				sy0 = (float)(tspr->y-globalpos.y);
				if ((j+0)&2) { sy0 -= s*y0; sx0 -= c*y0; } else { sy0 += s*y1; sx0 += c*y1; }
				if ((j+1)&2) { sx0 -= s*x0; sy0 += c*x0; } else { sx0 += s*x1; sy0 -= c*x1; }
				px[j] = sy0*gcosang  - sx0*gsinang;
				py[j] = sx0*gcosang2 + sy0*gsinang2;
			}

			if (tspr->z < globalpos.z) //if floor sprite is above you, reverse order of points
			{
				std::swap(px[0], px[1]);
				std::swap(py[0], py[1]);
				std::swap(px[2], px[3]);
				std::swap(py[2], py[3]);
			}

				//Clip to SCISDIST plane
			npoints = 0;
			for(i=0;i<4;i++)
			{
				j = ((i+1)&3);
				if (py[i] >= SCISDIST) { px2[npoints] = px[i]; py2[npoints] = py[i]; npoints++; }
				if ((py[i] >= SCISDIST) != (py[j] >= SCISDIST))
				{
					f = (SCISDIST-py[i])/(py[j]-py[i]);
					px2[npoints] = (px[j]-px[i])*f + px[i];
					py2[npoints] = (py[j]-py[i])*f + py[i]; npoints++;
				}
			}
			if (npoints < 3) return;

				//Project rotated 3D points to screen
			f = ((float)(tspr->z-globalpos.z))*gyxscale;
			for(j=0;j<npoints;j++)
			{
				ryp0 = 1/py2[j];
				px[j] = ghalfx*px2[j]*ryp0 + ghalfx;
				py[j] = f*ryp0 + ghoriz;
			}

				//gd? Copied from floor rendering code
			gdx = 0;
			gdy = gxyaspect / (double)(tspr->z-globalpos.z);
			gdo = -ghoriz*gdy;
				//copied&modified from relative alignment
			xv = (float)tspr->x + s*x1 + c*y1; fx = (double)-(x0+x1)*s;
			yv = (float)tspr->y + s*y1 - c*x1; fy = (double)+(x0+x1)*c;
			f = 1.0/std::hypot(fx, fy);
			fx *= f;
			fy *= f;
			ft[2] = singlobalang*fy + cosglobalang*fx;
			ft[3] = singlobalang*fx - cosglobalang*fy;
			ft[0] = ((double)(globalpos.y-yv))*fy + ((double)(globalpos.x-xv))*fx;
			ft[1] = ((double)(globalpos.x-xv))*fy - ((double)(globalpos.y-yv))*fx;
			gux = (double)ft[3]*((double)viewingrange)/(-65536.0*262144.0);
			gvx = (double)ft[2]*((double)viewingrange)/(-65536.0*262144.0);
			guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
			guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
			guo += (double)(ft[2]/262144.0-gux)*ghalfx;
			gvo -= (double)(ft[3]/262144.0+gvx)*ghalfx;
			f = 4.0/(float)tspr->xrepeat; gux *= f; guy *= f; guo *= f;
			f =-4.0/(float)tspr->yrepeat; gvx *= f; gvy *= f; gvo *= f;
			if (globalorientation&4)
			{
				gux = ((float)tilesizx[globalpicnum])*gdx - gux;
				guy = ((float)tilesizx[globalpicnum])*gdy - guy;
				guo = ((float)tilesizx[globalpicnum])*gdo - guo;
			}

			drawpoly(px,py,npoints,method);
			break;

		case 3: //Voxel sprite
		    break;
	}
	if (automapping) show2dsprite[spritenum >> 3] |= pow2char[spritenum & 7];
}

	//sx,sy       center of sprite; screen coods*65536
	//z           zoom*65536. > is zoomed in
	//a           angle (0 is default)
	//dastat&1    1:translucence
	//dastat&2    1:auto-scale mode (use 320*200 coordinates)
	//dastat&4    1:y-flip
	//dastat&8    1:don't clip to startumost/startdmost
	//dastat&16   1:force point passed to be top-left corner, 0:Editart center
	//dastat&32   1:reverse translucence
	//dastat&64   1:non-masked, 0:masked
	//dastat&128  1:draw all pages (permanent)
	//cx1,...     clip window (actual screen coords)
void polymost_dorotatesprite (int sx, int sy, int z, short a, short picnum,
	signed char dashade, unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2, int uniqid)
{
	int n;
	int nn;
	int x;
	int zz;
	int xoff;
	int yoff;
	int xsiz;
	int ysiz;
	int method;
	int ogpicnum;
	int ogshade;
	int ogpal;
	int oxdimen;
	int oydimen;
	intptr_t ofoffset;
	double ogchang;
	double ogshang;
	double ogctang;
	double ogstang;
	double oghalfx;
	double oghoriz;
	double fx;
	double fy;
	double x1;
	double y1;
	double x2;
	double y2;
	double ogrhalfxdown10;
	double ogrhalfxdown10x;
	double d, cosang, sinang, cosang2, sinang2, px[8], py[8], px2[8], py2[8];

#if USE_OPENGL
	double z1;
	double ogxyaspect;
	double ogfogdensity;
	int oldviewingrange;
	static int onumframes = 0;

	if (rendmode == rendmode_t::OpenGL && usemodels && hudmem[(dastat&4)>>2][picnum].angadd)
	{
		if ((tile2model[picnum].modelid >= 0) && (tile2model[picnum].framenum >= 0))
		{
			if (hudmem[(dastat&4)>>2][picnum].flags&1) return; //"HIDE" is specified in DEF

			ogchang = gchang;
			gchang = 1.0;
			ogshang = gshang;
			gshang = 0.0;
			d = (double)z/(65536.0*16384.0);
			ogctang = gctang;
			gctang = (double)sintable[(a+512)&2047]*d;
			ogstang = gstang;
			gstang = (double)sintable[a&2047]*d;
			ogshade  = globalshade;
			globalshade  = dashade;
			ogpal    = globalpal;
			globalpal    = (int)((unsigned char)dapalnum);
			ogxyaspect = gxyaspect;
			gxyaspect = 1.0;
			ogfogdensity = gfogdensity;
			gfogdensity = 0.0;
			oldviewingrange = viewingrange;
			viewingrange = 65536;

			x1 = hudmem[(dastat&4)>>2][picnum].xadd;
			y1 = hudmem[(dastat&4)>>2][picnum].yadd;
			z1 = hudmem[(dastat&4)>>2][picnum].zadd;
			if (!(hudmem[(dastat&4)>>2][picnum].flags&2)) //"NOBOB" is specified in DEF
			{
				fx = ((double)sx)*(1.0/65536.0);
				fy = ((double)sy)*(1.0/65536.0);

				if (dastat&16)
				{
					xsiz = tilesizx[picnum]; ysiz = tilesizy[picnum];
					xoff = (int)((signed char)((picanm[picnum]>>8)&255))+(xsiz>>1);
					yoff = (int)((signed char)((picanm[picnum]>>16)&255))+(ysiz>>1);

					d = (double)z/(65536.0*16384.0);
					cosang2 = cosang = (double)sintable[(a+512)&2047]*d;
					sinang2 = sinang = (double)sintable[a&2047]*d;
					if ((dastat&2) || (!(dastat&8))) //Don't aspect unscaled perms
						{ d = (double)xyaspect/65536.0; cosang2 *= d; sinang2 *= d; }
					fx += -(double)xoff*cosang2+ (double)yoff*sinang2;
					fy += -(double)xoff*sinang - (double)yoff*cosang;
			}

				if (!(dastat&2))
			{
					x1 += fx/((double)(xdim<<15))-1.0; //-1: left of screen, +1: right of screen
					y1 += fy/((double)(ydim<<15))-1.0; //-1: top of screen, +1: bottom of screen
			}
			else
			{
					x1 += fx/160.0-1.0; //-1: left of screen, +1: right of screen
					y1 += fy/100.0-1.0; //-1: top of screen, +1: bottom of screen
			}
			}

			if (dastat & 4) {
				x1 = -x1;
				y1 = -y1;
			}

			spritetype tspr{
				.x = (int)(((double)gcosang*z1 - (double)gsinang*x1)*16384.0 + globalpos.x),
				.y = (int)(((double)gsinang*z1 + (double)gcosang*x1)*16384.0 + globalpos.y),
				.z = (int)(globalpos.z + y1 * 16384.0 * 0.8),
				.cstat{0},
				.picnum = picnum,
				.shade = dashade,
				.pal = dapalnum,
				.clipdist{0},
				.filler{0},
				.xrepeat{32},
				.yrepeat{32},
				.xoffset{0},
				.yoffset{0},
				.sectnum{0},
				.statnum{0},
				.ang = hudmem[(dastat & 4) >> 2][picnum].angadd + globalang,
				.owner = static_cast<short>(uniqid + MAXSPRITES), // FIXME: Narrowing conversion!
				.xvel{0},
				.yvel{0},
				.zvel{0},
				.lotag{0},
				.hitag{0},
				.extra{0}
			};

			globalorientation = (dastat&1)+((dastat&32)<<4)+((dastat&4)<<1);

			if ((dastat&10) == 2) glfunc.glViewport(windowx1,yres-(windowy2+1),windowx2-windowx1+1,windowy2-windowy1+1);
			else { glfunc.glViewport(0,0,xdim,ydim); glox1 = -1; } //Force fullscreen (glox1=-1 forces it to restore)

			if (hudmem[(dastat&4)>>2][picnum].flags&8) //NODEPTH flag
				glfunc.glDisable(GL_DEPTH_TEST);
			else
			{
				glfunc.glEnable(GL_DEPTH_TEST);
				if (onumframes != numframes)
				{
					onumframes = numframes;
					glfunc.glClear(GL_DEPTH_BUFFER_BIT);
				}
			}

			mddraw(&tspr, !!((dastat&10) != 2));

			viewingrange = oldviewingrange;
			gfogdensity = ogfogdensity;
			gxyaspect = ogxyaspect;
			globalshade  = ogshade;
			globalpal    = ogpal;
			gchang = ogchang;
			gshang = ogshang;
			gctang = ogctang;
			gstang = ogstang;
			return;
		}
	}
#endif

	ogpicnum = globalpicnum;
	globalpicnum = picnum;
	ogshade  = globalshade;
	globalshade  = dashade;
	ogpal    = globalpal;
	globalpal    = (int)((unsigned char)dapalnum);
	oghalfx  = ghalfx;
	ghalfx       = (double)(xdim>>1);
	ogrhalfxdown10 = grhalfxdown10;
	grhalfxdown10 = 1.0/(((double)ghalfx)*1024);
	ogrhalfxdown10x = grhalfxdown10x;
	grhalfxdown10x = grhalfxdown10;
	oghoriz  = ghoriz;
	ghoriz       = (double)(ydim>>1);
	ofoffset = frameoffset;
	frameoffset  = frameplace;
	oxdimen  = xdimen;
	xdimen       = xdim;
	oydimen  = ydimen;
	ydimen       = ydim;
	ogchang = gchang;
	gchang = 1.0;
	ogshang = gshang;
	gshang = 0.0;
	ogctang = gctang;
	gctang = 1.0;
	ogstang = gstang;
	gstang = 0.0;

#if USE_OPENGL
	if (rendmode == rendmode_t::OpenGL)
	{
		glfunc.glViewport(0,0,xdim,ydim); glox1 = -1; //Force fullscreen (glox1=-1 forces it to restore)
		glfunc.glDisable(GL_DEPTH_TEST);
	}
#endif

	method = METH_SOLID;
	if (!(dastat&64))
	{
		 method = METH_MASKED;
		 if (dastat&1) { if (!(dastat&32)) method = METH_TRANS; else method = METH_INTRANS; }
	}
	method |= METH_CLAMPED; //Use OpenGL clamping - dorotatesprite never repeats
	method |= METH_ROTATESPRITE;	//Use rotatesprite projection

	xsiz = tilesizx[globalpicnum]; ysiz = tilesizy[globalpicnum];
	if (dastat&16) { xoff = 0; yoff = 0; }
	else
	{
		xoff = (int)((signed char)((picanm[globalpicnum]>>8)&255))+(xsiz>>1);
		yoff = (int)((signed char)((picanm[globalpicnum]>>16)&255))+(ysiz>>1);
	}
	if (dastat&4) yoff = ysiz-yoff;

	if (dastat&2)  //Auto window size scaling
	{
		if (!(dastat&8))
		{
			if (widescreen) {
				x = ydimenscale;   //= scale(xdimen,yxaspect,320);
				sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),oydimen<<16,200*pixelaspect);
			} else {
				x = xdimenscale;   //= scale(xdimen,yxaspect,320);
				sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),oxdimen,320);
			}
			sy = ((cy1+cy2+2)<<15)+mulscalen<16>(sy-(200<<15),x);
		}
		else
		{
				//If not clipping to startmosts, & auto-scaling on, as a
				//hard-coded bonus, scale to full screen instead
			if (widescreen) {
				x = scale(ydim<<16,yxaspect,200*pixelaspect);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),ydim<<16,200*pixelaspect);
			} else {
				x = scale(xdim,yxaspect,320);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),xdim,320);
			}
			sy = (ydim<<15)+32768+mulscalen<16>(sy-(200<<15),x);
		}
		z = mulscalen<16>(z,x);
	}

	d = (double)z/(65536.0*16384.0);
	cosang2 = cosang = (double)sintable[(a+512)&2047]*d;
	sinang2 = sinang = (double)sintable[a&2047]*d;
	if ((dastat&2) || (!(dastat&8))) //Don't aspect unscaled perms
		{ d = (double)xyaspect/65536.0; cosang2 *= d; sinang2 *= d; }
	px[0] = (double)sx/65536.0 - (double)xoff*cosang2+ (double)yoff*sinang2;
	py[0] = (double)sy/65536.0 - (double)xoff*sinang - (double)yoff*cosang;
	px[1] = px[0] + (double)xsiz*cosang2;
	py[1] = py[0] + (double)xsiz*sinang;
	px[3] = px[0] - (double)ysiz*sinang2;
	py[3] = py[0] + (double)ysiz*cosang;
	px[2] = px[1]+px[3]-px[0];
	py[2] = py[1]+py[3]-py[0];
	n = 4;

	gdx = 0; gdy = 0; gdo = 1.0;
		//px[0]*gux + py[0]*guy + guo = 0
		//px[1]*gux + py[1]*guy + guo = xsiz-.0001
		//px[3]*gux + py[3]*guy + guo = 0
	d = 1.0/(px[0]*(py[1]-py[3]) + px[1]*(py[3]-py[0]) + px[3]*(py[0]-py[1]));
	gux = (py[3]-py[0])*((double)xsiz-.0001)*d;
	guy = (px[0]-px[3])*((double)xsiz-.0001)*d;
	guo = 0 - px[0]*gux - py[0]*guy;

	if (!(dastat&4))
	{     //px[0]*gvx + py[0]*gvy + gvo = 0
			//px[1]*gvx + py[1]*gvy + gvo = 0
			//px[3]*gvx + py[3]*gvy + gvo = ysiz-.0001
		gvx = (py[0]-py[1])*((double)ysiz-.0001)*d;
		gvy = (px[1]-px[0])*((double)ysiz-.0001)*d;
		gvo = 0 - px[0]*gvx - py[0]*gvy;
	}
	else
	{     //px[0]*gvx + py[0]*gvy + gvo = ysiz-.0001
			//px[1]*gvx + py[1]*gvy + gvo = ysiz-.0001
			//px[3]*gvx + py[3]*gvy + gvo = 0
		gvx = (py[1]-py[0])*((double)ysiz-.0001)*d;
		gvy = (px[0]-px[1])*((double)ysiz-.0001)*d;
		gvo = (double)ysiz-.0001 - px[0]*gvx - py[0]*gvy;
	}

	cx2++; cy2++;
		//Clippoly4 (converted from int to double)
	nn = z = 0;
	do
	{
		zz = z+1; if (zz == n) zz = 0;
		x1 = px[z]; 
		x2 = px[zz]-x1;
		
		if ((cx1 <= x1) && (x1 <= cx2)) {
			px2[nn] = x1;
			py2[nn] = py[z];
			nn++;
		}

		if (x2 <= 0) fx = cx2; else fx = cx1;  d = fx-x1;
		if ((d < x2) != (d < 0)) { px2[nn] = fx; py2[nn] = (py[zz]-py[z])*d/x2 + py[z]; nn++; }
		if (x2 <= 0) fx = cx1; else fx = cx2;  d = fx-x1;
		if ((d < x2) != (d < 0)) { px2[nn] = fx; py2[nn] = (py[zz]-py[z])*d/x2 + py[z]; nn++; }
		z = zz;
	} while (z);
	if (nn >= 3)
	{
		n = z = 0;
		do
		{
			zz = z+1; if (zz == nn) zz = 0;
			y1 = py2[z];
			y2 = py2[zz]-y1;
			
			if ((cy1 <= y1) && (y1 <= cy2)) {
				py[n] = y1;
				px[n] = px2[z];
				n++;
			}
			
			if (y2 <= 0)
				fy = cy2;
			else
				fy = cy1; 
			
			d = fy-y1;
			if ((d < y2) != (d < 0)) { py[n] = fy; px[n] = (px2[zz]-px2[z])*d/y2 + px2[z]; n++; }
			if (y2 <= 0)
				fy = cy1;
			else
				fy = cy2;
			
			d = fy - y1;
			if ((d < y2) != (d < 0)) { py[n] = fy; px[n] = (px2[zz]-px2[z])*d/y2 + px2[z]; n++; }
			z = zz;
		} while (z);
		drawpoly(px,py,n,method);
	}

	globalpicnum = ogpicnum;
	globalshade  = ogshade;
	globalpal    = ogpal;
	ghalfx       = oghalfx;
	grhalfxdown10 = ogrhalfxdown10;
	grhalfxdown10x = ogrhalfxdown10x;
	ghoriz       = oghoriz;
	frameoffset  = ofoffset;
	xdimen       = oxdimen;
	ydimen       = oydimen;
	gchang = ogchang;
	gshang = ogshang;
	gctang = ogctang;
	gstang = ogstang;
}

#if USE_OPENGL

namespace {

float trapextx[2];
void drawtrap (float x0, float x1, float y0, float x2, float x3, float y1,
	struct polymostdrawpolycall *draw)
{
	std::array<float, 4> px;
	std::array<float, 4> py;

	if (y0 == y1)
		return;

	px[0] = x0;
	py[0] = y0; 
	py[2] = y1;

	int n{0};
	if (x0 == x1) {
		px[1] = x3;
		py[1] = y1;
		px[2] = x2;
		n = 3;
	}
	else if (x2 == x3) {
		px[1] = x1;
		py[1] = y0;
		px[2] = x3;
		n = 3;
	}
	else {
		px[1] = x1;
		py[1] = y0;
		px[2] = x3;
		px[3] = x2;
		py[3] = y1;
		n = 4;
	}

	polymostvboitem vboitem[4];

	for(int i{0}; i < n; ++i)
	{
		px[i] = std::min(std::max(px[i], trapextx[0]), trapextx[1]);
		vboitem[i].t.s = px[i]*gux + py[i]*guy + guo;
		vboitem[i].t.t = px[i]*gvx + py[i]*gvy + gvo;
		vboitem[i].v.x = px[i];
		vboitem[i].v.y = py[i];
		vboitem[i].v.z = 0.F;
	}
	draw->indexcount = n;
	draw->elementcount = n;
	draw->elementvbo = vboitem;
	polymost_drawpoly_glcall(GL_TRIANGLE_FAN, draw);
	draw->elementvbo = nullptr;
}

void tessectrap (const float *px, const float *py, std::span<const int> point2, int numpoints,
	struct polymostdrawpolycall *draw)
{
	float x0;
	float x1;
	float m0;
	float m1;
	int i;
	int j;
	int k;
	int z;
	int i0;
	int i1;
	int i2;
	int i3;
	int npoints;
	int gap;
	int numrst;

	static int allocpoints{0};
	static int* slist{nullptr};
	static int* npoint2{nullptr};
	struct raster { float x, y, xi; int i; };
	static raster *rst = nullptr;
	static polymostvboitem *vboitem = nullptr;
	if (numpoints + 16 > allocpoints) //16 for safety
	{
		allocpoints = numpoints+16;
		rst = (raster*)realloc(rst,allocpoints*sizeof(raster));
		slist = (int*)realloc(slist,allocpoints*sizeof(int));
		npoint2 = (int*)realloc(npoint2,allocpoints*sizeof(int));
		vboitem = (polymostvboitem *)realloc(vboitem, allocpoints*sizeof(polymostvboitem));
	}

		//Remove unnecessary collinear points:
	// FIXME: nullptr access here if npoint2 is not set!
	for(i=0;i<numpoints;i++) npoint2[i] = point2[i];
	npoints = numpoints; z = 0;
	for(i=0;i<numpoints;i++)
	{
		j = npoint2[i]; if ((point2[i] < i) && (i < numpoints-1)) z = 3;
		if (j < 0) continue;
		k = npoint2[j];
		m0 = (px[j]-px[i])*(py[k]-py[j]);
		m1 = (py[j]-py[i])*(px[k]-px[j]);
		if (m0 < m1) { z |= 1; continue; }
		if (m0 > m1) { z |= 2; continue; }
		npoint2[i] = k; npoint2[j] = -1; npoints--; i--; //collinear
	}
	if (!z) return;
	trapextx[0] = trapextx[1] = px[0];
	for(i=j=0;i<numpoints;i++)
	{
		if (npoint2[i] < 0) continue;
		if (px[i] < trapextx[0]) trapextx[0] = px[i];
		if (px[i] > trapextx[1]) trapextx[1] = px[i];
		slist[j++] = i;
	}
	if (z != 3) //Simple polygon... early out
	{
		for(i=0;i<npoints;i++)
		{
			j = slist[i];
			vboitem[i].t.s = px[j]*gux + py[j]*guy + guo;
			vboitem[i].t.t = px[j]*gvx + py[j]*gvy + gvo;
			vboitem[i].v.x = px[j];
			vboitem[i].v.y = py[j];
			vboitem[i].v.z = 0.F;
		}
		draw->indexcount = npoints;
		draw->elementcount = npoints;
		draw->elementvbo = vboitem;
		polymost_drawpoly_glcall(GL_TRIANGLE_FAN, draw);
		return;
	}

		//Sort points by y's
	for(gap=(npoints>>1);gap;gap>>=1)
		for(i=0;i<npoints-gap;i++)
			for(j=i;j>=0;j-=gap)
			{
				if (py[npoint2[slist[j]]] <= py[npoint2[slist[j+gap]]]) break;
				k = slist[j]; slist[j] = slist[j+gap]; slist[j+gap] = k;
			}

	numrst = 0;
	for(z=0;z<npoints;z++)
	{
		i0 = slist[z]; i1 = npoint2[i0]; if (py[i0] == py[i1]) continue;
		i2 = i1; i3 = npoint2[i1];
		if (py[i1] == py[i3]) { i2 = i3; i3 = npoint2[i3]; }

			//i0        i3
			//  \      /
			//   i1--i2
			//  /      \ ~
			//i0        i3

		if ((py[i1] < py[i0]) && (py[i2] < py[i3])) //Insert raster
		{
			for(i=numrst;i>0;i--)
			{
				if (rst[i-1].xi*(py[i1]-rst[i-1].y) + rst[i-1].x < px[i1]) break;
				rst[i+1] = rst[i-1];
			}
			numrst += 2;

			if (i&1) //split inside area
			{
				j = i-1;

				x0 = (py[i1] - rst[j  ].y)*rst[j  ].xi + rst[j  ].x;
				x1 = (py[i1] - rst[j+1].y)*rst[j+1].xi + rst[j+1].x;
				drawtrap(rst[j].x,rst[j+1].x,rst[j].y,x0,x1,py[i1],draw);
				rst[j  ].x = x0; rst[j  ].y = py[i1];
				rst[j+3].x = x1; rst[j+3].y = py[i1];
			}

			m0 = (px[i0]-px[i1]) / (py[i0]-py[i1]);
			m1 = (px[i3]-px[i2]) / (py[i3]-py[i2]);
			j = ((px[i1] > px[i2]) || ((i1 == i2) && (m0 >= m1))) + i;
			k = (i<<1)+1 - j;
			rst[j].i = i0; rst[j].xi = m0; rst[j].x = px[i1]; rst[j].y = py[i1];
			rst[k].i = i3; rst[k].xi = m1; rst[k].x = px[i2]; rst[k].y = py[i2];
		}
		else
		{                  //NOTE:don't count backwards!
			if (i1 == i2) { for(i=0;i<numrst;i++) if (rst[i].i == i1) break; }
						else { for(i=0;i<numrst;i++) if ((rst[i].i == i1) || (rst[i].i == i2)) break; }
			j = i&~1;

			if ((py[i1] > py[i0]) && (py[i2] > py[i3])) //Delete raster
			{
				for(;j<=i+1;j+=2)
				{
					x0 = (py[i1] - rst[j  ].y)*rst[j  ].xi + rst[j  ].x;
					if ((i == j) && (i1 == i2)) x1 = x0; else x1 = (py[i1] - rst[j+1].y)*rst[j+1].xi + rst[j+1].x;
					drawtrap(rst[j].x,rst[j+1].x,rst[j].y,x0,x1,py[i1],draw);
					rst[j  ].x = x0; rst[j  ].y = py[i1];
					rst[j+1].x = x1; rst[j+1].y = py[i1];
				}
				numrst -= 2; for(;i<numrst;i++) rst[i] = rst[i+2];
			}
			else
			{
				x0 = (py[i1] - rst[j  ].y)*rst[j  ].xi + rst[j  ].x;
				x1 = (py[i1] - rst[j+1].y)*rst[j+1].xi + rst[j+1].x;
				drawtrap(rst[j].x,rst[j+1].x,rst[j].y,x0,x1,py[i1],draw);
				rst[j  ].x = x0; rst[j  ].y = py[i1];
				rst[j+1].x = x1; rst[j+1].y = py[i1];

				if (py[i0] < py[i3]) { rst[i].x = px[i2]; rst[i].y = py[i2]; rst[i].i = i3; }
									 else { rst[i].x = px[i1]; rst[i].y = py[i1]; rst[i].i = i0; }
				rst[i].xi = (px[rst[i].i] - rst[i].x) / (py[rst[i].i] - py[i1]);
			}
		}
	}
}

} // namespace

void polymost_fillpolygon (int npoints)
{
	int i;
	struct polymostdrawpolycall draw;

	g_pt1.x = mulscalen<16>(g_pt1.x,xyaspect);
	g_pt2.y = mulscalen<16>(g_pt2.y,xyaspect);
	gux = ((double)asm1)*(1.0/4294967296.0);
	gvx = ((double)asm2)*(1.0/4294967296.0);
	guy = ((double)g_pt1.x)*(1.0/4294967296.0);
	gvy = ((double)g_pt2.y)*(-1.0/4294967296.0);
	guo = (((double)xdim)*gux + ((double)ydim)*guy)*-.5 + ((double)globalpos.x)*(1.0/4294967296.0);
	gvo = (((double)xdim)*gvx + ((double)ydim)*gvy)*-.5 - ((double)globalpos.y)*(1.0/4294967296.0);

		//Convert int to float (in-place)
	for(i=npoints-1;i>=0;i--)
	{
		((float *)&rx1)[i] = ((float)rx1[i])/4096.0;
		((float *)&ry1)[i] = ((float)ry1[i])/4096.0;
	}

	if ((unsigned int)globalpicnum >= MAXTILES)
		globalpicnum = 0;
	
	if (palookup[globalpal].empty())
		globalpal = 0;

	unsigned short ptflags{0};

	if (usehightile)
		ptflags |= PTH_HIGHTILE;

	draw.texture0 = 0;
	const PTHead* pth = PT_GetHead(globalpicnum, globalpal, ptflags, 0);

	if (pth && pth->pic[PTHPIC_BASE]) {
		draw.texture0 = pth->pic[ PTHPIC_BASE ]->glpic;
	}

	draw.texture1 = nulltexture;
	draw.alphacut = 0.F;
	draw.fogcolour.r = draw.fogcolour.g = draw.fogcolour.b = draw.fogcolour.a = 0.F;
	draw.fogdensity = 0.F;

	draw.colour.r = draw.colour.g = draw.colour.b =
		((float)(numpalookups - std::min(std::max(globalshade, 0), static_cast<int>(numpalookups))))/((float)numpalookups);

	switch ((globalorientation>>7)&3) {
		case 0:
		case 1: draw.colour.a = 1.0; glfunc.glDisable(GL_BLEND); break;
		case 2: draw.colour.a = 0.66; glfunc.glEnable(GL_BLEND); break;
		case 3: draw.colour.a = 0.33; glfunc.glEnable(GL_BLEND); break;
	}
	if (pth && (pth->flags & PTH_HIGHTILE) && (globalpal != pth->repldef->palnum)) {
		// apply tinting for replaced textures
		draw.colour.r *= (float)hictinting[globalpal].r / 255.0;
		draw.colour.g *= (float)hictinting[globalpal].g / 255.0;
		draw.colour.b *= (float)hictinting[globalpal].b / 255.0;
	}

	draw.modelview = &gidentitymat[0][0];
	draw.projection = &gorthoprojmat[0][0];

	draw.indexbuffer = 0;
	draw.elementbuffer = 0;
	tessectrap((float *)&rx1[0], (float *)&ry1[0], xb1, npoints, &draw);
}

int polymost_drawtilescreen (int tilex, int tiley, int wallnum, int dimen)
{
	struct polymostdrawauxcall draw;
	polymostvboitem vboitem[4];

	if ((rendmode != rendmode_t::OpenGL) || (qsetmode != 200))
		return -1;

	const auto xdime = (float)tilesizx[wallnum];
	const auto ydime = (float)tilesizy[wallnum];

	float scx;
	float scy;

	if ((xdime <= dimen) && (ydime <= dimen)) {
		scx = xdime;
		scy = ydime;
	} else {
		scx = (float)dimen;
		scy = (float)dimen;
		if (xdime < ydime) {
			scx *= xdime/ydime;
		} else {
			scy *= ydime/xdime;
		}
	}

	const PTHead* pth = PT_GetHead(wallnum, 0, (usehightile ? PTH_HIGHTILE : 0) | PTH_CLAMPED, 0);

	float xdimepad;
	float ydimepad;

	if (pth) {
		xdimepad = (float)pth->pic[PTHPIC_BASE]->tsizx / (float)pth->pic[PTHPIC_BASE]->sizx;
		ydimepad = (float)pth->pic[PTHPIC_BASE]->tsizy / (float)pth->pic[PTHPIC_BASE]->sizy;
	} else {
		xdimepad = 1.0;
		ydimepad = 1.0;
	}

	palette_t bgcolour = curpalette[255];

	if (!gammabrightness) {
		bgcolour.r = britable[curbrightness][bgcolour.r];
		bgcolour.g = britable[curbrightness][bgcolour.g];
		bgcolour.b = britable[curbrightness][bgcolour.b];
	}

	draw.mode = 1;	// Tile.

	draw.texture0 = nulltexture;
	if (pth) {
		if (pth->pic[PTHPIC_BASE]) {
			draw.texture0 = pth->pic[PTHPIC_BASE]->glpic;
		}
	}

	draw.bgcolour.r = bgcolour.r / 255.F;
	draw.bgcolour.g = bgcolour.g / 255.F;
	draw.bgcolour.b = bgcolour.b / 255.F;
	draw.bgcolour.a = 1.F;

	vboitem[0].v.x = (GLfloat)tilex;
	vboitem[0].v.y = (GLfloat)tiley;
	vboitem[0].v.z = 0.F;
	vboitem[0].t.s = 0.F;
	vboitem[0].t.t = 0.F;

	vboitem[1].v.x = (GLfloat)tilex+scx;
	vboitem[1].v.y = (GLfloat)tiley;
	vboitem[1].v.z = 0.F;
	vboitem[1].t.s = xdimepad;
	vboitem[1].t.t = 0.F;

	vboitem[2].v.x = (GLfloat)tilex+scx;
	vboitem[2].v.y = (GLfloat)tiley+scy;
	vboitem[2].v.z = 0.F;
	vboitem[2].t.s = xdimepad;
	vboitem[2].t.t = ydimepad;

	vboitem[3].v.x = (GLfloat)tilex;
	vboitem[3].v.y = (GLfloat)tiley+scy;
	vboitem[3].v.z = 0.F;
	vboitem[3].t.s = 0.F;
	vboitem[3].t.t = ydimepad;

	draw.indexcount = 4;
	draw.indexes = nullptr;
	draw.elementcount = 4;
	draw.elementvbo = vboitem;

	polymost_drawaux_glcall(GL_TRIANGLE_FAN, &draw);

	return 0;
}

int polymost_printext256(int xpos, int ypos, short col, short backcol, std::string_view name, char fontsize)
{
	GLfloat tx;
	GLfloat ty;
	GLfloat txc;
	GLfloat tyc;
	GLfloat txg;
	GLfloat tyg;
	GLfloat tyoff;
	GLfloat cx;
	GLfloat cy;
	int indexcnt;
	int vbocnt;
	palette_t colour;
	struct polymostdrawauxcall draw;
	polymostvboitem vboitem[80*4];
	GLushort vboindexes[80*6];

	if ((rendmode != rendmode_t::OpenGL) || (qsetmode != 200)) {
		return -1;
	}

	polymost_preparetext();
	setpolymost2dview();	// disables blending, texturing, and depth testing
	glfunc.glDepthMask(GL_FALSE);	// disable writing to the z-buffer
	glfunc.glEnable(GL_BLEND);

	draw.mode = 0;	// Text.

	draw.texture0 = texttexture;

	colour = curpalette[col];

	if (!gammabrightness) {
		colour.r = britable[curbrightness][ colour.r ];
		colour.g = britable[curbrightness][ colour.g ];
		colour.b = britable[curbrightness][ colour.b ];
	}

	draw.colour.r = colour.r / 255.F;
	draw.colour.g = colour.g / 255.F;
	draw.colour.b = colour.b / 255.F;
	draw.colour.a = 1.F;

	colour = curpalette[std::min(0, static_cast<int>(backcol))];

	if (!gammabrightness) {
		colour.r = britable[curbrightness][ colour.r ];
		colour.g = britable[curbrightness][ colour.g ];
		colour.b = britable[curbrightness][ colour.b ];
	}

	draw.bgcolour.r = colour.r / 255.F;
	draw.bgcolour.g = colour.g / 255.F;
	draw.bgcolour.b = colour.b / 255.F;
	if (backcol >= 0) {
		draw.bgcolour.a = 1.F;
	} else {
		draw.bgcolour.a = 0.F;
	}

	fontsize = std::min(static_cast<int>(fontsize), 2); // FIXME: Don't use char for indexes.
	tyoff = 64.F * (float)fontsize / 256.F;
	cx = (float)textfonts[(unsigned)fontsize].charxsiz;
	cy = (float)textfonts[(unsigned)fontsize].charysiz;
	txc = cx/256.F;		// texture-space width/height of character
	tyc = cy/256.F;
	txg = 8.F/256.F;	// texture-space width/height of cell
	tyg = (fontsize < 2 ? 8.F : 16.F) / 256.F;

	draw.indexes = vboindexes;
	draw.elementvbo = vboitem;

	int c{ 0 };

	indexcnt = vbocnt = 0;
	while (name[c]) {
		for (; name[c] && indexcnt < 80*6; c++) {
			tx = (GLfloat)((unsigned char)name[c]%32)*txg;
			ty = (GLfloat)((unsigned char)name[c]/32)*tyg + tyoff;

			vboindexes[indexcnt + 0] = vbocnt+0;
			vboindexes[indexcnt + 1] = vbocnt+1;
			vboindexes[indexcnt + 2] = vbocnt+2;
			vboindexes[indexcnt + 3] = vbocnt+0;
			vboindexes[indexcnt + 4] = vbocnt+2;
			vboindexes[indexcnt + 5] = vbocnt+3;

			vboitem[vbocnt + 0].v.x = (GLfloat)xpos;
			vboitem[vbocnt + 0].v.y = (GLfloat)ypos;
			vboitem[vbocnt + 0].v.z = 0.F;
			vboitem[vbocnt + 0].t.s = tx;
			vboitem[vbocnt + 0].t.t = ty;

			vboitem[vbocnt + 1].v.x = (GLfloat)xpos+cx;
			vboitem[vbocnt + 1].v.y = (GLfloat)ypos;
			vboitem[vbocnt + 1].v.z = 0.F;
			vboitem[vbocnt + 1].t.s = tx+txc;
			vboitem[vbocnt + 1].t.t = ty;

			vboitem[vbocnt + 2].v.x = (GLfloat)xpos+cx;
			vboitem[vbocnt + 2].v.y = (GLfloat)ypos+cy;
			vboitem[vbocnt + 2].v.z = 0.F;
			vboitem[vbocnt + 2].t.s = tx+txc;
			vboitem[vbocnt + 2].t.t = ty+tyc;

			vboitem[vbocnt + 3].v.x = (GLfloat)xpos;
			vboitem[vbocnt + 3].v.y = (GLfloat)ypos+cy;
			vboitem[vbocnt + 3].v.z = 0.F;
			vboitem[vbocnt + 3].t.s = tx;
			vboitem[vbocnt + 3].t.t = ty+tyc;

			indexcnt += 6;
			vbocnt += 4;
			xpos += textfonts[(unsigned)fontsize].charxsiz;
		}

		draw.indexcount = indexcnt;
		draw.elementcount = vbocnt;

		polymost_drawaux_glcall(GL_TRIANGLES, &draw);

		indexcnt = 0;
		vbocnt = 0;
	}

	glfunc.glDepthMask(GL_TRUE);	// re-enable writing to the z-buffer

	return 0;
}

int polymost_drawline256(int x1, int y1, int x2, int y2, unsigned char col)
{
	palette_t colour;
	struct polymostdrawauxcall draw;
	polymostvboitem vboitem[2];

	if ((rendmode != rendmode_t::OpenGL) || (qsetmode != 200)) return(-1);

	polymost_preparetext();
	setpolymost2dview();	// disables blending, texturing, and depth testing
	glfunc.glDepthMask(GL_FALSE);	// disable writing to the z-buffer
	glfunc.glEnable(GL_BLEND);

	draw.mode = 2;	// Solid colour.

	colour = curpalette[col];
	if (!gammabrightness) {
		colour.r = britable[curbrightness][ colour.r ];
		colour.g = britable[curbrightness][ colour.g ];
		colour.b = britable[curbrightness][ colour.b ];
	}
	draw.colour.r = colour.r / 255.F;
	draw.colour.g = colour.g / 255.F;
	draw.colour.b = colour.b / 255.F;
	draw.colour.a = 1.F;

	vboitem[0].v.x = (GLfloat)(x1+2048)/4096.F;
	vboitem[0].v.y = (GLfloat)(y1+2048)/4096.F;
	vboitem[0].v.z = 0.F;

	vboitem[1].v.x = (GLfloat)(x2+2048)/4096.F;
	vboitem[1].v.y = (GLfloat)(y2+2048)/4096.F;
	vboitem[1].v.z = 0.F;

	draw.indexcount = 2;
	draw.indexes = nullptr;
	draw.elementcount = 2;
	draw.elementvbo = vboitem;

	polymost_drawaux_glcall(GL_LINES, &draw);

	glfunc.glDepthMask(GL_TRUE);	// re-enable writing to the z-buffer

	return 0;
}

int polymost_plotpixel(int x, int y, unsigned char col)
{
	palette_t colour;
	struct polymostdrawauxcall draw;
	polymostvboitem vboitem[1];

	if ((rendmode != rendmode_t::OpenGL) || (qsetmode != 200)) return(-1);

	setpolymost2dview();	// disables blending, texturing, and depth testing
	glfunc.glDepthMask(GL_FALSE);	// disable writing to the z-buffer
	glfunc.glEnable(GL_BLEND);

	draw.mode = 2;	// Solid colour.

	colour = curpalette[col];
	if (!gammabrightness) {
		colour.r = britable[curbrightness][ colour.r ];
		colour.g = britable[curbrightness][ colour.g ];
		colour.b = britable[curbrightness][ colour.b ];
	}
	draw.colour.r = colour.r / 255.F;
	draw.colour.g = colour.g / 255.F;
	draw.colour.b = colour.b / 255.F;
	draw.colour.a = 1.F;

	vboitem[0].v.x = (GLfloat)x;
	vboitem[0].v.y = (GLfloat)y;
	vboitem[0].v.z = 0.F;

	draw.indexcount = 1;
	draw.indexes = nullptr;
	draw.elementcount = 1;
	draw.elementvbo = vboitem;

	polymost_drawaux_glcall(GL_POINTS, &draw);

	glfunc.glDepthMask(GL_TRUE);	// re-enable writing to the z-buffer

	return 0;
}

namespace {

int polymost_preparetext()
{
	if (texttexture) {
		return 0;
	}

	// construct a 256x256 rgba texture for the font glyph matrix.
	glfunc.glGenTextures(1,&texttexture);
	
	if (!texttexture)
		return -1;

	std::vector<unsigned int> tbuf(256 * 256);

	// 8x8 - lines 0 to 63, 8 lines per row
	// 4x6 - lines 64 to 127, 8 lines per row
	// 8x14 - lines 128-255, 16 lines per row

	for (int fn{0}; fn < 3; ++fn) {
		const struct textfontspec *f = &textfonts[fn];
		unsigned int* tptr = &tbuf[0] + 256 * 64 * fn;
		const int cellh = fn < 2 ? 8 : 16;

		for (int c = 0; c < 256; ++c) {
			const unsigned char *letptr = &f->font[c * f->cellh + f->cellyoff];
			unsigned int *cptr = tptr + ((c / 32) * 256 * cellh) + ((c % 32) *8);

			for(int y{0}; y < f->charysiz; ++y)
			{
				for(int x{0}; x < f->charxsiz; ++x)
				{
					if (letptr[y] & pow2char[7 - x - f->cellxoff])
						cptr[x] = 0xffffffff;
				}

				cptr += 256;
			}
		}
	}

	glfunc.glActiveTexture(GL_TEXTURE0);
	glfunc.glBindTexture(GL_TEXTURE_2D, texttexture);
	glfunc.glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE, &tbuf[0]);
	glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glfunc.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

	return 0;
}

} // namespace

void polymost_precache_begin()
{
	PTBeginPriming();
}

void polymost_precache(int dapicnum, int dapalnum, int datype)
{
	// dapicnum and dapalnum are like you'd expect
	// datype is 0 for a wall/floor/ceiling and 1 for a sprite
	//    basically this just means walls are repeating
	//    while sprites are clamped

	if (rendmode != rendmode_t::OpenGL)
		return;

	if (palookup[dapalnum].empty())
		return;//dapalnum = 0;

		//FIXME
	//buildprintf("precached {} {} type {}\n", dapicnum, dapalnum, datype);
	unsigned short flags = (datype & 1) ? PTH_CLAMPED : 0;

	if (usehightile)
		flags |= PTH_HIGHTILE;
	
	PTMarkPrime(dapicnum, dapalnum, flags);

	if (datype == 0) return;

	const int mid = md_tilehasmodel(dapicnum);

	if (mid < 0 || models[mid]->mdnum < 2) return;

	{
		int j{0};

		if (models[mid]->mdnum == 3)
			j = ((md3model *)models[mid])->head.numsurfs;

		for (int i{0}; i <= j; ++i)
			mdloadskin((md2model*)models[mid], 0, dapalnum, i);
	}
}

int polymost_precache_run(int* done, int* total)
{
	return PTDoPrime(done, total);
}

namespace {

#ifdef DEBUGGINGAIDS
// because I'm lazy
int osdcmd_debugdumptexturedefs(const osdfuncparm_t *parm)
{
	hicreplctyp *hr;
	int i;
	(void)parm;

	if (!hicfirstinit) return OSDCMD_OK;

	buildprintf("// Begin Texture Dump\n");
	for (i=0;i<MAXTILES;i++) {
		hr = hicreplc[i];
		if (!hr) continue;
		buildprintf("texture {} {\n", i);
		for (; hr; hr = hr->next) {
			if (!hr->filename) continue;
			buildprintf("    pal {} { name \"{}\" ", hr->palnum, hr->filename);
			if (hr->alphacut >= 0.0) buildprintf("alphacut {} ", hr->alphacut);
			buildprintf("}\n");
		}
		buildprintf("}\n");
	}
	buildprintf("// End Texture Dump\n");

	return OSDCMD_OK;	// no replacement found
}

namespace {

int osdcmd_debugtexturehash(const osdfuncparm_t *parm)
{
	PTIter iter;
	PTHead * pth;
	int i;
	(void)parm;

	if (!hicfirstinit) {
		return OSDCMD_OK;
	}

	buildprintf("// Begin texture hash dump\n");
	iter = PTIterNew();
	while ((pth = PTIterNext(iter)) != 0) {
		buildprintf(" picnum:{} palnum:{} flags:{:04X} repldef:{}\n",
			   pth->picnum, pth->palnum, pth->flags,
			   (void*)pth->repldef);
		for (i=0; i<6; i++) {
			if (pth->pic[i]) {
				buildprintf("   pic[{}]: {} => glpic:{} flags:{} sizx/y:{}/{} tsizx/y:{}/{}\n",
					   i, (void*)pth->pic[i], pth->pic[i]->glpic, pth->pic[i]->flags,
					   pth->pic[i]->sizx, pth->pic[i]->sizy,
					   pth->pic[i]->tsizx, pth->pic[i]->tsizy);
			}
		}
	}
	PTIterFree(iter);
	buildprintf("// End texture hash dump\n");

	return OSDCMD_OK;	// no replacement found
}

} // namespace

#endif

#ifdef SHADERDEV
int osdcmd_debugreloadshaders(const osdfuncparm_t *parm)
{
	std::ignore = parm;
	polymost_loadshaders();
	return OSDCMD_OK;
}
#endif

int osdcmd_gltexturemode(const osdfuncparm_t *parm)
{
	if (parm->parms.size() != 1) {
		buildprintf("Current texturing mode is {}\n", glfiltermodes[gltexfiltermode].name);
		buildprintf("  Vaild modes are:\n");
		std::ranges::for_each(glfiltermodes, [mode_cnt = 0](std::string_view aModeName) mutable {
			buildprintf("     {} - {}\n", (mode_cnt++), aModeName);
		}, &glfiltermode::name);			

		return OSDCMD_OK;
	}

	const std::string_view parmv{parm->parms[0]};
	int m{0};
	auto [ptr, ec] = std::from_chars(parmv.data(), parmv.data() + parmv.size(), m);
	
	if (ec != std::errc()) {
		// string
		for (m = 0; m < numglfiltermodes; m++) {
			if (IsSameAsNoCase(parm->parms[0], glfiltermodes[m].name)) break;
		}
		if (m == numglfiltermodes) m = gltexfiltermode;   // no change
	}
	else {
		if (m < 0) m = 0;
		else if (m >= numglfiltermodes) m = numglfiltermodes - 1;
	}

	if (m != gltexfiltermode) {
		gltexfiltermode = m;
		gltexapplyprops();
	}

	buildprintf("Texture filtering mode changed to {}\n", glfiltermodes[gltexfiltermode].name );

	return OSDCMD_OK;
}

int osdcmd_gltextureanisotropy(const osdfuncparm_t *parm)
{
	if (parm->parms.size() != 1) {
		buildprintf("Current texture anisotropy is {}\n", glanisotropy);
		buildprintf("  Maximum is {}\n", glinfo.maxanisotropy);

		return OSDCMD_OK;
	}

	const std::string_view parmv{parm->parms[0]};
	int l{0};
	std::from_chars(parmv.data(), parmv.data() + parmv.size(), l);

	// FIXME: What to do with return result?

	if (l < 0 || l > glinfo.maxanisotropy)
		l = 0;

	if (l != gltexfiltermode) {
		glanisotropy = l;
		gltexapplyprops();
	}

	buildprintf("Texture anisotropy changed to {}\n", glanisotropy );

	return OSDCMD_OK;
}

int osdcmd_forcetexcacherebuild(const osdfuncparm_t *parm)
{
	std::ignore = parm;
	PTCacheForceRebuild();
	buildprintf("Compressed texture cache invalidated. Use 'restartvid' to reinitialise it.\n");
	return OSDCMD_OK;
}

} // namespace

#endif //USE_OPENGL

namespace {

int osdcmd_polymostvars(const osdfuncparm_t *parm)
{
	const bool showval = parm->parms.size() < 1;
	int val{0};

	if(!showval) {
		const std::string_view parmv{parm->parms[0]};
		auto [ptr, ec] = std::from_chars(parmv.data(), parmv.data() + parmv.size(), val);

		// TODO: What to do with ec here?
	}

#if USE_OPENGL
	if (IsSameAsNoCase(parm->name, "usemodels")) {
		if (showval) {
			buildprintf("usemodels is {}\n", usemodels);
		}
		else
			usemodels = (val != 0);

		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "usehightile")) {
		if (showval) {
			buildprintf("usehightile is {}\n", usehightile);
		}
		else
			usehightile = (val != 0);
		
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "glusetexcompr")) {
		if (showval) { buildprintf("glusetexcompr is {}\n", glusetexcompr); }
		else glusetexcompr = (val != 0);
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "gltexcomprquality")) {
		if (showval) { buildprintf("gltexcomprquality is {}\n", gltexcomprquality); }
		else {
			if (val < 0 || val > 2) val = 0;
			gltexcomprquality = val;
		}
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "glredbluemode")) {
		if (showval) { buildprintf("glredbluemode is {}\n", glredbluemode); }
		else glredbluemode = (val != 0);
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "gltexturemaxsize")) {
		if (showval) { buildprintf("gltexturemaxsize is {}\n", gltexmaxsize); }
		else gltexmaxsize = val;
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "gltexturemiplevel")) {
		if (showval) { buildprintf("gltexturemiplevel is {}\n", gltexmiplevel); }
		else gltexmiplevel = val;
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "usegoodalpha")) {
		if (showval) { buildprintf("usegoodalpha is {}\n", usegoodalpha); }
		else usegoodalpha = (val != 0);
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "glpolygonmode")) {
		if (showval) { buildprintf("glpolygonmode is {}\n", glpolygonmode); }
		else glpolygonmode = val;
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "glusetexcache")) {
		if (showval) { buildprintf("glusetexcache is {}\n", glusetexcache); }
		else glusetexcache = (val != 0);
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "glmultisample")) {
		if (showval) {
			if (!glmultisample) buildprintf("glmultisample is {} (off)\n", glmultisample);
			else buildprintf("glmultisample is {} ({}x)\n", glmultisample, 1<<glmultisample);
		}
		else glmultisample = std::min(2, std::max(0, val));
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "glnvmultisamplehint")) {
		if (showval) { buildprintf("glnvmultisamplehint is {}\n", glnvmultisamplehint); }
		else glnvmultisamplehint = (val != 0);
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "glsampleshading")) {
		if (showval) { buildprintf("glsampleshading is {}\n", glsampleshading); }
		else glsampleshading = (val != 0);
		return OSDCMD_OK;
	}
	else if (IsSameAsNoCase(parm->name, "polymosttexverbosity")) {
		if (showval) { buildprintf("polymosttexverbosity is {}\n", polymosttexverbosity); }
		else {
			if (val < 0 || val > 2) val = 1;
			polymosttexverbosity = val;
		}
		return OSDCMD_OK;
	}
#endif //USE_OPENGL
#ifdef DEBUGGINGAIDS
	if (IsSameAsNoCase(parm->name, "debugshowcallcounts")) {
		if (showval) { buildprintf("debugshowcallcounts is {}\n", polymostshowcallcounts); }
		else polymostshowcallcounts = (val != 0);
		return OSDCMD_OK;
	}
#endif
	return OSDCMD_SHOWHELP;
}

} // namespace

void polymost_initosdfuncs()
{
#if USE_OPENGL
	OSD_RegisterFunction("usemodels","usemodels: enable/disable model rendering in >8-bit mode",osdcmd_polymostvars);
	OSD_RegisterFunction("usehightile","usehightile: enable/disable hightile texture rendering in >8-bit mode",osdcmd_polymostvars);
	OSD_RegisterFunction("glusetexcompr","glusetexcompr: enable/disable OpenGL texture compression",osdcmd_polymostvars);
	OSD_RegisterFunction("gltexcomprquality","gltexcomprquality: sets texture compression quality. 0 = fast (default), 1 = slow, 2 = very slow",osdcmd_polymostvars);
	OSD_RegisterFunction("glredbluemode","glredbluemode: enable/disable experimental OpenGL red-blue glasses mode",osdcmd_polymostvars);
	OSD_RegisterFunction("gltexturemode", "gltexturemode: changes the texture filtering settings", osdcmd_gltexturemode);
	OSD_RegisterFunction("gltextureanisotropy", "gltextureanisotropy: changes the OpenGL texture anisotropy setting", osdcmd_gltextureanisotropy);
	OSD_RegisterFunction("gltexturemaxsize","gltexturemaxsize: changes the maximum OpenGL texture size limit",osdcmd_polymostvars);
	OSD_RegisterFunction("gltexturemiplevel","gltexturemiplevel: changes the highest OpenGL mipmap level used",osdcmd_polymostvars);
	OSD_RegisterFunction("usegoodalpha","usegoodalpha: enable/disable better looking OpenGL alpha hack",osdcmd_polymostvars);
	OSD_RegisterFunction("glpolygonmode","glpolygonmode: debugging feature. 0 = normal, 1 = edges, 2 = points, 3 = clear each frame",osdcmd_polymostvars);
	OSD_RegisterFunction("glusetexcache","glusetexcache: enable/disable OpenGL compressed texture cache",osdcmd_polymostvars);
	OSD_RegisterFunction("glmultisample","glmultisample: enable/disable OpenGL (edge) multisampling. 0 = off, 1 = 2x, 2 = 4x",osdcmd_polymostvars);
	OSD_RegisterFunction("glnvmultisamplehint","glnvmultisamplehint: enable/disable Nvidia multisampling (Quincunx)",osdcmd_polymostvars);
	OSD_RegisterFunction("glsampleshading","glsampleshading: enable/disable OpenGL sample multisampling",osdcmd_polymostvars);
	OSD_RegisterFunction("polymosttexverbosity","polymosttexverbosity: sets the level of chatter during texture loading. 0 = none, 1 = errors (default), 2 = all",osdcmd_polymostvars);
	OSD_RegisterFunction("forcetexcacherebuild","forcetexcacherebuild: invalidates the compressed texture cache", osdcmd_forcetexcacherebuild);
#ifdef SHADERDEV
	OSD_RegisterFunction("debugreloadshaders","debugreloadshaders: reloads the OpenGL shaders",osdcmd_debugreloadshaders);
#endif
#ifdef DEBUGGINGAIDS
	OSD_RegisterFunction("debugdumptexturedefs","dumptexturedefs: dumps all texture definitions in the new style",osdcmd_debugdumptexturedefs);
	OSD_RegisterFunction("debugtexturehash","debugtexturehash: dumps all the entries in the texture hash",osdcmd_debugtexturehash);
	OSD_RegisterFunction("debugshowcallcounts","debugshowcallcounts: display rendering call counts",osdcmd_polymostvars);
#endif
#endif	//USE_OPENGL
}

#endif	//USE_POLYMOST

// vim:ts=4:sw=4:
