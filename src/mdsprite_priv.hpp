#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef MDSPRITE_PRIV_H
#define MDSPRITE_PRIV_H

#include <array>
#include <memory>
#include <string>
#include <vector>

struct mdskinmap_t
{
	unsigned char palette; // Build palette number
	std::array<unsigned char, 3> filler;
	int skinnum;  // Skin identifier, surface number
	int surfnum;
	std::string fn;   // Skin filename
	std::array<PTMHead*, HICEFFECTMASK + 1> tex;
	mdskinmap_t* next;
};

struct mdmodel
{
	int mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	int shadeoff;
	float scale;
	float bscale;
	float zadd;
};

struct mdanim_t
{
	int startframe{0};
	int endframe{0};
	int fpssc{0};
	int flags{0};
};

#define MDANIM_LOOP 0
#define MDANIM_ONESHOT 1


	//This MD2 code is based on the source code from David Henry (tfc_duke(at)hotmail.com)
	//   Was at http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//   Available from http://web.archive.org/web/20030816010242/http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//   Now at http://tfc.duke.free.fr/coding/md2.html (in French)
	//He probably wouldn't recognize it if he looked at it though :)
struct point3d {
	float x;
	float y;
	float z;

	constexpr point3d& operator+=(const point3d& pt) {
		x += pt.x;
		y += pt.y;
		z += pt.z;
		return *this;
	}

	constexpr point3d& operator-() {
		x = -x;
		y = -y;
		z = -z;
		return *this;
	}

	constexpr point3d& operator-=(const point3d& pt) {
		x -= pt.x;
		y -= pt.y;
		z -= pt.z;
		return *this;
	}

	constexpr point3d& operator*=(auto val) {
		x *= val;
		y *= val;
		z *= val;
		return *this;
	}

	constexpr point3d& operator/=(auto val) {
		x /= val;
		y /= val;
		z /= val;
		return *this;
	}

};

inline constexpr point3d operator+(point3d lhp, const point3d& rhp) {
	lhp += rhp;
	return lhp;
}

inline constexpr point3d operator-(point3d lhp, const point3d& rhp) {
	lhp -= rhp;
	return lhp;
}

// TODO: Add constraints
inline constexpr point3d operator*(point3d pt, auto val) {
	pt *= val;
	return pt;
}

inline constexpr point3d operator*(auto val, point3d pt) {
	return pt * val;
}

inline constexpr point3d operator/(point3d pt, auto val) {
	pt /= val;
	return pt;
}

struct md2head_t
{
	int id; //id:"IPD2", vers:8
	int vers;
	int skinxsiz;
	int skinysiz;
	int framebytes;
	int numskins;
	int numverts;
	int numuv;
	int numtris;
	int numglcmds;
	int numframes;
	int ofsskins; //ofsskins: skin names (64 bytes each)
	int ofsuv;
	int ofstris;
	int ofsframes;
	int ofsglcmds;
	int ofseof;
};

struct md2vert_t {
	std::array<unsigned char, 3> v;
	unsigned char ni;
}; //compressed vertex coords (x,y,z), light normal index

struct md2uv_t {
	short u;
	short v;
};	//compressed texture coords

struct md2tri_t {
	std::array<short, 3> ivert;
	std::array<short, 3> iuv;
};	//indices of vertices and tex coords for each point of a triangle

// FIXME: Why use single element array?
struct md2frame_t
{
	point3d mul;
	point3d add; //scale&translation vector
	char name[16];    //frame name
	std::array<md2vert_t, 1> verts; //first vertex of this frame
};

struct md2model
{
		//common between mdmodel/voxmodel/md2model/md3model
	int mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	int shadeoff;
	float scale;
	float bscale;
	float zadd;

		//MD2 and MD3 share these
	PTMHead **tex;   // textures for base skin if no mappings defined
	int numframes;
	int cframe;
	int nframe;
	int fpssc;
	int usesalpha;
	float oldtime;
	float curtime;
	float interpol;
	std::vector<mdanim_t> animations;
	std::vector<mdskinmap_t> skinmap;
	int numskins;
	int skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,

		//MD2 specific stuff:
	int numverts;
	int numuv;
	int numtris;
	int framebytes;
	int skinxsiz;
	int skinysiz;
	std::vector<char> frames;
	std::vector<md2uv_t> uvs;
	std::vector<md2tri_t> tris;
	std::string basepath;
	char *skinfn;   // pointer to first of numskins 64-char strings
};


struct md3shader_t {
	std::array<char, 64> nam;
	int i;
}; //ascz path of shader, shader index

struct md3tri_t {
	std::array<int, 3> i;
}; //indices of tri

struct md3uv_t {
	float u;
	float v;
};

struct md3xyzn_t {
	signed short x;
	signed short y;
	signed short z;
	unsigned char nlat;
	unsigned char nlng;
}; //xyz are [10:6] ints

struct md3frame_t
{
	point3d min;
	point3d max;
	point3d cen; //bounding box&origin
	float r; //radius of bounding sphere
	char nam[16]; //ascz frame name
};

