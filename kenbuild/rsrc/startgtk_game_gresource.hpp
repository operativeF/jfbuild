#ifndef __RESOURCE_startgtk_H__
#define __RESOURCE_startgtk_H__

#include <gio/gio.h>

extern GResource *startgtk_get_resource ();

extern void startgtk_register_resource ();
extern void startgtk_unregister_resource ();

#endif
