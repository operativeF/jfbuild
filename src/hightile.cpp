/*
 * High-colour textures support for Polymost
 * by Jonathon Fowler
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "build.hpp"

#if USE_POLYMOST && USE_OPENGL

#include "kplib.hpp"
#include "hightile_priv.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

std::array<palette_t, MAXPALOOKUPS> hictinting;
std::array<hicreplctyp*, MAXTILES> hicreplc;

int hicfirstinit{0};

/**
 * Find a substitute definition which satisfies the given parameters.
 * This will try for a perfect match with the requested palette, or if
 * none is found, try and find a palette 0 equivalent.
 *
 * @param picnum tile number
 * @param palnum palette index
 * @param skybox 'true' to find a substitute that defines a skybox
 * @return the substitute header, or null
 */
hicreplctyp* hicfindsubst(int picnum, int palnum, int skybox)
{
	hicreplctyp *hr;

	if (!hicfirstinit)
		return nullptr;

	if ((unsigned int)picnum >= (unsigned int)MAXTILES)
		return nullptr;

	do {
		for (hr = hicreplc[picnum]; hr; hr = hr->next) {
			if (hr->palnum == palnum) {
				if (skybox) {
					if (hr->skybox && !hr->skybox->ignore)
						return hr;
				} else {
					if (!hr->ignore)
						return hr;
				}
			}
		}

		if (palnum == 0) break;
		palnum = 0;
	} while (1);

	return nullptr;	// no replacement found
}


/**
 * Initialise the high-colour stuff to a default state
 */
void hicinit()
{
	int i;
	int j;
	hicreplctyp *hr;
	hicreplctyp *next;

	for (i=0;i<MAXPALOOKUPS;i++) {	// all tints should be 100%
		hictinting[i].r = hictinting[i].g = hictinting[i].b = 0xff;
		hictinting[i].f = 0;
	}

	if (hicfirstinit) {
		for (i=MAXTILES-1;i>=0;i--) {
			for (hr=hicreplc[i]; hr; ) {
				next = hr->next;

				if (hr->skybox) {
					for (j=5;j>=0;j--) {
						if (hr->skybox->face[j]) {
							std::free(hr->skybox->face[j]);
						}
					}
					
					std::free(hr->skybox);
				}

				if (hr->filename)
					std::free(hr->filename);

				std::free(hr);

				hr = next;
			}
		}
	}

	std::ranges::fill(hicreplc, nullptr);

	hicfirstinit = 1;
}


//
// hicsetpalettetint(pal,r,g,b,effect)
//   The tinting values represent a mechanism for emulating the effect of global sector
//   palette shifts on true-colour textures and only true-colour textures.
//   effect bitset: 1 = greyscale, 2 = invert
//
void hicsetpalettetint(int palnum, unsigned char r, unsigned char g, unsigned char b, unsigned char effect)
{
	if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS)
		return;

	if (!hicfirstinit)
		hicinit();

	hictinting[palnum].r = r;
	hictinting[palnum].g = g;
	hictinting[palnum].b = b;
	hictinting[palnum].f = effect & HICEFFECTMASK;
}


