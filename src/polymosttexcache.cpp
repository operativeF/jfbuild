#include "build.hpp"

#if USE_POLYMOST && USE_OPENGL

#include "polymosttexcache.hpp"
#include "baselayer.hpp"
#include "glbuild_priv.hpp"
#include "hightile_priv.hpp"
#include "polymosttex_priv.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>


/*
 PolymostTex Cache file formats

 INDEX (texture.cacheindex):
   signature  "PolymostTexIndx"
   version    CACHEVER
   ENTRIES...
     filename  char[PTCACHEINDEXFILENAMELEN]
     effects   int32
     flags     int32		PTH_CLAMPED
     offset    uint32		Offset from the start of the STORAGE file
     mtime     int32		When kplib can return mtimes from ZIPs, this will be used to spot stale entries

 STORAGE (texture.cache):
   signature  "PolymostTexStor"
   version    CACHEVER
   ENTRIES...
     tsizx     int32		Unpadded dimensions
     tsizy     int32
     flags     int32		PTH_CLAMPED | PTH_HASALPHA
     format    int32		OpenGL compressed format code
     nmipmaps  int32		The number of mipmaps following
     MIPMAPS...
       sizx    int32		Padded dimensions
       sizy    int32
       length  int32
       data    char[length]

 All multibyte values are little-endian.
 */

struct PTCacheIndex_typ {
	std::string filename;
	int effects;
	int flags;
	unsigned offset;
	struct PTCacheIndex_typ* next;
};

typedef struct PTCacheIndex_typ PTCacheIndex;
constexpr auto PTCACHEHASHSIZ{512};
static std::array<PTCacheIndex*, PTCACHEHASHSIZ> cachehead;	// will be initialized 0 by .bss segment
constexpr auto PTCACHEINDEXFILENAMELEN{260};

static constexpr int CACHEVER{ 0 };
static constexpr std::array<int8_t, 16> indexsig = { 'P','o','l','y','m','o','s','t','T','e','x','I','n','d','x',CACHEVER };
static constexpr std::array<int8_t, 16> storagesig = { 'P','o','l','y','m','o','s','t','T','e','x','S','t','o','r',CACHEVER };

static constexpr char CACHEINDEXFILE[]   = "texture.cacheindex";
static constexpr char CACHESTORAGEFILE[] = "texture.cache";

static bool cachedisabled{false};
static bool cachereplace{false};

static unsigned int gethashhead(const std::string& filename)
{
	// implements the djb2 hash, constrained to the hash table size
	// http://www.cse.yorku.ca/~oz/hash.html
	unsigned long hash = 5381;

	for(auto ch : filename) {
		hash = ((hash << 5) + hash) ^ static_cast<int>(ch);
	}

    return hash & (PTCACHEHASHSIZ-1);
}

/**
 * Adds an item to the cachehead hash.
 * @param filename
 * @param effects
 * @param flags
 * @param offset
 */
static void ptcache_addhash(const std::string& filename, int effects, int flags, unsigned offset)
{
	const unsigned int hash = gethashhead(filename);

	// to reduce memory fragmentation we tack the filename onto the end of the block
	PTCacheIndex* pci = (PTCacheIndex *) std::malloc(sizeof(PTCacheIndex));

	pci->filename = filename;
	pci->effects = effects;
	pci->flags   = flags & (PTH_CLAMPED);
	pci->offset  = offset;
	pci->next = cachehead[hash];

	cachehead[hash] = pci;
}

/**
 * Locates an item in the cachehead hash.
 * @param filename
 * @param effects
 * @param flags
 * @return the PTCacheIndex item, or null
 */
static PTCacheIndex* ptcache_findhash(const std::string& filename, int effects, int flags)
{
	PTCacheIndex* pci = cachehead[ gethashhead(filename) ];

	if (!pci) {
		return nullptr;
	}

	flags &= PTH_CLAMPED;

	while (pci) {
		if (effects == pci->effects &&
		    flags == pci->flags &&
		    (pci->filename == filename)) {
			return pci;
		}
		pci = pci->next;
	}

	return nullptr;
}

/**
 * Loads the cache index file into memory
 */
