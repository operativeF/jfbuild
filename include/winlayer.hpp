// Windows DIB/DirectDraw interface layer
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#ifndef __build_interface_layer__
#define __build_interface_layer__ WIN

#ifdef __cplusplus
extern "C" {
#endif

intptr_t win_gethwnd();
intptr_t win_gethinstance();

void win_allowtaskswitching(int onf);
void win_allowbackgroundidle(int onf);
int win_checkinstance();

void win_setmaxrefreshfreq(unsigned frequency);
unsigned win_getmaxrefreshfreq();

#ifdef __cplusplus
}
#endif

#include "baselayer.hpp"

#else
#if (__build_interface_layer__ != WIN)
#error "Already using the " __build_interface_layer__ ". Can't now use Windows."
#endif
#endif // __build_interface_layer__

