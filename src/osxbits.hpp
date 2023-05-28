#ifndef __osxbits_h__
#define __osxbits_h__

char *osx_gethomedir();
char *osx_getappdir();
char *osx_getsupportdir(int global);

int wmosx_filechooser(const char *initialdir, const char *initialfile, const char *type, int foropen, char **choice);

#endif
