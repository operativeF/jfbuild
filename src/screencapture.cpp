
#include "baselayer.hpp"
#include "build.hpp"
#include "crc32.hpp"
#include "engine_priv.hpp"
#include "polymost_priv.hpp"
#include "pragmas.hpp"
#include "screencapture.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

static char capturename[20] = "capt0000.xxx";
static short capturecount{0};

std::FILE *screencapture_openfile(const char *ext)
{
	char *seq;
	std::FILE *fil;

	do {	// JBF 2004022: So we don't overwrite existing screenshots
		if (capturecount > 9999) {
			return nullptr;
		}

		seq = strrchr(capturename, '.'); if (!seq) return nullptr;
		seq -= 4;
		seq[0] = ((capturecount/1000)%10)+48;
		seq[1] = ((capturecount/100)%10)+48;
		seq[2] = ((capturecount/10)%10)+48;
		seq[3] = (capturecount%10)+48;
		seq[5] = ext[0];
		seq[6] = ext[1];
		seq[7] = ext[2];

		if ((fil = std::fopen(capturename, "rb")) == nullptr) break;
		std::fclose(fil);
		capturecount++;
	} while (1);
	fil = std::fopen(capturename, "wb");
	if (fil) capturecount++;
	return fil;
}

int screencapture_writeframe(std::FILE *fil, char mode, void *v,
	void (*writeline)(unsigned char *, int, int, std::FILE *, void *))
{
	int y;
	int ystart;
	int yend;
	int yinc;
	int j;
	unsigned char *ptr;
	unsigned char *buf;
	char inverseit = 0;
	char bottotop = 0;

	inverseit = (mode & 1);
	bottotop = (mode & 2);

#if USE_POLYMOST && USE_OPENGL
	if (rendmode >= 3 && qsetmode == 200) {
		const char bgr = (mode & 4);

		// OpenGL returns bottom-to-top ordered lines.
		if (bottotop) {
			ystart = 0;
			yend = ydim;
			yinc = 1;
		} else {
			ystart = ydim-1;
			yend = -1;
			yinc = -1;
		}
		std::vector<unsigned char> buf;
		buf.resize(xdim * ydim * 3);
		glfunc.glReadPixels(0, 0, xdim, ydim, GL_RGB,GL_UNSIGNED_BYTE, &buf[0]);
		if (bgr) {
			for (j=(xdim * ydim-1) * 3; j >= 0; j -= 3) {
				std::swap(buf[j + 0], buf[j + 2]);
			}
		}
		for (y = ystart; y != yend; y += yinc) {
			ptr = &buf[0] + y * xdim * 3;
			writeline(ptr, xdim, 3, fil, v);
		}

		return 0;
	}
#endif

	ptr = (unsigned char *)frameplace;
	if (bottotop) {
		ystart = ydim-1;
		yend = -1;
		yinc = -1;
	} else {
		ystart = 0;
		yend = ydim;
		yinc = 1;
	}

	if (inverseit && qsetmode != 200) {
		std::vector<unsigned char> buf;
		buf.resize(bytesperline);
		for (y = ystart; y != yend; y += yinc) {
			copybuf(ptr + y * bytesperline, &buf[0], xdim >> 2);
			for (j=(bytesperline>>2)-1; j>=0; j--)
				((int *)&buf[0])[j] ^= 0x0f0f0f0fL;
			writeline(&buf[0], xdim, 1, fil, v);
		}
	}
	else {
		for (y = ystart; y != yend; y += yinc) {
			writeline(ptr + y*bytesperline, xdim, 1, fil, v);
		}
	}

	return(0);
}

void screencapture_writetgaline(unsigned char *buf, int bytes, int elements, std::FILE *fp, void *v)
{
	std::ignore = v;
	std::fwrite(buf, bytes, elements, fp);
}