//
// hicsetsubsttex(picnum,pal,filen,alphacut)
//   Specifies a replacement graphic file for an ART tile.
//
int hicsetsubsttex(int picnum, int palnum, const char *filen, float alphacut, unsigned char flags)
{
	hicreplctyp* hr;
	hicreplctyp* hrn;

	if ((unsigned int)picnum >= (unsigned int)MAXTILES)
		return -1;

	if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS)
		return -1;

	if (!hicfirstinit)
		hicinit();

	for (hr = hicreplc[picnum]; hr; hr = hr->next) {
		if (hr->palnum == palnum)
			break;
	}

	if (!hr) {
		// no replacement yet defined
		hrn = (hicreplctyp *)std::calloc(1,sizeof(hicreplctyp));
		if (!hrn) return -1;
		hrn->palnum = palnum;
	}
	else
		hrn = hr;

	// store into hicreplc the details for this replacement
	if (hrn->filename)
		std::free(hrn->filename);

	hrn->filename = strdup(filen);

	if (!hrn->filename) {
		if (hrn->skybox)
			return -1;	// don't free the base structure if there's a skybox defined
		
		if (hr == nullptr)
			std::free(hrn);	// not yet a link in the chain
		return -1;
	}

	hrn->ignore = 0;
	hrn->alphacut = std::min(alphacut, 1.0F);
	hrn->flags = flags;

	if (hr == nullptr) {
		hrn->next = hicreplc[picnum];
		hicreplc[picnum] = hrn;
	}

	//std::printf("Replacement [%d,%d]: %s\n", picnum, palnum, hicreplc[i]->filename);

	return 0;
}


//
// hicsetskybox(picnum,pal,faces[6])
//   Specifies a graphic files making up a skybox.
//
int hicsetskybox(int picnum, int palnum, const char* const faces[6])
{
	hicreplctyp* hr;
	hicreplctyp* hrn;
	int j;

	if ((unsigned int)picnum >= (unsigned int)MAXTILES)
		return -1;

	if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS)
		return -1;

	for (j=5;j>=0;j--)
		if (!faces[j])
			return -1;

	if (!hicfirstinit)
		hicinit();

	for (hr = hicreplc[picnum]; hr; hr = hr->next) {
		if (hr->palnum == palnum)
			break;
	}

	if (!hr) {
		// no replacement yet defined
		hrn = (hicreplctyp *)std::calloc(1,sizeof(hicreplctyp));
		if (!hrn) return -1;

		hrn->palnum = palnum;
	}
	else
		hrn = hr;

	if (!hrn->skybox) {
		hrn->skybox = (struct hicskybox_t *)std::calloc(1,sizeof(struct hicskybox_t));
		if (!hrn->skybox) {
			if (hr == nullptr)
				std::free(hrn);	// not yet a link in the chain

			return -1;
		}
	} else {
		for (j=5;j>=0;j--) {
			if (hrn->skybox->face[j])
				std::free(hrn->skybox->face[j]);
		}
	}

	// store each face's filename
	for (j=0;j<6;j++) {
		hrn->skybox->face[j] = strdup(faces[j]);
		if (!hrn->skybox->face[j]) {
			for (--j; j>=0; --j)	// free any previous faces
				std::free(hrn->skybox->face[j]);
			
			std::free(hrn->skybox);
			hrn->skybox = nullptr;
			
			if (hr == nullptr)
				std::free(hrn);

			return -1;
		}
	}
	hrn->skybox->ignore = 0;
	if (hr == nullptr) {
		hrn->next = hicreplc[picnum];
		hicreplc[picnum] = hrn;
	}

	return 0;
}


//
// hicclearsubst(picnum,pal)
//   Clears a replacement for an ART tile, including skybox faces.
//
int hicclearsubst(int picnum, int palnum)
{
	hicreplctyp *hr;
	hicreplctyp* hrn{nullptr};

	if ((unsigned int)picnum >= (unsigned int)MAXTILES)
		return -1;
	
	if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS)
		return -1;

	if (!hicfirstinit)
		return 0;

	for (hr = hicreplc[picnum]; hr; hrn = hr, hr = hr->next) {
		if (hr->palnum == palnum)
			break;
	}

	if (!hr)
		return 0;

	if (hr->filename)
		std::free(hr->filename);

	if (hr->skybox) {
		for (int i{5}; i >= 0; --i)
			if (hr->skybox->face[i])
				std::free(hr->skybox->face[i]);

		std::free(hr->skybox);
	}

	if (hrn)
		hrn->next = hr->next;
	else
		hicreplc[picnum] = hr->next;

	std::free(hr);

	return 0;
}

#endif //USE_POLYMOST && USE_OPENGL
