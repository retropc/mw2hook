/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#include "mw2hooksetup.h"
#include <shellapi.h>
#include <shlobj.h>
#include <string.h>
#include <stdio.h>

#define VERSION "1.00"

static int CALLBACK bfcallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {
  char *defaultdir = (char *)lpData;
  if(uMsg != BFFM_INITIALIZED)
    return 0;
  
  if(defaultdir)
    SendMessage(hwnd, BFFM_SETSELECTION, 1, (LPARAM)defaultdir);
  return 0;
}

static int getfolder(char *buf, char *title, char *defaultdir) {
  LPITEMIDLIST p;
  BROWSEINFOA bi;
  
  memset(&bi, 0, sizeof(bi));
  bi.pszDisplayName = buf;
  bi.lpszTitle = title;
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
  bi.lpfn = bfcallback;
  bi.lParam = (LPARAM)defaultdir;
  
  p = SHBrowseForFolder(&bi);
  if(!p)
    return 0;
    
  if(SHGetPathFromIDList(p, buf))
    return 1;
    
  CoTaskMemFree(p);
  return 1;
}

static int getinstallationdir(char *dir) {
  int createddir = 0, r;
  char progfiles[MAX_PATH], createdirbuf[MAX_PATH*2], *defaultdir = NULL;
  
  if(SHGetSpecialFolderPath(0, progfiles, CSIDL_PROGRAM_FILES, 0)) {
    int r;
    sprintf_s(createdirbuf, sizeof(createdirbuf), "%s\\%s", progfiles, "MW2Hook");
    r = directoryexists(createdirbuf);
    if(!r) {
      if(CreateDirectory(createdirbuf, 0)) {
        createddir = 1;
        defaultdir = createdirbuf;
      }
    } else {
      defaultdir = createdirbuf;
    }
  }
  
  r = getfolder(dir, "Select the directory you'd like to install MW2Hook into:", defaultdir);
  if(createddir)
    RemoveDirectory(createdirbuf);
    
  return r;    
}


static int copyfiles(char *dir) {
  SHFILEOPSTRUCT s;
  
  memset(&s, 0, sizeof(s));
  
  s.wFunc = FO_COPY;
  s.pFrom = files;
  s.pTo = dir;
  
  return !SHFileOperation(&s);
}

static int licenseagreement(void) {
  if(MessageBox(0, "MW2Hook: fixes MechWarrior 2 under Windows XP\r\nhttp://www.warp13.co.uk/mech2\r\n\r\nCopyright (C) Chris Porter 2008, all rights reserved.\r\n\r\nThis program will install MW2Hook and can also create shortcuts to enable easy use of MW2Inject.\r\n\r\nTo install you must accept the following terms:\r\n\r\n"
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\r\n\r\n"
    "Do you accept these terms?", MSGBOX_TITLE, MB_ICONQUESTION | MB_YESNO) != IDYES)
      return 0;
  return 1;
}

static int getinstallpath(char *key, char *inbuf, DWORD bufsize, char *exename) {
  char buf[MAX_PATH];
  DWORD bufsize2 = bufsize;
  HKEY h;
  DWORD r, typ = REG_SZ;
  size_t len, len2;
  char *p;
  
  sprintf_s(buf, sizeof(buf), "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s", key);
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, KEY_READ, &h) != ERROR_SUCCESS)
    return 0;
    
  r = RegQueryValueEx(h, "UninstallString", NULL, &typ, inbuf, &bufsize2);
  RegCloseKey(h);
    
  if(r != ERROR_SUCCESS)
    return 0;
  
  p = strstr(inbuf, "uninst.exe -f");
  if(!p)
    return 0;
    
  len = strlen("uninst.exe -f");
  memmove(inbuf, p + len, strlen(p + len) + 1);
  len = strlen(inbuf);
  len2 = strlen("\\DeIsL1.isu");
  if(_strcmpi(inbuf + len - len2, "\\DeIsL1.isu"))
    return 0;
  *(inbuf + len - len2) = '\0';

  if(!directoryexists(inbuf))
    return 0;
    
  if(GetLongPathName(inbuf, buf, sizeof(buf)))
    strcpy_s(inbuf, bufsize, buf);
  
  sprintf_s(buf, sizeof(buf), "%s\\%s", inbuf, exename);
  if(fileexists(buf))
    return 1;
  return 0;
}

static char *installexes[] = { "Ghost Bear's Legacy", "GBLDeinstKey", "gblwin.exe", "Mercenaries", "MercenariesDeinstKey", "mercswin.exe", "MechWarrior 2", "MW2W95DeinstKey", "mech2.exe" };
#define INSTALLEXECOUNT 3

static int createshortcut(char *desktopdir, char *hookdir, char *gamename, char *gamepath, char *gameexe) {
  HRESULT h;
  IShellLink *psl;
  IPersistFile *ppf;
  char buf[1024];
  WCHAR wpath[MAX_PATH];
    
  h = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID*)&psl);
  if(!SUCCEEDED(h))
    return 0;
    
  sprintf_s(buf, sizeof(buf), "\"%s\\%s\"", gamepath, gameexe);
  psl->lpVtbl->SetArguments(psl, buf);
  sprintf_s(buf, sizeof(buf), "%s\\%s", gamepath, gameexe);
  psl->lpVtbl->SetIconLocation(psl, buf, 0);
  
  sprintf_s(buf, sizeof(buf), "\"%s\"", gamepath);
  psl->lpVtbl->SetWorkingDirectory(psl, gamepath);
  
  sprintf_s(buf, sizeof(buf), "\"%s\\%s\"", hookdir, EXENAME);
  psl->lpVtbl->SetPath(psl, buf);
  
  h = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (LPVOID *)&ppf);
  if(!SUCCEEDED(h)) {
    psl->lpVtbl->Release(psl);
    return 0;
  }
    
  sprintf_s(buf, sizeof(buf), "%s\\%s (MW2Hook).lnk", desktopdir, gamename);
  MultiByteToWideChar(CP_ACP, 0, buf, -1, wpath, MAX_PATH);
  ppf->lpVtbl->Save(ppf, wpath, TRUE);
  ppf->lpVtbl->Release(ppf);
  
  return 1;
}

