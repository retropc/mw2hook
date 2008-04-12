/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#ifndef __MW2HOOKSETUP_H
#define __MW2HOOKSETUP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MSGBOX_TITLE "MW2HookSetup"
#define MSGBOX_TITLE_UNINSTALL "MW2HookUninstall"

#define EXENAME "mw2inject.exe"
#define SHIMDLL "mw2shim.dll"
#define FATAL(x) MessageBox(0, x, MSGBOX_TITLE, MB_ICONERROR)
#define WARNING(x) MessageBox(0, x, MSGBOX_TITLE, MB_ICONWARNING)
#define FATALU(x) MessageBox(0, x, MSGBOX_TITLE_UNINSTALL, MB_ICONERROR)
#define WARNINGU(x) MessageBox(0, x, MSGBOX_TITLE_UNINSTALL, MB_ICONWARNING)

#ifndef INSTALLFILE
#define INSTALLFILE(x) x "\0"
#endif

static char *files = { 
  INSTALLFILE(EXENAME)
  INSTALLFILE(SHIMDLL)
  INSTALLFILE("LICENSE.TXT")
  INSTALLFILE("")
};

char *lasterrormessage(void);
int directoryexists(char *dir);
int fileexists(char *dir);

#endif