void PTCacheLoadIndex()
{
	std::array<char, PTCACHEINDEXFILENAMELEN> filename{};

	int32_t effects;
	int32_t flags;
	uint32_t offset;
	int32_t mtime;
	PTCacheIndex * pci;

	int total{0};
	int dups{0};
	int haveindex{0};
	int havestore{0};

	// first, check the cache storage file's signature.
	// we open for reading and writing to test permission
	std::FILE* fh = std::fopen(CACHESTORAGEFILE, "r+b");

	std::array<int8_t, 16> sig;

	if (fh) {
		havestore = 1;

		if (std::fread(&sig[0], 16, 1, fh) != 1 || std::memcmp(&sig[0], &storagesig[0], 16)) {
			cachereplace = true;
		}
		
		std::fclose(fh);
	}
	else {
		if (errno == ENOENT) {
			// file doesn't exist, which is fine
			;
		}
		else {
			buildprintf("PolymostTexCache: error opening {}, texture cache disabled\n", CACHESTORAGEFILE);
			cachedisabled = true;
			return;
		}
	}

	// next, check the index
	fh = std::fopen(CACHEINDEXFILE, "r+b");
	if (fh) {
		haveindex = 1;

		if (std::fread(&sig[0], 16, 1, fh) != 1 || std::memcmp(&sig[0], &indexsig[0], 16)) {
			cachereplace = true;
		}
	} else {
		if (errno == ENOENT) {
			// file doesn't exist, which is fine
			return;
		} else {
			buildprintf("PolymostTexCache: error opening {}, texture cache disabled\n", CACHEINDEXFILE);
			cachedisabled = true;
			return;
		}
	}

	// if we're missing either the index or the store, but not both at the same
	// time, the cache is broken and should be replaced
	if ((!haveindex || !havestore) && !(!haveindex && !havestore)) {
		cachereplace = true;
	}

	if (cachereplace) {
		buildprintf("PolymostTexCache: texture cache will be replaced\n");
		if (fh) {
			std::fclose(fh);
		}
		return;
	}

	// now that the index is sitting at the first entry, load everything
	while (!std::feof(fh)) {
		if (std::fread(&filename[0], PTCACHEINDEXFILENAMELEN, 1, fh) != 1 && std::feof(fh)) {
			break;
		}
		if (std::fread(&effects, 4,         1, fh) != 1 ||
		    std::fread(&flags,   4,         1, fh) != 1 ||
		    std::fread(&offset,  4,         1, fh) != 1 ||
		    std::fread(&mtime,   4,         1, fh) != 1) {
			// truncated entry, so throw the whole cache away
			buildprintf("PolymostTexCache: corrupt texture cache index detected, cache will be replaced\n");
			cachereplace = true;
			PTCacheUnloadIndex();
			break;
		}

		effects = B_LITTLE32(effects);
		flags   = B_LITTLE32(flags);
		offset  = B_LITTLE32(offset);
		mtime   = B_LITTLE32(mtime);

		filename[sizeof(filename)-1] = 0;
		pci = ptcache_findhash(&filename[0], (int) effects, (int) flags);
		if (pci) {
			// superseding an old hash entry
			pci->offset = offset;
			dups++;
		} else {
			ptcache_addhash(&filename[0], (int) effects, (int) flags, offset);
		}
		total++;
	}

	std::fclose(fh);

	buildprintf("PolymostTexCache: cache index loaded ({} entries, {} old entries skipped)\n", total, dups);
}

/**
 * Unloads the cache index from memory
 */
void PTCacheUnloadIndex()
{

	for (auto& pci : cachehead) {
		while (pci) {
			PTCacheIndex* next = pci->next;
			// we needn't free pci->filename since it was alloced with pci
			std::free(pci);
			pci = next;
		}
		
		pci = nullptr;
	}

	buildprintf("PolymostTexCache: cache index unloaded\n");
}

/**
 * Does the task of loading a tile from the cache
 * @param offset the starting offset
 * @return a PTCacheTile entry fully completed
 */
