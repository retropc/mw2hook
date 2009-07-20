/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <ddraw.h>
#include <mmsystem.h>

#include "mw2shim.h"

static BOOL	(WINAPI *TrueBitBlt)(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop) = BitBlt;
static HANDLE (WINAPI *TrueFindFirstFileA)(LPCSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData) = FindFirstFileA;
static BOOL (WINAPI *TrueFindNextFileA)(HANDLE hFindFile, LPWIN32_FIND_DATA lpFindFileData) = FindNextFileA;
static BOOL (WINAPI *TrueFindClose)(HANDLE hFindFile) = FindClose;
static BOOL (WINAPI *TrueHeapFree)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) = HeapFree;
static HRESULT (WINAPI *TrueDirectDrawCreate)(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter);

static HRESULT (STDMETHODCALLTYPE *TrueCreateSurface)(void *, LPDDSURFACEDESC, LPDIRECTDRAWSURFACE *, IUnknown *);
static HRESULT (STDMETHODCALLTYPE *TrueUnlock)(IDirectDrawSurface *, LPVOID);
static HRESULT (STDMETHODCALLTYPE *TrueLock)(IDirectDrawSurface *, LPRECT, LPDDSURFACEDESC, DWORD, HANDLE);

static BOOL WINAPI FixedBitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop) {
  BOOL b = TrueBitBlt(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);  
  if(!b)
    return 0;

  return cy;
}

static HANDLE mechlab = INVALID_HANDLE_VALUE;

static void mutatefinddata(LPWIN32_FIND_DATA fd) {
  memcpy(fd->cAlternateFileName, fd->cFileName, sizeof(fd->cAlternateFileName));
  fd->cAlternateFileName[sizeof(fd->cAlternateFileName) - 1] = '\0';
}

static HANDLE WINAPI FixedFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData) {
  HANDLE h = TrueFindFirstFileA(lpFileName, lpFindFileData);
  char a, b, c;

  if(h == INVALID_HANDLE_VALUE)
    return h;
  
  if(sscanf_s(lpFileName, "mek\\%c%c%c??usr.mek", &a, &b, &c)) {
    mechlab = h;
    mutatefinddata(lpFindFileData);
  }
    
  return h;
}

static BOOL WINAPI FixedFindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATA lpFindFileData) {
  BOOL b = TrueFindNextFileA(hFindFile, lpFindFileData);
  
  if(b && (mechlab == lpFindFileData))
    mutatefinddata(lpFindFileData);
  
  return b;
}

static BOOL WINAPI FixedFindClose(HANDLE hFindFile) {
  if(mechlab == hFindFile)
    mechlab = INVALID_HANDLE_VALUE;

  return TrueFindClose(hFindFile);
}

static BOOL WINAPI FixedHeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
  /* memory leaks ahoy */
  
  return TRUE;
}

static void LimitRate(int rate) {
  static DWORD nexttick;
  DWORD t = timeGetTime();
  DWORD sleepfor;

  if(nexttick == 0)
    nexttick = t;

  nexttick+=rate;
  
  /*sleepfor = nexttick - t;*/

  /* for some reason sleeping here screws up windows... */

  while(timeGetTime() < nexttick)
    ;
}

static int frameratelimit;
static const char *setupframeratelimit(char *args) {
  frameratelimit = 1000 / atoi(args);
  if(frameratelimit < 1 || frameratelimit > 1000)
    return "Bad frame rate limit supplied.";

  return NULL;
}

static int showfps;
static const char *setupfpscounter(char *args) {
  if(!strcmp(args, "1"))
    showfps = 1;
  
  return NULL;
}

static int getfpsrate() {
  static DWORD lastticks;
  static int fps, frames;
  DWORD ticks = timeGetTime(), delta;

  if(!lastticks) {
    lastticks = ticks;
    return 0;
  }

  frames++;
  delta = ticks - lastticks;
  if(delta > 1000) {
    double denom = ((double)delta / (double)frames);
    if(denom == 0) {
      fps = 0;
    } else {
      fps = (int)(1000.0 / denom);
    }

    frames = 0;
    lastticks = ticks;
  }

  return fps;
}

