// mmulti.h

#ifndef __mmulti_h__
#define __mmulti_h__

#ifdef __cplusplus
extern "C" {
#endif

inline constexpr auto MAXMULTIPLAYERS{16};

#define MMULTI_MODE_MS  0
#define MMULTI_MODE_P2P 1

extern int myconnectindex, numplayers, networkmode;
extern int connecthead, connectpoint2[MAXMULTIPLAYERS];
extern unsigned char syncstate;

void initsingleplayers();
void initmultiplayers(int argc, char const * const argv[]);
int initmultiplayersparms(int argc, char const * const argv[]);
int initmultiplayerscycle();

void setpackettimeout(int datimeoutcount, int daresendagaincount);
void uninitmultiplayers();
void sendlogon();
void sendlogoff();
int getoutputcirclesize();
void setsocket(int newsocket);
void sendpacket(int other, const unsigned char *bufptr, int messleng);
int getpacket(int *other, unsigned char *bufptr);
void flushpackets();
void genericmultifunction(int other, const unsigned char *bufptr, int messleng, int command);

#ifdef __cplusplus
}
#endif

#endif	// __mmulti_h__

