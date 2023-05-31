#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef MDSPRITE_PRIV_H
#define MDSPRITE_PRIV_H

struct mdskinmap_t
{
	unsigned char palette, filler[3]; // Build palette number
	int skinnum, surfnum;   // Skin identifier, surface number
	char *fn;   // Skin filename
	PTMHead *tex[HICEFFECTMASK+1];
	mdskinmap_t* next;
};

struct mdmodel
{
	int mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	int shadeoff;
	float scale, bscale, zadd;
};

struct mdanim_t
{
	int startframe, endframe;
	int fpssc, flags;
	mdanim_t* next;
};

#define MDANIM_LOOP 0
#define MDANIM_ONESHOT 1


	//This MD2 code is based on the source code from David Henry (tfc_duke(at)hotmail.com)
	//   Was at http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//   Available from http://web.archive.org/web/20030816010242/http://tfc.duke.free.fr/us/tutorials/models/md2.htm
	//   Now at http://tfc.duke.free.fr/coding/md2.html (in French)
	//He probably wouldn't recognize it if he looked at it though :)
struct point3d { float x, y, z; };

struct md2head_t
{
	int id, vers, skinxsiz, skinysiz, framebytes; //id:"IPD2", vers:8
	int numskins, numverts, numuv, numtris, numglcmds, numframes;
	int ofsskins, ofsuv, ofstris, ofsframes, ofsglcmds, ofseof; //ofsskins: skin names (64 bytes each)
};

struct md2vert_t { unsigned char v[3], ni; }; //compressed vertex coords (x,y,z), light normal index
struct md2uv_t { short u, v; };	//compressed texture coords
struct md2tri_t { short ivert[3], iuv[3]; };	//indices of vertices and tex coords for each point of a triangle

struct md2frame_t
{
	point3d mul, add; //scale&translation vector
	char name[16];    //frame name
	md2vert_t verts[1]; //first vertex of this frame
};

struct md2model
{
		//common between mdmodel/voxmodel/md2model/md3model
	int mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	int shadeoff;
	float scale, bscale, zadd;

		//MD2 and MD3 share these
	PTMHead **tex;   // textures for base skin if no mappings defined
	int numframes, cframe, nframe, fpssc, usesalpha;
	float oldtime, curtime, interpol;
	mdanim_t *animations;
	mdskinmap_t *skinmap;
	int numskins, skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,

		//MD2 specific stuff:
	int numverts, numuv, numtris, framebytes;
	int skinxsiz, skinysiz;
	char *frames;
	md2uv_t *uvs;
	md2tri_t *tris;
	char *basepath;   // pointer to string of base path
	char *skinfn;   // pointer to first of numskins 64-char strings
};


struct md3shader_t { char nam[64]; int i; }; //ascz path of shader, shader index
struct md3tri_t { int i[3]; }; //indices of tri
struct md3uv_t { float u, v; };
struct md3xyzn_t { signed short x, y, z; unsigned char nlat, nlng; }; //xyz are [10:6] ints

struct md3frame_t
{
	point3d min, max, cen; //bounding box&origin
	float r; //radius of bounding sphere
	char nam[16]; //ascz frame name
};

struct md3tag_t
{
	char nam[64]; //ascz tag name
	point3d p, x, y, z; //tag object pos&orient
};

struct md3surf_t
{
	int id; //IDP3(0x33806873)
	char nam[64]; //ascz surface name
	int flags; //?
	int numframes, numshaders, numverts, numtris; //numframes same as md3head,max shade=~256,vert=~4096,tri=~8192
	md3tri_t *tris;       //file format: rel offs from md3surf
	md3shader_t *shaders; //file format: rel offs from md3surf
	md3uv_t *uv;          //file format: rel offs from md3surf
	md3xyzn_t *xyzn;      //file format: rel offs from md3surf
	int ofsend;
};

struct md3filesurf_t
{
	int id; //IDP3(0x33806873)
	char nam[64]; //ascz surface name
	int flags; //?
	int numframes, numshaders, numverts, numtris; //numframes same as md3head,max shade=~256,vert=~4096,tri=~8192
	int tris;       //file format: rel offs from md3surf
	int shaders;    //file format: rel offs from md3surf
	int uv;         //file format: rel offs from md3surf
	int xyzn;       //file format: rel offs from md3surf
	int ofsend;
};

struct md3head_t
{
	int id, vers; //id=IDP3(0x33806873), vers=15
	char nam[64]; //ascz path in PK3
	int flags; //?
	int numframes, numtags, numsurfs, numskins; //max=~1024,~16,~32,numskins=artifact of MD2; use shader field instead
	md3frame_t *frames; //file format: abs offs
	md3tag_t *tags;     //file format: abs offs
	md3surf_t *surfs;   //file format: abs offs
	int eof;           //file format: abs offs
};

struct md3filehead_t
{
	int id, vers; //id=IDP3(0x33806873), vers=15
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
	float scale, bscale, zadd;

		//MD2 and MD3 share these
	PTMHead **tex;   // textures for base skin if no mappings defined
	int numframes, cframe, nframe, fpssc, usesalpha;
	float oldtime, curtime, interpol;
	mdanim_t *animations;
	mdskinmap_t *skinmap;
	int numskins, skinloaded;   // set to 1+numofskin when a skin is loaded and the tex coords are modified,

		//MD3 specific
	md3head_t head;
};

struct tile2model_t
{ // maps build tiles to particular animation frames of a model
	int modelid;
	int skinnum;
	int framenum;   // calculate the number from the name when declaring
};

extern tile2model_t tile2model[MAXTILES];

struct hudtyp { float xadd, yadd, zadd; short angadd, flags; };
extern hudtyp hudmem[2][MAXTILES];

#define VOXUSECHAR 0
#if (VOXUSECHAR != 0)
struct vert_t { unsigned char x, y, z, u, v; };
#else
struct vert_t { unsigned short x, y, z, u, v; };
#endif
struct voxrect_t { vert_t v[4]; };
struct voxmodel
{
		//common between mdmodel/voxmodel/md2model/md3model
	int mdnum; //VOX=1, MD2=2, MD3=3. NOTE: must be first in structure!
	int shadeoff;
	float scale, bscale, zadd;

		//VOX specific stuff:
	GLuint *texid;	// skins for palettes
	voxrect_t *quad; int qcnt, qfacind[7];
	int *mytex, mytexx, mytexy;
	int xsiz, ysiz, zsiz;
	float xpiv, ypiv, zpiv;
	int is8bit;

	GLuint vertexbuf;		// 4 per quad.
	GLuint indexbuf;		// 6 per quad (0, 1, 2, 0, 2, 3)
	unsigned int indexcount;
};

extern voxmodel *voxmodels[MAXVOXELS];
extern mdmodel **models;

extern char mdinited;
extern int mdtims, omdtims;
extern int nextmodelid;

void freeallmodels ();
void clearskins ();
void voxfree (voxmodel *m);
voxmodel *voxload (const char *filnam);
int voxdraw (voxmodel *m, const spritetype *tspr, int method);

void mdinit ();
PTMHead * mdloadskin (md2model *m, int number, int pal, int surf);
int mddraw (spritetype *, int method);

#endif