int screencapture_tga(char mode)
{
	unsigned char head[18] = { 0,1,1,0,0,0,1,24,0,0,0,0,0/*wlo*/,0/*whi*/,0/*hlo*/,0/*hhi*/,8,0 };
	std::FILE* fil;

	if ((fil = screencapture_openfile("tga")) == nullptr) {
		return -1;
	}

#if USE_POLYMOST && USE_OPENGL
	if (rendmode >= 3 && qsetmode == 200) {
		head[1] = 0;	// no colourmap
		head[2] = 2;	// uncompressed truecolour
		head[3] = 0;	// (low) first colourmap index
		head[4] = 0;	// (high) first colourmap index
		head[5] = 0;	// (low) number colourmap entries
		head[6] = 0;	// (high) number colourmap entries
		head[7] = 0;	// colourmap entry size
		head[16] = 24;	// 24 bits per pixel
	}
#endif

	head[12] = xdim & 0xff;
	head[13] = (xdim >> 8) & 0xff;
	head[14] = ydim & 0xff;
	head[15] = (ydim >> 8) & 0xff;

	std::fwrite(head, 18, 1, fil);

	// palette first
#if USE_POLYMOST && USE_OPENGL
	if (rendmode < 3 || (rendmode == 3 && qsetmode != 200)) {
#endif
		for (const auto& fadedCol : curpalettefaded) {
			std::fputc(fadedCol.b, fil);
			std::fputc(fadedCol.g, fil);
			std::fputc(fadedCol.r, fil);
		}

#if USE_POLYMOST && USE_OPENGL
	}
#endif

	// Targa renders bottom to top, from left to right.
	// 24bit images use BGR element order.
	screencapture_writeframe(fil, (mode & 1) | 2 | 4, nullptr, screencapture_writetgaline);

	std::fclose(fil);

	return 0;
}

// PCX is nasty, which is why I've lifted these functions from the PCX spec by ZSoft
int writepcxbyte(unsigned char colour, unsigned char count, std::FILE* fp)
{
	if (!count)
		return 0;

	if (count == 1 && (colour & 0xc0) != 0xc0) {
		std::fputc(colour, fp);

		return 1;
	}
	else {
		std::fputc(0xc0 | count, fp);
		std::fputc(colour, fp);

		return 2;
	}
}

void writepcxline(unsigned char *buf, int bytes, int step, std::FILE* fp)
{
    unsigned char last = *buf;
	unsigned char runCount{1};


	for (int srcIndex{1}; srcIndex < bytes; ++srcIndex) {
		buf += step;
		const unsigned char ths = *buf;

		if (ths == last) {
			runCount++;

			if (runCount == 63) {
				writepcxbyte(last, runCount, fp);
				runCount = 0;
			}
		}
		else {
			if (runCount)
				writepcxbyte(last, runCount, fp);

			last = ths;
			runCount = 1;
		}
	}

	if (runCount != 0)
		writepcxbyte(last, runCount, fp);

	if (bytes & 1)
		writepcxbyte(0, 1, fp);
}

void screencapture_writepcxline(unsigned char *buf, int bytes, int elements, std::FILE *fp, void *v)
{
	std::ignore = v;

	if (elements == 3) {
		writepcxline(buf,     bytes, 3, fp);
		writepcxline(buf + 1, bytes, 3, fp);
		writepcxline(buf + 2, bytes, 3, fp);
		return;
	}

	writepcxline(buf, bytes, 1, fp);
}

