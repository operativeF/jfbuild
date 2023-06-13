#ifndef __screencapture_h__
#define __screencapture_h__

#include <cstdio>

//
// screencapture
//

inline char captureatnextpage{0};

std::FILE *screencapture_openfile(const char *ext);

int screencapture_writeframe(std::FILE *fil, char mode, void *v,
	void (*writeline)(unsigned char *, int, int, std::FILE *, void *));

void screencapture_writetgaline(unsigned char *buf, int bytes, int elements, std::FILE *fp, void *v);

int screencapture_tga(char mode);

// PCX is nasty, which is why I've lifted these functions from the PCX spec by ZSoft
int writepcxbyte(unsigned char colour, unsigned char count, std::FILE *fp);

void writepcxline(unsigned char *buf, int bytes, int step, std::FILE* fp);

void screencapture_writepcxline(unsigned char *buf, int bytes, int elements, std::FILE *fp, void *v);

int screencapture_pcx(char mode);

struct pngsums {
	unsigned int crc;
	unsigned short adlers1;
	unsigned short adlers2;
};

void screencapture_writepngline(unsigned char *buf, int bytes, int elements, std::FILE* fp, void *v);

int screencapture_png(char mode);

int screencapture(const char* filename, char mode);

#endif // __screencapture_h__