static std::unique_ptr<PTCacheTile> ptcache_load(off_t offset)
{
	int32_t tsizx;
	int32_t tsizy;
	int32_t sizx;
	int32_t sizy;
	int32_t flags;
	int32_t format;
	int32_t nmipmaps;
	int32_t i;
	int32_t length;

	std::unique_ptr<PTCacheTile> tdef;

	if (cachereplace) {
		// cache is in a broken state, so don't try loading
		return nullptr;
	}

	std::FILE* fh = std::fopen(CACHESTORAGEFILE, "rb");

	if (!fh) {
		cachedisabled = true;
		buildprintf("PolymostTexCache: error opening {}, texture cache disabled\n", CACHESTORAGEFILE);
		return nullptr;
	}

	std::fseek(fh, offset, SEEK_SET);

	if (std::fread(&tsizx, 4, 1, fh) != 1 ||
	    std::fread(&tsizy, 4, 1, fh) != 1 ||
	    std::fread(&flags, 4, 1, fh) != 1 ||
	    std::fread(&format, 4, 1, fh) != 1 ||
	    std::fread(&nmipmaps, 4, 1, fh) != 1) {
		// truncated entry, so throw the whole cache away
		goto fail;
	}

	tsizx = B_LITTLE32(tsizx);
	tsizy = B_LITTLE32(tsizy);
	flags = B_LITTLE32(flags);
	format = B_LITTLE32(format);
	nmipmaps = B_LITTLE32(nmipmaps);

	tdef = PTCacheAllocNewTile(nmipmaps);
	tdef->tsizx = tsizx;
	tdef->tsizy = tsizy;
	tdef->flags = flags;
	tdef->format = format;

	for (i = 0; i < nmipmaps; i++) {
		if (std::fread(&sizx, 4, 1, fh) != 1 ||
		    std::fread(&sizy, 4, 1, fh) != 1 ||
		    std::fread(&length, 4, 1, fh) != 1) {
			// truncated entry, so throw the whole cache away
			goto fail;
		}

		sizx = B_LITTLE32(sizx);
		sizy = B_LITTLE32(sizy);
		length = B_LITTLE32(length);

		tdef->mipmap[i].sizx = sizx;
		tdef->mipmap[i].sizy = sizy;
		tdef->mipmap[i].length = length;
		tdef->mipmap[i].data = (unsigned char *) std::malloc(length);

		if (std::fread(tdef->mipmap[i].data, length, 1, fh) != 1) {
			// truncated data
			goto fail;
		}
	}

	std::fclose(fh);

	return tdef;

fail:
	cachereplace = true;
	buildprintf("PolymostTexCache: corrupt texture cache detected, cache will be replaced\n");
	PTCacheUnloadIndex();
	std::fclose(fh);

	if(tdef) {
		PTCacheFreeTile(tdef.get());
	}

	return {};
}

/**
 * Loads a tile from the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return a PTCacheTile entry fully completed
 */
std::unique_ptr<PTCacheTile> PTCacheLoadTile(const std::string& filename, int effects, int flags)
{
	if (cachedisabled) {
		return nullptr;
	}

	const PTCacheIndex* pci = ptcache_findhash(filename, effects, flags);

	if (!pci) {
		return nullptr;
	}

	std::unique_ptr<PTCacheTile> tdef = ptcache_load(pci->offset);

	if (tdef) {
		tdef->filename = filename;
		tdef->effects  = effects;
	}

	return tdef;
}

/**
 * Checks to see if a tile exists in the cache.
 * @param filename the filename
 * @param effects the effects bits
 * @param flags the flags bits
 * @return !nullptr if it exists
 */
bool PTCacheHasTile(const std::string& filename, int effects, int flags)
{
	if (cachedisabled) {
		return false;
	}

	return ptcache_findhash(filename, effects, flags) != nullptr;
}

/**
 * Disposes of the resources allocated for a PTCacheTile
 * @param tdef a PTCacheTile entry
 */
void PTCacheFreeTile(PTCacheTile * tdef)
{
	for (int i{0}; i < tdef->nummipmaps; ++i) {
		if (tdef->mipmap[i].data) {
			std::free(tdef->mipmap[i].data);
		}
	}
}

/**
 * Allocates the skeleton of a PTCacheTile entry for you to complete.
 * Memory for filenames and mipmap data should be allocated using malloc()
 * @param nummipmaps allocate mipmap entries for nummipmaps items
 * @return a PTCacheTile entry
 */
std::unique_ptr<PTCacheTile> PTCacheAllocNewTile(int nummipmaps)
{
	auto tdef = std::make_unique<PTCacheTile>();

	tdef->nummipmaps = nummipmaps;

	return tdef;
}

/**
 * Stores a PTCacheTile into the cache.
 * @param tdef a PTCacheTile entry fully completed
 * @return !0 on success
 */