int screencapture_pcx(char mode)
{
	unsigned char head[128];
	std::FILE* fil;

	if ((fil = screencapture_openfile("pcx")) == nullptr) {
		return -1;
	}

	std::memset(head, 0, 128);

	head[0] = 10;
	head[1] = 5;
	head[2] = 1;
	head[3] = 8;
	head[12] = 72;
	head[13] = 0;
	head[14] = 72;
	head[15] = 0;
	head[65] = 1;	// 8-bit
	head[68] = 1;

#if USE_POLYMOST && USE_OPENGL
	if (rendmode >= 3 && qsetmode == 200) {
		head[65] = 3;	// 24-bit
	}
#endif

	head[8] = (xdim-1) & 0xff;
	head[9] = ((xdim-1) >> 8) & 0xff;
	head[10] = (ydim-1) & 0xff;
	head[11] = ((ydim-1) >> 8) & 0xff;

	const int bpl = xdim + (xdim & 1);

	head[66] = bpl & 0xff;
	head[67] = (bpl >> 8) & 0xff;

	std::fwrite(head, 128, 1, fil);

	// PCX renders top to bottom, from left to right.
	// 24-bit images have each scan line written as deinterleaved RGB.
	screencapture_writeframe(fil, (mode & 1), nullptr, screencapture_writepcxline);

	// palette last
#if USE_POLYMOST && USE_OPENGL
	if (rendmode < 3 || (rendmode == 3 && qsetmode != 200)) {
#endif
		std::fputc(12, fil);

		for (const auto& fadedColor : curpalettefaded) {
			std::fputc(fadedColor.r, fil);	// b
			std::fputc(fadedColor.g, fil);	// g
			std::fputc(fadedColor.b, fil);	// r
		}
#if USE_POLYMOST && USE_OPENGL
	}
#endif

	std::fclose(fil);
	return 0;
}

void screencapture_writepngline(unsigned char *buf, int bytes, int elements, std::FILE* fp, void *v)
{
	unsigned char header[6];
	auto* sums = static_cast<struct pngsums *>(v);

	unsigned short blklen = static_cast<unsigned short>(B_LITTLE16(1 + bytes * elements));	// One extra for the filter type.
	header[0] = 0;	// BFINAL = 0, BTYPE = 00.
	std::memcpy(&header[1], &blklen, 2);
	blklen = ~blklen;
	std::memcpy(&header[3], &blklen, 2);

	header[5] = 0;	// No filter.
	sums->adlers2 = (sums->adlers2 + sums->adlers1) % 65521;
	crc32block(&sums->crc, header, sizeof(header));
	std::fwrite(header, sizeof(header), 1, fp);

	for (int i{0}; i < bytes * elements; ++i) {
		sums->adlers1 = (sums->adlers1 + buf[i]) % 65521;
		sums->adlers2 = (sums->adlers2 + sums->adlers1) % 65521;
	}

	crc32block(&sums->crc, buf, bytes * elements);
	
	std::fwrite(buf, bytes, elements, fp);
}

int screencapture_png(char mode)
{
	const unsigned char pngsig[] = { 0x89, 'P', 'N', 'G', 0xd, 0xa, 0x1a, 0xa };
	const unsigned char enddeflate[] = { 1, 0, 0, 0xff, 0xff };

#define BEGIN_PNG_CHUNK(type) { \
	acclen = 4; \
	std::memcpy(&buf[acclen], type, 4); \
	acclen += 4; \
}
#define SET_PNG_CHUNK_LEN(fore) { \
	/* Accumulated and forecast, minus length and type fields. */ \
	const int len = B_BIG32(acclen + fore - 8); \
	std::memcpy(&buf[0], &len, 4); \
}
#define END_PNG_CHUNK(ccrc) { \
	const unsigned int crc = B_BIG32(ccrc); \
	std::memcpy(&buf[acclen], &crc, 4); \
	acclen += 4; \
	std::fwrite(buf, acclen, 1, fil); \
}

	unsigned char buf[1024];
	int length;
	int i;
	int acclen;
	int glmode{0};
	unsigned short s;
	std::FILE* fil;
	struct pngsums sums;

#if USE_POLYMOST && USE_OPENGL
	glmode = (rendmode == 3 && qsetmode == 200);
