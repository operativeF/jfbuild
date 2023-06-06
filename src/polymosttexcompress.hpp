#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef POLYMOSTTEXCOMPRESS_H
#define POLYMOSTTEXCOMPRESS_H

enum {
	PTCOMPRESS_NONE = 0,
	PTCOMPRESS_DXT1 = 1,
	PTCOMPRESS_DXT5 = 2,
	PTCOMPRESS_ETC1 = 3,
};
int ptcompress_getstorage(int width, int height, int format);
int ptcompress_compress(void * bgra, int width, int height, unsigned char * output, int format);

#endif
