/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */
 
#include <windows.h>
#include <stdio.h>
#include <time.h>

#include "mw2shim.h"

#ifndef DEBUG_LOG

void openlog(char *filename) {
}

void closelog(void) {
}

void logentry(char *s, ...) {
}

#else

static CRITICAL_SECTION filelock;
static HANDLE logfile = INVALID_HANDLE_VALUE;

static void vlogentry(char *s, va_list ap) {
  char timebuf[100], buf1[512], buf2[512];
  int bytes;
  DWORD byteswritten;
  
  if(logfile == INVALID_HANDLE_VALUE)
    return;
    
  _strtime_s(timebuf, sizeof(timebuf));

  vsnprintf_s(buf1, sizeof(buf1), _TRUNCATE, s, ap);
  
  bytes = sprintf_s(buf2, sizeof(buf2), "[%s] %s\r\n", timebuf, buf1);
  
  WriteFile(logfile, buf2, bytes, &byteswritten, 0);
}


void __logentry(char *s, ...) {
  va_list ap;
  
  va_start(ap, s);
  vlogentry(s, ap);  
  va_end(ap);  
}

void logentry(char *s, ...) {
  va_list ap;
  
  va_start(ap, s);
  
  EnterCriticalSection(&filelock);
  vlogentry(s, ap);
  LeaveCriticalSection(&filelock);
  
  va_end(ap);  
}

void openlog(char *filename) {
  InitializeCriticalSection(&filelock);
  
  logfile = CreateFile(filename, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  if(logfile == INVALID_HANDLE_VALUE)
    return;
    
  __logentry("Log opened");
}

void closelog(void) {
  /*
    we're assuming we are called last, but other threads might be locked ahead of us...
    we're also assuming the enter calls are called in order... probably not the case!
  */
  
  EnterCriticalSection(&filelock);
  
  if(logfile != INVALID_HANDLE_VALUE) {
    __logentry("Log closed");
    CloseHandle(logfile);
    logfile = INVALID_HANDLE_VALUE;
  
  }
  LeaveCriticalSection(&filelock);
  DeleteCriticalSection(&filelock);
}
#endif
