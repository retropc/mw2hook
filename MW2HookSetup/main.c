/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#include "mw2hooksetup.h"
#include <objbase.h>

int setup(void);
int uninstall(char *path);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  int r;
  
  CoInitialize(NULL);
  if(!strcmp(lpCmdLine, "")) {
    r = setup();
  } else {
    r = uninstall(lpCmdLine);
  }
  CoUninitialize();
  
  return r;
}
