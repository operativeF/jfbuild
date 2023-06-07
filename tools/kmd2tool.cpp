#if 0
kmd2tool.exe: kmd2tool.c; cl kmd2tool.c /Ox /G6fy /MD /link /opt:nowin98
	del kmd2tool.obj
!if 0
#endif

#include <io.h>

#include <cstdio>
#include <cmath>

struct point3d { float x, y, z; };

struct md2typ
{  long id, vers, skinxsiz, skinysiz, framebytes; //id:"IPD2", vers:8
	long numskins, numverts, numuv, numtris, numglcmds, numframes;
	long ofsskins, ofsuv, ofstris, ofsframes, ofsglcmds, ofseof; //ofsskins: skin names (64 bytes each)
};

struct frametyp { point3d mul, add; };

int main (int argc, char **argv)
{
	std::FILE* fil;
	long i, leng;
	char *fbuf;
	md2typ *head;
	frametyp *fptr;

	if (argc != 4) { std::puts("KMD2TOOL [MD2 in file] [MD2 out file] [z offset]                    by Ken Silverman"); return(0); }
	if (!stricmp(argv[1],argv[2])) { std::puts("input&output filenames can't be same"); return(0); }

	fil = std::fopen(argv[1],"rb"); if (!fil) { std::puts("error"); return(0); }
	leng = filelength(_fileno(fil));
	fbuf = (char *)std::malloc(leng); if (!fbuf) { std::puts("error"); return(0); }
	fread(fbuf,leng,1,fil);
	std::fclose(fil);

	head = (md2typ *)fbuf;
	if ((head->id != 0x32504449) && (head->vers != 8)) { std::free(fbuf); std::puts("error"); return(0); } //"IDP2"
	for(i=0;i<head->numframes;i++)
	{
		fptr = (frametyp *)&fbuf[head->ofsframes+head->framebytes*i];
		std::printf("frame %2d scale:%f,%f,%f offs:%f,%f,%f\n",i,fptr->mul.x,fptr->mul.y,fptr->mul.z,fptr->add.x,fptr->add.y,fptr->add.z);
		fptr->add.z += atof(argv[3]);
	}

	fil = std::fopen(argv[2],"wb"); if (!fil) { std::puts("error"); return(0); }
	fwrite(fbuf,leng,1,fil);
	std::fclose(fil);

	std::free(fbuf);

	return(0);
}

#if 0
!endif
#endif
