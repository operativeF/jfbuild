// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)


#include "mmulti.hpp"

int isvalidipaddress (char *st)
{
	return 0;
}

int initmultiplayersparms(int argc, char const * const argv[])
{
	return 0;
}

int initmultiplayerscycle()
{
	return 0;
}

void initmultiplayers (int argc, char const * const argv[])
{
	numplayers = 1;
	myconnectindex = 0;
	connecthead = 0;
	connectpoint2[0] = -1;
}

void setpackettimeout(int datimeoutcount, int daresendagaincount)
{
}

void uninitmultiplayers()
{
}

void initsingleplayers()
{
	numplayers = 1;
	myconnectindex = 0;
	connecthead = 0;
	connectpoint2[0] = -1;
}

void sendlogon()
{
}

void sendlogoff()
{
}

int getoutputcirclesize()
{
	return 0;
}

void setsocket(int newsocket)
{
}

void sendpacket(int other, unsigned char *bufptr, int messleng)
{
}

int getpacket (int *other, unsigned char *bufptr)
{
	return 0;
}

void flushpackets()
{
}

void genericmultifunction(int other, unsigned char *bufptr, int messleng, int command)
{
}


