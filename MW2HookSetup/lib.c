/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#include "mw2hooksetup.h"

char *lasterrormessage(void) {
  static char buf[1024];
  
  if(!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, sizeof(buf), NULL))
    return "Error fetching error message.\r\n";
    
  return buf;
}

int directoryexists(char *dir) {
  DWORD r = GetFileAttributes(dir);
  if(r == INVALID_FILE_ATTRIBUTES)
    return 0;
  return r & FILE_ATTRIBUTE_DIRECTORY;
}

int fileexists(char *dir) {
  DWORD r = GetFileAttributes(dir);
  if(r == INVALID_FILE_ATTRIBUTES || r & FILE_ATTRIBUTE_DIRECTORY)
    return 0;
  return 1;
}
