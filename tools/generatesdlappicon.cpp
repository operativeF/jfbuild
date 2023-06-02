#include "compat.hpp"
#include "kplib.hpp"

struct icon {
	int width,height;
	unsigned int *pixels;
	unsigned char *mask;
};

int writeicon(FILE *fp, struct icon *ico)
{
	int i;

	fprintf(fp,
		"// Generated by generatesdlappicon. Do not edit directly.\n\n"
		"#include \"compat.h\"\n"
		"#include \"sdlayer.h\"\n"
		"\n"
	);
	fprintf(fp,"static unsigned int sdlappicon_pixels[] = {\n");
	for (i=0;i<ico->width*ico->height;i++) {
		if ((i%6) == 0) fprintf(fp,"\t");
		else fprintf(fp," ");
		fprintf(fp, "0x%08x,", B_LITTLE32(ico->pixels[i]));
		if ((i%6) == 5) fprintf(fp,"\n");
	}
	if ((i%16) > 0) fprintf(fp, "\n");
	fprintf(fp, "};\n\n");

	fprintf(fp,"static unsigned char sdlappicon_mask[] = {\n");
	for (i=0;i<((ico->width+7)/8)*ico->height;i++) {
		if ((i%14) == 0) fprintf(fp,"\t");
		else fprintf(fp," ");
		fprintf(fp, "%3d,", ico->mask[i]);
		if ((i%14) == 13) fprintf(fp,"\n");
	}
	if ((i%16) > 0) fprintf(fp, "\n");
	fprintf(fp, "};\n\n");

	fprintf(fp,
		"struct sdlappicon sdlappicon = {\n"
		"	%d,%d,	// width,height\n"
		"	sdlappicon_pixels,\n"
		"	sdlappicon_mask\n"
		"};\n",
		ico->width, ico->height
	);

	return 0;
}

int main(int argc, char **argv)
{
	struct icon icon;
	int bpl, xsiz, ysiz;
	int i,j,k,c;
	unsigned char *mask, *maskp, bm, *pp, ch;

	if (argc<2) {
		fprintf(stderr, "generatesdlappicon <picture file>\n");
		return 1;
	}

	std::memset(&icon, 0, sizeof(icon));

	kpzload(argv[1], (intptr_t*)&icon.pixels, &bpl, &icon.width, &icon.height);
	if (!icon.pixels) {
		fprintf(stderr, "Failure loading %s\n", argv[1]);
		return 1;
	}

	if (bpl != icon.width * 4) {
		fprintf(stderr, "bpl != icon.width * 4 (%d != %d * 4)\n", bpl, icon.width);
		free(icon.pixels);
		return 1;
	}

	icon.mask = (unsigned char *)calloc(icon.height, (icon.width+7)/8);
	if (!icon.mask) {
		fprintf(stderr, "Out of memory\n");
		free(icon.pixels);
		return 1;
	}

	maskp = icon.mask;
	bm = 1;
	pp = (unsigned char *)icon.pixels;
	for (i=0; i<icon.height*icon.width; i++) {
		if (bm == 0) {
			bm = 1;
			maskp++;
		}

		{
			ch = pp[0];
			pp[0] = pp[2];
			pp[2] = ch;
		}
		if (pp[3] > 0) *maskp |= bm;

		bm <<= 1;
		pp += 4;
	}

	writeicon(stdout, &icon);

	free(icon.pixels);
	free(icon.mask);

	return 0;
}

