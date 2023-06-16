#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif

#include <array>
#include <string_view>

extern int rendmode;
extern float gtang;
inline std::array<double, 4096> dxb1{};
inline std::array<double, 4096> dxb2{}; // FIXME: Was MAXWALLSB in size.

#ifdef DEBUGGINGAIDS
struct polymostcallcounts {
    int drawpoly_glcall;
    int drawaux_glcall;
    int drawpoly;
    int domost;
    int drawalls;
    int drawmaskwall;
    int drawsprite;
};
extern struct polymostcallcounts polymostcallcounts;
#endif

enum {
    METH_SOLID   = 0,
    METH_MASKED  = 1,
    METH_TRANS   = 2,
    METH_INTRANS = 3,

    METH_CLAMPED = 4,
    METH_LAYERS  = 8,       // when given to drawpoly, renders the additional texture layers
    METH_POW2XSPLIT = 16,   // when given to drawpoly, splits polygons for non-2^x-capable GL devices
    METH_ROTATESPRITE = 32, // when given to drawpoly, use the rotatesprite projection matrix
};

#if USE_OPENGL

#include "glbuild_priv.hpp"

struct coltype {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct coltypef {
    GLfloat r;
    GLfloat g;
    GLfloat b;
    GLfloat a;
};

extern float glox1;
extern float gloy1;
extern double gxyaspect;
extern double grhalfxdown10x;
extern double gcosang;
extern double gsinang;
extern double gcosang2;
extern double gsinang2;
extern double gchang;
extern double gshang;
extern double gctang;
extern double gstang;
extern int gfogpalnum;
extern float gfogdensity;

struct glfiltermode {
	const char* name;
	int min;
    int mag;
};

inline constexpr auto numglfiltermodes{6};

inline constexpr std::array<glfiltermode, numglfiltermodes> glfiltermodes = {{
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
}};

inline int gltexcomprquality{0};	// 0 = fast, 1 = slow and pretty, 2 = very slow and pretty
inline int gltexmaxsize{0};	// 0 means autodetection on first run
inline int gltexmiplevel{0};	// discards this many mipmap levels

inline constexpr std::array<std::array<GLfloat, 4>, 4> gidentitymat = {{
	{1.F, 0.F, 0.F, 0.F},
	{0.F, 1.F, 0.F, 0.F},
	{0.F, 0.F, 1.F, 0.F},
	{0.F, 0.F, 0.F, 1.F},
}};

extern GLfloat gdrawroomsprojmat[4][4];      // Proj. matrix for drawrooms() calls.
extern GLfloat grotatespriteprojmat[4][4];   // Proj. matrix for rotatesprite() calls.
extern GLfloat gorthoprojmat[4][4];          // Proj. matrix for 2D (aux) calls.

struct polymostvboitem {
    struct {    // Vertex
        GLfloat x;
        GLfloat y;
        GLfloat z;
    } v;
    struct {    // Texture
        GLfloat s;
        GLfloat t;
    } t;
};

struct polymostdrawpolycall {
    GLuint texture0;
    GLuint texture1;
    GLfloat alphacut;
    coltypef colour;
    coltypef fogcolour;
    GLfloat fogdensity;

    const GLfloat *modelview;     // 4x4 matrices.
    const GLfloat *projection;

    GLuint indexbuffer;     // Buffer object identifier, or 0 for the global index buffer.
    GLuint indexcount;      // Number of index items.

    GLuint elementbuffer;   // Buffer object identifier. >0 ignores elementvbo.
    GLuint elementcount;    // Number of elements in the element buffer. Ignored if elementbuffer >0.
    const struct polymostvboitem *elementvbo; // Elements. elementbuffer must be 0 to recognise this.
};

// Smallest initial size for the global index buffer.
inline constexpr auto MINVBOINDEXES{16};

struct polymostdrawauxcall {
    GLuint texture0;
    coltypef colour;
    coltypef bgcolour;
    int mode;

    GLuint indexcount;      // Number of index items.
    GLushort *indexes;      // Array of indexes, or nullptr to use the global index buffer.

    int elementcount;
    struct polymostvboitem *elementvbo;
};

void polymost_drawpoly_glcall(GLenum mode, struct polymostdrawpolycall const *draw);

bool polymost_texmayhavealpha (int dapicnum, int dapalnum);
void polymost_texinvalidate (int dapicnum, int dapalnum, int dameth);
void polymost_texinvalidateall ();
void polymost_glinit();
int polymost_printext256(int xpos, int ypos, short col, short backcol, std::string_view name, char fontsize);
int polymost_drawline256(int x1, int y1, int x2, int y2, unsigned char col);
int polymost_plotpixel(int x, int y, unsigned char col);
void polymost_fillpolygon (int npoints);
void polymost_setview();

#endif //USE_OPENGL

void polymost_nextpage();
void polymost_aftershowframe();
void polymost_drawrooms ();
void polymost_drawmaskwall (int damaskwallcnt);
void polymost_drawsprite (int snum);
void polymost_dorotatesprite (int sx, int sy, int z, short a, short picnum,
    signed char dashade, unsigned char dapalnum, unsigned char dastat, int cx1, int cy1, int cx2, int cy2, int uniqid);
void polymost_initosdfuncs();