static LPDDSURFACEDESC mainsurface;
static HRESULT STDMETHODCALLTYPE FixedUnlock(IDirectDrawSurface *p, LPVOID a) {
  static TIMECAPS timecaps;
  static int gottimecaps;

  if(gottimecaps == 0) {
    gottimecaps = 1;
    if(timeGetDevCaps(&timecaps, sizeof(timecaps)) != TIMERR_NOERROR)
      timecaps.wPeriodMin = 10;
  }

  timeBeginPeriod(timecaps.wPeriodMin);
  LimitRate(frameratelimit); 

  if(showfps) {
    int rate = getfpsrate();
    int scale = 4, oncolour = 0, offcolour = 255;
    plotnumbers(rate, mainsurface, 0, 0, scale, oncolour);
    plotnumbers(rate, mainsurface, 2, 2, scale, oncolour);
    plotnumbers(rate, mainsurface, 0, 2, scale, oncolour);
    plotnumbers(rate, mainsurface, 2, 0, scale, oncolour);
    plotnumbers(rate, mainsurface, 1, 1, scale, offcolour);
  }

  timeEndPeriod(timecaps.wPeriodMin);

  return TrueUnlock(p, a);
}

static HRESULT STDMETHODCALLTYPE FixedLock(IDirectDrawSurface *p, LPRECT a, LPDDSURFACEDESC b, DWORD c, HANDLE d) {
  HRESULT ret = TrueLock(p, a, b, c, d);
  if(ret != DD_OK)
    return ret;

  mainsurface = b;
  return ret;
}

static HRESULT STDMETHODCALLTYPE FixedCreateSurface(void *p, LPDDSURFACEDESC a, LPDIRECTDRAWSURFACE *b, IUnknown *c) {
  IDirectDrawSurface *psurf;
  HRESULT ret = TrueCreateSurface(p, a, b, c);
  
  if(ret != DD_OK)
    return ret;

  psurf = *b;
  
  if(psurf->lpVtbl->Unlock != FixedUnlock)
    TrueUnlock = psurf->lpVtbl->Unlock;
  psurf->lpVtbl->Unlock = FixedUnlock;
  
  if(showfps) {
    if(psurf->lpVtbl->Lock != FixedLock)
      TrueLock = psurf->lpVtbl->Lock;
    psurf->lpVtbl->Lock = FixedLock;
  }

  return ret;
}

static HRESULT WINAPI FixedDirectDrawCreate(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter) {
  IDirectDraw *pdd;
  HRESULT ret = TrueDirectDrawCreate(lpGUID, lplpDD, pUnkOuter);
  
  if(ret != DD_OK)
    return ret;
  
  pdd = *lplpDD;
  
  if(pdd->lpVtbl->CreateSurface != FixedCreateSurface)
    TrueCreateSurface = pdd->lpVtbl->CreateSurface;
  pdd->lpVtbl->CreateSurface = FixedCreateSurface;
  
  return ret;
}

static hunk hstartup[] = {
  { HUNK_FUNC, (void *)&FixedBitBlt, (void *)&TrueBitBlt, },
};

static hunk hmechlab[] = {
  { HUNK_FUNC, (void *)&FixedFindFirstFileA, (void *)&TrueFindFirstFileA },
  { HUNK_FUNC, (void *)&FixedFindNextFileA, (void *)&TrueFindNextFileA },
  { HUNK_FUNC, (void *)&FixedFindClose, (void *)&TrueFindClose },
};

static hunk hheaphack[] = {
  { HUNK_FUNC, (void *)&FixedHeapFree, (void *)&TrueHeapFree },
};

static hunk hddraw[] = {
  { HUNK_NAME, (void *)&FixedDirectDrawCreate, NULL, "ddraw.dll", "DirectDrawCreate", &TrueDirectDrawCreate },
};

int patchcount = 5;
patch patches[] = { 
  { "startup", "Fixes startup termination", 1, hstartup },
  { "mechlab", "Fixes Mech Lab overweight issue", 3, hmechlab },
  { "heaphack", "Fixes a lot of random crashes but will increase memory usage.", 1, hheaphack },
  { "frameratelimit", "Fixes jumpjet and missile problems", 1, hddraw, "45", "[frame rate in frames/second]", setupframeratelimit },
  { "fpscounter", "FPS counter", 1, hddraw, "0", "[1 to enable]", setupfpscounter },
};