struct md3tag_t
{
	char nam[64]; //ascz tag name
	point3d p;
	point3d x;
	point3d y;
	point3d z; //tag object pos&orient
};

struct md3surf_t
{
	int id; //IDP3(0x33806873)
	std::string nam; //ascz surface name
	int flags; //?
	int numframes;
	int numshaders;
	int numverts;
	int numtris; //numframes same as md3head,max shade=~256,vert=~4096,tri=~8192
	std::vector<md3tri_t> tris;       //file format: rel offs from md3surf
	md3shader_t *shaders; //file format: rel offs from md3surf
	md3uv_t *uv;          //file format: rel offs from md3surf
	md3xyzn_t *xyzn;      //file format: rel offs from md3surf
	int ofsend;
};

struct md3filesurf_t
{
	int id; //IDP3(0x33806873)
	std::string nam; //ascz surface name
	int flags; //?
	int numframes;
	int numshaders;
	int numverts;
	int numtris; //numframes same as md3head,max shade=~256,vert=~4096,tri=~8192
	int tris;       //file format: rel offs from md3surf
	int shaders;    //file format: rel offs from md3surf
	int uv;         //file format: rel offs from md3surf
	int xyzn;       //file format: rel offs from md3surf
	int ofsend;
};

struct md3head_t
{
	int id;
	int vers; //id=IDP3(0x33806873), vers=15
	std::string nam; //ascz path in PK3
	int flags; //?
	int numframes;
	int numtags;
	int numsurfs;
	int numskins; //max=~1024,~16,~32,numskins=artifact of MD2; use shader field instead
	std::vector<md3frame_t> frames; //file format: abs offs
	std::vector<md3tag_t> tags;     //file format: abs offs
	std::vector<md3surf_t> surfs;   //file format: abs offs
	int eof;           //file format: abs offs
};

struct md3filehead_t
{
	int id;
	int vers; //id=IDP3(0x33806873), vers=15
	char nam[64]; //ascz path in PK3
	int flags; //?
	int numframes, numtags, numsurfs, numskins; //max=~1024,~16,~32,numskins=artifact of MD2; use shader field instead
	int frames; //file format: abs offs
	int tags;     //file format: abs offs
	int surfs;   //file format: abs offs
	int eof;           //file format: abs offs
};

struct md3model
{
		//common between mdmodel/voxmodel/md2model/md3model
	int mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	int shadeoff;
	float scale;
	float bscale;
	float zadd;

		//MD2 and MD3 share these
	PTMHead **tex;   // textures for base skin if no mappings defined
	int numframes;
	int cframe;
	int nframe;
	int fpssc;
	int usesalpha;
	float oldtime;
	float curtime;
	float interpol;
	std::vector<mdanim_t> animations;
	std::vector<mdskinmap_t> skinmap;
	int numskins;
	int skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,

		//MD3 specific
	md3head_t head;
};

struct tile2model_t
{ // maps build tiles to particular animation frames of a model
	int modelid;
	int skinnum;
	int framenum;   // calculate the number from the name when declaring
};

inline std::array<tile2model_t, MAXTILES> tile2model;

struct hudtyp {
	float xadd;
	float yadd;
	float zadd;
	short angadd;
	short flags;
};

extern hudtyp hudmem[2][MAXTILES];

#define VOXUSECHAR 0
#if (VOXUSECHAR != 0)
struct vert_t {
	unsigned char x;
	unsigned char y;
	unsigned char z;
	unsigned char u;
	unsigned char v;
};
#else
struct vert_t {
	unsigned short x;
	unsigned short y;
	unsigned short z;
	unsigned short u;
	unsigned short v;
};
#endif

struct voxrect_t {
	std::array<vert_t, 4> v;
};

struct voxmodel
{
		//common between mdmodel/voxmodel/md2model/md3model
	int mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	int shadeoff;
	float scale;
	float bscale;
	float zadd;

		//VOX specific stuff:
	GLuint *texid;	// skins for palettes
	std::vector<voxrect_t> quad;
	int qcnt;
	std::array<int, 7> qfacind;
	std::vector<int> mytex;
	int mytexx;
	int mytexy;
	int xsiz;
	int ysiz;
	int zsiz;
	float xpiv;
	float ypiv;
	float zpiv;
	int is8bit;

	GLuint vertexbuf;		// 4 per quad.
	GLuint indexbuf;		// 6 per quad (0, 1, 2, 0, 2, 3)
	unsigned int indexcount;
};

extern std::array<std::unique_ptr<voxmodel>, MAXVOXELS> voxmodels;
extern mdmodel **models;

extern char mdinited;
extern int mdtims, omdtims;
extern int nextmodelid;

void freeallmodels ();
void clearskins ();
void voxfree (voxmodel *m);
std::unique_ptr<voxmodel> voxload (const std::string& filnam);
int voxdraw (voxmodel *m, const spritetype *tspr, int method);

void mdinit ();
PTMHead * mdloadskin (md2model *m, int number, int pal, int surf);
int mddraw (spritetype *, int method);

#endif
