#if (USE_POLYMOST == 0)
#error Polymost not enabled.
#endif
#if (USE_OPENGL == 0)
#error OpenGL not enabled.
#endif

#ifndef POLYMOSTTEXCACHE_H
#define POLYMOSTTEXCACHE_H

#include <memory>

struct PTCacheTileMip_typ {
	int sizx{0};
	int sizy{0};
	int length{0};
	unsigned char * data{nullptr};
};
typedef struct PTCacheTileMip_typ PTCacheTileMip;

struct PTCacheTile_typ {
	std::string filename;
	int effects{0};
	int flags{0};
	int format{0};	// OpenGL format code
	int tsizx{0};
	int tsizy{0};
	int nummipmaps{0};
	PTCacheTileMip mipmap[1]{};
};
typedef struct PTCacheTile_typ PTCacheTile;

/**
 * Loads the cache index file into memory
 */
void PTCacheLoadIndex();

/**
 * Unloads the cache index from memory
 */
void PTCacheUnloadIndex();

/**
 * Loads a tile from the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return a PTCacheTile entry fully completed
 */
std::unique_ptr<PTCacheTile> PTCacheLoadTile(const std::string& filename, int effects, int flags);

/**
 * Checks to see if a tile exists in the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return !0 if it exists
 */
bool PTCacheHasTile(const std::string& filename, int effects, int flags);

/**
 * Disposes of the resources allocated for a PTCacheTile
 * @param tdef a PTCacheTile entry
 */
void PTCacheFreeTile(PTCacheTile * tdef);

/**
 * Allocates the skeleton of a PTCacheTile entry for you to complete.
 * Memory for filenames and mipmap data should be allocated using malloc()
 * @param nummipmaps allocate mipmap entries for nummipmaps items
 * @return a PTCacheTile entry
 */
std::unique_ptr<PTCacheTile> PTCacheAllocNewTile(int nummipmaps);

/**
 * Stores a PTCacheTile into the cache.
 * @param tdef a PTCacheTile entry fully completed
 * @return !0 on success
 */
int PTCacheWriteTile(const PTCacheTile * tdef);

/**
 * Forces the cache to be rebuilt.
 */
void PTCacheForceRebuild();

#endif
