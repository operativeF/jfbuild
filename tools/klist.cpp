// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "compat.hpp"
#include "crc32.hpp"

constexpr auto MAXFILES{4096};

static char buf[65536];

static int numfiles, anyfiles4listing;
static char marked4listing[MAXFILES];
static char filelist[MAXFILES][16];
static int fileoffs[MAXFILES+1], fileleng[MAXFILES];

void findfiles(const char *dafilespec)
{
    char t[13];
    int i;

    for(i=numfiles-1;i>=0;i--)
    {
        std::memcpy(t,filelist[i],12);
        t[12] = 0;

        if (Bwildmatch(t,dafilespec)) {
            marked4listing[i] = 1;
            anyfiles4listing = 1;
        }
    }
}

int main(int argc, char **argv)
{
    int i, j, k, l, fil, fil2;
    ssize_t r;
    unsigned int crc;

    if (argc < 2)
    {
        std::printf("KLIST [grouped file] [@file or filespec...]\n");
        std::printf("   This program lists files in a group file.\n");
        std::printf("   Filter files using ? and * wildcards.\n");
        std::printf("   Ex: klist stuff.dat *.art\n");
        return(0);
    }

    if ((fil = Bopen(argv[1],BO_BINARY|BO_RDONLY,BS_IREAD)) == -1)
    {
        std::printf("Error: %s could not be opened\n",argv[1]);
        return(0);
    }

    r = Bread(fil,buf,16);
    if (r != 16 || memcmp(buf, "KenSilverman", 12))
    {
        Bclose(fil);
        std::printf("Error: %s not a valid group file\n",argv[1]);
        return(0);
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

    for(i=0;i<numfiles;i++) marked4listing[i] = 0;

    anyfiles4listing = 0;
    for(i=argc-1;i>1;i--)
    {
        if (argv[i][0] == '@')
        {
            if ((fil2 = Bopen(&argv[i][1],BO_BINARY|BO_RDONLY,BS_IREAD)) != -1)
            {
                l = Bread(fil2,buf,65536);
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

    if (anyfiles4listing == 0 && argc > 2)
    {
        Bclose(fil);
        std::printf("No files found in group file with those names\n");
        return(0);
    }

    for(i=0;i<numfiles;i++)
    {
        if (marked4listing[i] == 0 && argc > 2) continue;

        fileleng[i] = fileoffs[i+1]-fileoffs[i];

        crc32init(&crc);
        Blseek(fil,fileoffs[i]+((numfiles+1)<<4),SEEK_SET);
        for(j=0;j<fileleng[i];j+=65536)
        {
            k = std::min(fileleng[i] - j, 65536);
            if (Bread(fil,buf,k) < k)
            {
                std::printf("Read error\n");
                Bclose(fil);
                return(1);
            }
            crc32block(&crc, (unsigned char *)buf, k);
        }
        crc32finish(&crc);

        std::printf("%-12s  %08X  %d\n",filelist[i], crc, fileleng[i]);
    }
    Bclose(fil);

    return 0;
}

