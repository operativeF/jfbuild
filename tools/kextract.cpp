// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "compat.hpp"

#include <algorithm>

constexpr auto MAXFILES{4096};

static char buf[65536];

static int numfiles, anyfiles4extraction;
static char marked4extraction[MAXFILES];
static char filelist[MAXFILES][16];
static int fileoffs[MAXFILES+1], fileleng[MAXFILES];

static void findfiles(const char *dafilespec)
{
	char t[13];
	int i;

	for(i=numfiles-1;i>=0;i--)
	{
		std::memcpy(t,filelist[i],12);
		t[12] = 0;
		
		if (Bwildmatch(t,dafilespec)) {
			marked4extraction[i] = 1;
			anyfiles4extraction = 1;
		}
	}
}

int main(int argc, char **argv)
{
	int i, j, k, l, fil, fil2;
	ssize_t r;

	if (argc < 3)
	{
		std::printf("KEXTRACT [grouped file][@file or filespec...]           by Kenneth Silverman\n");
		std::printf("   This program extracts files from a previously grouped group file.\n");
		std::printf("   You can extract files using the ? and * wildcards.\n");
		std::printf("   Ex: kextract stuff.dat tiles000.art nukeland.map palette.dat\n");
		std::printf("      (stuff.dat is the group file, the rest are the files to extract)\n");
		return(0);
	}

	if ((fil = Bopen(argv[1],BO_BINARY|BO_RDONLY,BS_IREAD)) == -1)
	{
		std::printf("Error: %s could not be opened\n",argv[1]);
		return(1);
	}

	r = Bread(fil,buf,16);
	if (r != 16 || std::memcmp(buf, "KenSilverman", 12))
	{
		Bclose(fil);
		std::printf("Error: %s not a valid group file\n",argv[1]);
		return(1);
	}
	numfiles = *((int*)&buf[12]);

	r = Bread(fil,filelist,numfiles<<4);
	if (r != numfiles<<4)
	{
		Bclose(fil);
		std::printf("Error: %s not a valid group file\n",argv[1]);
		return(1);
	}

	j = 0;
	for(i=0;i<numfiles;i++)
	{
		k = *((int*)&filelist[i][12]);
		filelist[i][12] = 0;
		fileoffs[i] = j;
		j += k;
	}
	fileoffs[numfiles] = j;

	for(i=0;i<numfiles;i++) marked4extraction[i] = 0;

	anyfiles4extraction = 0;
	for(i=argc-1;i>1;i--)
	{
		if (argv[i][0] == '@')
		{
			if ((fil2 = Bopen(&argv[i][1],BO_BINARY|BO_RDONLY,BS_IREAD)) != -1)
			{
				l = (int)Bread(fil2,buf,65536);
				j = 0;
				while ((j < l) && (buf[j] <= 32)) j++;
				while (j < l)
				{
					k = j;
					while ((k < l) && (buf[k] > 32)) k++;

					buf[k] = 0;
					findfiles(&buf[j]);
					j = k+1;

					while ((j < l) && (buf[j] <= 32)) j++;
				}
				Bclose(fil2);
			}
		}
		else
			findfiles(argv[i]);
	}

	if (anyfiles4extraction == 0)
	{
		Bclose(fil);
		std::printf("No files found in group file with those names\n");
		return(1);
	}

	for(i=0;i<numfiles;i++)
	{
		if (marked4extraction[i] == 0) continue;

		fileleng[i] = fileoffs[i+1]-fileoffs[i];

		if ((fil2 = Bopen(filelist[i],BO_BINARY|BO_TRUNC|BO_CREAT|BO_WRONLY,BS_IREAD|BS_IWRITE)) == -1)
		{
			std::printf("Error: Could not write to %s\n",filelist[i]);
			continue;
		}
		std::printf("Extracting %s...\n",filelist[i]);
		Blseek(fil,fileoffs[i]+((numfiles+1)<<4),SEEK_SET);
		for(j=0;j<fileleng[i];j+=65536)
		{
			k = std::min(fileleng[i] - j, 65536);
			if (Bread(fil,buf,k) < k)
			{
				std::printf("Read error\n");
				Bclose(fil2);
				Bclose(fil);
				return(1);
			}
			if (Bwrite(fil2,buf,k) < k)
			{
				std::printf("Write error (drive full?)\n");
				Bclose(fil2);
				Bclose(fil);
				return(1);
			}
		}
		Bclose(fil2);
	}
	Bclose(fil);

	return 0;
}