#define SHORTCUTHELP "hortcuts can be made by:\r\n- Creating a shortcut to mw2inject.exe (in the installation directory) with the arguments set to the game executable.\r\n- Compatibility mode set to Windows 95 on the game executable but not the mw2inject shortcut (go to properties for the main exe, compatibility tab)."

static int setcompatmode(char *gamepath, char *gameexe) {
  HKEY h;
  char buf[MAX_PATH * 2];
  int r;
  
  if(RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, KEY_WRITE, &h) != ERROR_SUCCESS)
    return 0;
    
  sprintf_s(buf, sizeof(buf), "%s\\%s", gamepath, gameexe);  
  r = RegSetValueEx(h, buf, 0, REG_SZ, "WIN95", 5);
  RegCloseKey(h);
  
  if(r != ERROR_SUCCESS)
    return 0;  
  
  /* this is a bit naughty */
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, KEY_WRITE, &h) != ERROR_SUCCESS)
    return 0;
      
  r = RegSetValueEx(h, buf, 0, REG_SZ, "DisableNXShowUI", 15);
  RegCloseKey(h);
  
  if(r != ERROR_SUCCESS)
    return 0;  
    
  return 1;
}

static void createshortcuts(char *installdir) {
  int found[INSTALLEXECOUNT], i, foundany = 0;
  char installloc[INSTALLEXECOUNT][MAX_PATH];
  char foundbuf[INSTALLEXECOUNT * 100];
  char desktopbuf[MAX_PATH];
  
  memset(found, 0, sizeof(found));
  
  strcpy_s(foundbuf, sizeof(foundbuf), "I found the following MW2 games: ");
  for(i=0;i<INSTALLEXECOUNT;i++) {
    if(!getinstallpath(installexes[i * 3 + 1], installloc[i], MAX_PATH, installexes[i * 3 + 2]))
      continue;
    if(foundany)
      strcat_s(foundbuf, sizeof(foundbuf), ", ");
    strcat_s(foundbuf, sizeof(foundbuf), installexes[i * 3]);
    found[i] = 1;
    foundany = 1;
  }
  
  if(!foundany) {
    /* the S here is deliberate! */
    MessageBox(0, "I couldn't find any game installations on your machine!\r\n\r\nS" SHORTCUTHELP, MSGBOX_TITLE, MB_ICONINFORMATION);
    return;
  }
  
  strcat_s(foundbuf, sizeof(foundbuf), ".\r\n\r\nCreate shortcuts for them on the desktop?\r\n\r\nNote: creating shortcuts manually is complex!");
  MessageBox(0, foundbuf, MSGBOX_TITLE, MB_ICONQUESTION|MB_YESNO);
  
  if(!SHGetSpecialFolderPath(0, desktopbuf, CSIDL_DESKTOP, 0)) {
    WARNING("Couldn't create shortcuts (desktop not found).");
    return;
  }
  for(i=0;i<INSTALLEXECOUNT;i++) {
    if(!found[i])
      continue;
      
    if(!createshortcut(desktopbuf, installdir, installexes[i * 3], installloc[i], installexes[i * 3 + 2])) {
      char buf[1024];
      sprintf_s(buf, sizeof(buf), "Couldn't create shortcut for %s.", installexes[i * 3]);
      WARNING(buf);
    }
    if(!setcompatmode(installloc[i], installexes[i * 3 + 2])) {
      char buf[1024];
      sprintf_s(buf, sizeof(buf), "Couldn't set compatibility mode for %s.", installexes[i * 3]);
      WARNING(buf);
    }
  }
  
  MessageBox(0, "More s" SHORTCUTHELP, MSGBOX_TITLE, MB_ICONINFORMATION);
}

static void createrecord(char *installdir) {
  HKEY h;
  DWORD len;
  
  if(RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Warp13\\MW2Hook", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &h, NULL) != ERROR_SUCCESS)
    return;
    
  len = (DWORD)strlen(installdir);
  RegSetValueEx(h, "InstallDir", 0, REG_SZ, installdir, len);
  
  len = (DWORD)strlen(VERSION);
  RegSetValueEx(h, "InstallVersion", 0, REG_SZ, VERSION, len);
  
  RegCloseKey(h);
}

int setup(void) {
  char installdir[MAX_PATH + 1];
  int dircreated = 0;
  
  if(!licenseagreement() || !getinstallationdir(installdir)) {
    FATAL("Installation aborted.");
    return 1;
  }
  
  if(!directoryexists(installdir)) {
    if(!CreateDirectory(installdir, NULL)) {
      FATAL("Unable to create destination directory.");
      return 2;
    }
    dircreated = 1;
  }
  
  installdir[strlen(installdir) + 1] = '\0';
  if(!copyfiles(installdir)) {
    if(dircreated)
      RemoveDirectory(installdir);
    FATAL("Installation failed.");
    return 3;
  }

  createrecord(installdir);
  createshortcuts(installdir);
  
  MessageBox(0, "Installation complete!", MSGBOX_TITLE, MB_ICONINFORMATION);
  return 0;
}