int PTCacheWriteTile(const PTCacheTile * tdef)
{
	char createmode[] = "ab";
	PTCacheIndex* pci{nullptr};

	if (cachedisabled) {
		return 0;
	}

	if (cachereplace) {
		createmode[0] = 'w';
		cachereplace = false;
	}

	// 1. write the tile data to the storage file
	std::FILE* fh = std::fopen(CACHESTORAGEFILE, createmode);

	if (!fh) {
		cachedisabled = true;
		buildprintf("PolymostTexCache: error opening {}, texture cache disabled\n", CACHESTORAGEFILE);
		return 0;
	}

	// apparently opening in append doesn't actually put the
	// file pointer at the end of the file like you would
	// imagine, so the ftell doesn't return the length of the
	// file like you'd expect it should
	std::fseek(fh, 0, SEEK_END);
	off_t offset = ftell(fh);

	if (offset >= UINT32_MAX)
		goto fail;

	if (offset == 0) {
		// new file
		if (std::fwrite(&storagesig[0], 16, 1, fh) != 1) {
			goto fail;
		}

		offset = 16;
	}

	{
		const int32_t tsizx = B_LITTLE32(tdef->tsizx);
		const int32_t tsizy = B_LITTLE32(tdef->tsizy);
		int32_t flags = tdef->flags & (PTH_CLAMPED | PTH_HASALPHA);
		flags = B_LITTLE32(flags);
		const int32_t format = B_LITTLE32(tdef->format);
		const int32_t nmipmaps = B_LITTLE32(tdef->nummipmaps);

		if (std::fwrite(&tsizx, 4, 1, fh) != 1 ||
		    std::fwrite(&tsizy, 4, 1, fh) != 1 ||
		    std::fwrite(&flags, 4, 1, fh) != 1 ||
		    std::fwrite(&format, 4, 1, fh) != 1 ||
		    std::fwrite(&nmipmaps, 4, 1, fh) != 1) {
			goto fail;
		}
	}

	for (int i{0}; i < tdef->nummipmaps; i++) {
		const int32_t sizx = B_LITTLE32(tdef->mipmap[i].sizx);
		const int32_t sizy = B_LITTLE32(tdef->mipmap[i].sizy);
		const int32_t length = B_LITTLE32(tdef->mipmap[i].length);

		if (std::fwrite(&sizx, 4, 1, fh) != 1 ||
		    std::fwrite(&sizy, 4, 1, fh) != 1 ||
		    std::fwrite(&length, 4, 1, fh) != 1) {
			goto fail;
		}

		if (std::fwrite(tdef->mipmap[i].data, tdef->mipmap[i].length, 1, fh) != 1) {
			// truncated data
			goto fail;
		}
	}

	std::fclose(fh);

	// 2. append to the index
	fh = std::fopen(CACHEINDEXFILE, createmode);

	if (!fh) {
		cachedisabled = true;
		buildprintf("PolymostTexCache: error opening {}, texture cache disabled\n", CACHEINDEXFILE);
		return 0;
	}

	std::fseek(fh, 0, SEEK_END);

	if (std::ftell(fh) == 0) {
		// new file
		if (std::fwrite(&indexsig[0], 16, 1, fh) != 1) {
			goto fail;
		}
	}
	
	{
		char filename[PTCACHEINDEXFILENAMELEN];
		int32_t effects;
		int32_t flags;
		int32_t mtime;
		uint32_t offs;

		tdef->filename.copy(filename, sizeof(filename));

		filename[sizeof(filename)-1] = 0;
		effects = B_LITTLE32(tdef->effects);
		flags   = tdef->flags & (PTH_CLAMPED);	// we don't want the informational flags in the index
		flags   = B_LITTLE32(flags);
		offs    = B_LITTLE32((uint32_t)offset);
		mtime = 0;

		if (std::fwrite(filename, sizeof(filename), 1, fh) != 1 ||
		    std::fwrite(&effects, 4, 1, fh) != 1 ||
		    std::fwrite(&flags, 4, 1, fh) != 1 ||
		    std::fwrite(&offs, 4, 1, fh) != 1 ||
		    std::fwrite(&mtime, 4, 1, fh) != 1) {
			goto fail;
		}
	}

	std::fclose(fh);

	// stow the data into the index in memory
	pci = ptcache_findhash(tdef->filename, tdef->effects, tdef->flags);
	
	if (pci) {
		// superseding an old hash entry
		pci->offset = (unsigned)offset;
	}
	else {
		ptcache_addhash(tdef->filename, tdef->effects, tdef->flags, (unsigned)offset);
	}

	return 1;
fail:
	cachedisabled = true;
	buildprintf("PolymostTexCache: error writing to cache, texture cache disabled\n");
	
	if (fh)
		std::fclose(fh);

	return 0;
}

/**
* Forces the cache to be rebuilt.
 */
void PTCacheForceRebuild()
{
	PTCacheUnloadIndex();
	cachedisabled = false;
	cachereplace = true;
}

#endif //USE_OPENGL
