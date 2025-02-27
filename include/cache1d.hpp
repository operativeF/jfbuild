// cache1d.h

#ifndef __cache1d_h__
#define __cache1d_h__

#include <string>

void	initcache(void *dacachestart, size_t dacachesize);
void	allocache(void **newhandle, size_t newbytes, unsigned char *newlockptr);
void	suckcache(void *suckptr);
void	agecache();

enum {
	PATHSEARCH_GAME  = 0, 	// default
	PATHSEARCH_SYSTEM = 1,

	KOPEN4LOAD_ANY = 0,
	KOPEN4LOAD_FIRSTGRP = 1,
	KOPEN4LOAD_ANYGRP = 2,
};

inline int cachecount{0};
inline int pathsearchmode{0};

int     addsearchpath(const char *p);
int		findfrompath(const char *fn, std::string& where);
int     openfrompath(const char *fn, int flags, int mode);
std::FILE  *fopenfrompath(const char *fn, const char *mode);

int 	initgroupfile(const std::string& filename);
void	uninitsinglegroupfile(int grphandle);
void	uninitgroupfile();
int 	kopen4load(const char *filename, char searchfirst);	// searchfirst: 0 = anywhere, 1 = first group, 2 = any group
int 	kread(int handle, void *buffer, unsigned leng);
int 	kgetc(int handle);
int 	klseek(int handle, int offset, int whence);
int 	kfilelength(int handle);
int 	ktell(int handle);
void	kclose(int handle);

enum {
	CACHE1D_FIND_FILE = 1,
	CACHE1D_FIND_DIR = 2,
	CACHE1D_FIND_DRIVE = 4,

	CACHE1D_OPT_NOSTACK = 0x100,
	
	// the lower the number, the higher the priority
	CACHE1D_SOURCE_DRIVE = 0,
	CACHE1D_SOURCE_CURDIR = 1,
	CACHE1D_SOURCE_PATH = 2,	// + path stack depth
	CACHE1D_SOURCE_ZIP = 0x7ffffffe,
	CACHE1D_SOURCE_GRP = 0x7fffffff,
};

struct CACHE1D_FIND_REC {
	std::string name;
	int type;
	int source;
	CACHE1D_FIND_REC* next;
	CACHE1D_FIND_REC* prev;
	CACHE1D_FIND_REC* usera;
	CACHE1D_FIND_REC* userb;
};

void klistfree(CACHE1D_FIND_REC *rec);
CACHE1D_FIND_REC *klistpath(const char *path, const char *mask, int type);

unsigned kdfread(void *buffer, unsigned dasizeof, unsigned count, int fil);
unsigned dfread(void *buffer, unsigned dasizeof, unsigned count, std::FILE *fil);
unsigned kdfwrite(void *buffer, unsigned dasizeof, unsigned count, int fil);
unsigned dfwrite(void *buffer, unsigned dasizeof, unsigned count, std::FILE *fil);

#endif // __cache1d_h__

