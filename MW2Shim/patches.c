/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <ddraw.h>

#include "mw2shim.h"

static BOOL	(WINAPI *TrueBitBlt)(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop) = BitBlt;
static HANDLE (WINAPI *TrueFindFirstFileA)(LPCSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData) = FindFirstFileA;
static BOOL (WINAPI *TrueFindNextFileA)(HANDLE hFindFile, LPWIN32_FIND_DATA lpFindFileData) = FindNextFileA;
static BOOL (WINAPI *TrueFindClose)(HANDLE hFindFile) = FindClose;
static BOOL (WINAPI *TrueHeapFree)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) = HeapFree;
static HRESULT (WINAPI *TrueDirectDrawCreate)(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter);

static HRESULT (STDMETHODCALLTYPE *TrueCreateSurface)(void *, LPDDSURFACEDESC, LPDIRECTDRAWSURFACE *, IUnknown *);
static HRESULT (STDMETHODCALLTYPE *TrueUnlock)(IDirectDrawSurface *, LPVOID);

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

static DWORD limitlasttime, limitavg;
static int fpscalls, fpslasttime, lastfps = -1;
static int LimitRate(int rate) {
  DWORD t = GetTickCount();
  int timetaken, delta;
  
  fpscalls++;
  if(!fpslasttime)
    fpslasttime = t;
  
  if(t - fpslasttime > 1000) {
    lastfps = fpscalls;
    fpscalls = 0;
    fpslasttime = t;
  }
  if(!limitlasttime) {
    limitlasttime = t;
    return lastfps;
  }
  
  timetaken = t - limitlasttime;
  limitlasttime = t;
  
  if(!limitavg) {
    limitavg = timetaken;
  } else {
    limitavg = (DWORD)((1 - ALPHA) * (double)limitavg + (ALPHA) * (double)timetaken);
  }
  delta = rate - limitavg;
  if(delta > 10)
    Sleep(delta);

  t+=delta;
  
  while(GetTickCount() < t)
    ;
    
  return lastfps;
}

static int frameratelimit;
static const char *setupframeratelimit(char *args) {
  frameratelimit = 1000 / atoi(args);
  if(frameratelimit < 1 || frameratelimit > 1000)
    return "Bad frame rate supplied.";

  return NULL;
}

static HRESULT STDMETHODCALLTYPE FixedUnlock(IDirectDrawSurface *p, LPVOID a) {
  LimitRate(frameratelimit); 

  return TrueUnlock(p, a);
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

static hunk hframerate[] = {
  { HUNK_NAME, (void *)&FixedDirectDrawCreate, NULL, "ddraw.dll", "DirectDrawCreate", &TrueDirectDrawCreate },
};

int patchcount = 4;

patch patches[] = { 
  { "startup", "Fixes startup termination", 1, hstartup },
  { "mechlab", "Fixes Mech Lab overweight issue", 3, hmechlab },
  { "heaphack", "Fixes a lot of random crashes but will increase memory usage.", 1, hheaphack },
  { "frameratelimit", "Fixes jumpjet and missile problems", 1, hframerate, "30", "[frame rate in frames/second]", setupframeratelimit },
};
