// Base services interface declaration, private-facing
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __baselayer_priv_h__
#define __baselayer_priv_h__

inline char modechange{1};
inline char videomodereset{0};

// undefine to restrict windowed resolutions to conventional sizes
#define ANY_WINDOWED_SIZE

int baselayer_init();

int initsystem();
void uninitsystem();

#if USE_OPENGL
// FIXME: Reverse logic.
inline bool glunavailable{false};

bool loadgldriver(const char *driver);   // or nullptr for platform default
void *getglprocaddress(const char *name, int ext);
int unloadgldriver();
#endif

#endif // __baselayer_priv_h__
