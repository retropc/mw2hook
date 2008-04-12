/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#include "mw2hooksetup.h"

int uninstall(char *path) {
  if(!directoryexists(path)) {
    FATALU("Directory does not exist!");
    return 1;
  }
  if(MessageBox(0, "Uninstall MW2Hook?", MSGBOX_TITLE_UNINSTALL, MB_ICONQUESTION | MB_YESNO) == IDNO) {
    FATALU("Uninstallation aborted.");
    return 2;
  }
  
  FATALU("Not implemented yet, sorry!");
  
  return 0;
}