#endif

	if ((fil = screencapture_openfile("png")) == nullptr) {
		return -1;
	}

	std::fwrite(pngsig, sizeof(pngsig), 1, fil);

	// Header.
	BEGIN_PNG_CHUNK("IHDR");
	i = B_BIG32(xdim); std::memcpy(&buf[acclen], &i, 4); acclen += 4;
	i = B_BIG32(ydim); std::memcpy(&buf[acclen], &i, 4); acclen += 4;
	buf[acclen++] = 8;	// Bit depth per sample/palette index.
	buf[acclen++] = glmode ? 2 : 3;	// Colour type.
	buf[acclen++] = 0;	// Deflate compression method.
	buf[acclen++] = 0;	// Adaptive filter.
	buf[acclen++] = 0;	// No interlace.
	SET_PNG_CHUNK_LEN(0);
	END_PNG_CHUNK(crc32once(&buf[4], acclen - 4));

	// Palette if needed.
#if USE_POLYMOST && USE_OPENGL
	if (rendmode < 3 || (rendmode == 3 && qsetmode != 200)) {
#endif
		BEGIN_PNG_CHUNK("PLTE");
		for (i=0; i<256; i++, acclen+=3) {
			buf[acclen+0] = curpalettefaded[i].r;
			buf[acclen+1] = curpalettefaded[i].g;
			buf[acclen+2] = curpalettefaded[i].b;
		}
		SET_PNG_CHUNK_LEN(0);
		END_PNG_CHUNK(crc32once(&buf[4], acclen - 4));
#if USE_POLYMOST && USE_OPENGL
	}
#endif

	// Image Data.
	BEGIN_PNG_CHUNK("IDAT");
	crc32init(&sums.crc);

	// Content is a Zlib stream.
	buf[acclen++] = 0x78;	// Deflate, 32k window.
	buf[acclen++] = 1;		// Check bits 0-4: (0x7800 + 1) % 0x1f == 0

	length = (1 + 2 + 2) + 1 + xdim * (glmode ? 3 : 1);	// Length of one scanline deflate block.
	length *= ydim;			// By height.
	length += sizeof(enddeflate);	// Plus length of 'End of Deflate' block.
	length += 4;			// Plus Adler32 checksum.
	SET_PNG_CHUNK_LEN(length);

	crc32block(&sums.crc, &buf[4], acclen - 4);
	std::fwrite(buf, acclen, 1, fil);	// Write header and start of Zlib stream.

	sums.adlers1 = 1;
	sums.adlers2 = 0;
	screencapture_writeframe(fil, (mode&1), &sums, screencapture_writepngline);

	// Finalise the Zlib stream.
	acclen = 0;
	std::memcpy(&buf[acclen], enddeflate, sizeof(enddeflate)); acclen += sizeof(enddeflate);
	s = B_BIG16(sums.adlers2); std::memcpy(&buf[acclen], &s, 2); acclen += 2;
	s = B_BIG16(sums.adlers1); std::memcpy(&buf[acclen], &s, 2); acclen += 2;

	// Finalise the Image Data chunk and write what remains out.
	crc32block(&sums.crc, buf, acclen);
	crc32finish(&sums.crc);
	END_PNG_CHUNK(sums.crc);

	// End chunk.
	BEGIN_PNG_CHUNK("IEND");
	SET_PNG_CHUNK_LEN(0);
	END_PNG_CHUNK(crc32once(&buf[4], acclen - 4));

	std::fclose(fil);

	return 0;
}

int screencapture(const char* filename, char mode)
{
	int ret;

	if (filename) {
		std::strcpy(capturename, filename);
	}

	if (qsetmode == 200 && (mode & 2) && !captureatnextpage) {
		captureatnextpage = mode;
		return 0;
	
	}
	switch (captureformat) {
		case 0: ret = screencapture_tga(mode&1); break;
		case 1: ret = screencapture_pcx(mode&1); break;
		default: ret = screencapture_png(mode&1); break;
	}

	if (ret == 0) {
		buildprintf("Saved screenshot to {}\n", capturename);
	}

	return ret;
}
