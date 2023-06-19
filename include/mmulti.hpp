// mmulti.h

#ifndef __mmulti_h__
#define __mmulti_h__

#include <array>

inline constexpr auto MAXMULTIPLAYERS{16};

#define MMULTI_MODE_MS  0
#define MMULTI_MODE_P2P 1

inline int myconnectindex{0};
inline int numplayers{0};
inline int networkmode{-1};
inline int connecthead{0};
inline std::array<int, MAXMULTIPLAYERS> connectpoint2{};
inline unsigned char syncstate{0};

void initsingleplayers();
void initmultiplayers(int argc, char const * const argv[]);
int initmultiplayersparms(int argc, char const * const argv[]);
int initmultiplayerscycle();

void setpackettimeout(int datimeoutcount, int daresendagaincount);
void uninitmultiplayers();
void setsocket(int newsocket);
void sendpacket(int other, const unsigned char *bufptr, int messleng);
int getpacket(int *other, unsigned char *bufptr);
void flushpackets();
void genericmultifunction(int other, const unsigned char *bufptr, int messleng, int command);

#endif	// __mmulti_h__

