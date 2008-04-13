/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */

#define WIN32_LEAN_AND_MEAN
 
#include <windows.h>
#include <detours.h>
#include <stdio.h>
#include <stdlib.h>

#define VERSION "1.00"
#define DLLNAME "mw2shim.dll"

#define FATAL(x, ...) { fprintf(stderr, x , ## __VA_ARGS__); pause = 1; }
#define WARNING(x , ...) fprintf(stderr, x , ## __VA_ARGS__)
#define INFO(x , ...) fprintf(stderr, x , ## __VA_ARGS__) 

static char spinner[] = "|/-\\";
static int pause = 0;

typedef int (WINAPI *querypatches)(int id, char **name, char **description);
typedef void (WINAPI *queryversion)(char **v);

typedef struct patch {
  char *name;
  char *desc;
  int inactive;
} patch;

typedef struct patches {
  int count;
  patch p[];
} patches;

patches *getpatches(HMODULE shim) {
  querypatches qp;
  patches *p;
  int i, patchcount;
  
  qp = (querypatches)GetProcAddress(shim, "querypatches");
  if(!qp) {
    FATAL("querypatches entry point not found.\r\n");
    return NULL;
  }
  
  patchcount = qp(-1, NULL, NULL);
  if(patchcount <= 0) {
    FATAL("Bad patch count.\r\n");
    return NULL;
  }
  
  p = (patches *)malloc(sizeof(patches) + patchcount * sizeof(patch));
  if(!p) {
    FATAL("Patch allocation error.\r\n");
    return NULL;
  }
  memset(p, 0, sizeof(patches) + patchcount * sizeof(patch));
  
  p->count = patchcount;
  for(i=0;i<patchcount;i++) {
    if(qp(i, &p->p[i].name, &p->p[i].desc)) {
      FATAL("Patch query or allocation error.\r\n");
      free(p);
      return NULL;
    }
  }
  
  return p;
}

int tempfile(char *prefix, char *buf) {
  char pbuf[MAX_PATH];
  int r;
  
  r = GetTempPath(sizeof(pbuf), pbuf);
  if(!r || r >= sizeof(pbuf))
    return 0;
  
  if(!GetTempFileName(pbuf, prefix, 0, buf))
    return 0;
  
  return 1;
}

void usage(char *name, patches *p) {
  int i;
  
  INFO("Usage: %s [options] [executable]\r\n", name);
  INFO("Options:\r\n  /PAUSE:      Instead of closing the console on termation, wait.\r\n  /-PATCHNAME: Disable a patch.\r\n\r\n");
  INFO("Available patches:\r\n");
  for(i=0;i<p->count;i++)
    INFO("%-15s: %s\r\n", p->p[i].name, p->p[i].desc);
    
  if(p->count > 1)
    INFO("\r\nExample: %s /-%s /-%s gblwin.exe\r\n", name, p->p[0].name, p->p[1].name);
  pause = 1;
}

char *lasterrormessage(void) {
  static char buf[1024];
  
  if(!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, sizeof(buf), NULL))
    return "Error fetching error message.\r\n";
    
  return buf;
}

char *getversion(HMODULE shim) {
  char *v;
  queryversion qv;
  
  qv = (queryversion)GetProcAddress(shim, "queryversion");
  if(!qv)
    return NULL;
  
  qv(&v);
  return v;
}

HANDLE executeprocess(char *executable, char *exeargs, char *dll) {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  char exepathbuf[MAX_PATH * 2];
  
  if(GetFullPathName(executable, sizeof(exepathbuf), (LPSTR)exepathbuf, NULL))
    executable = exepathbuf;

  INFO("Executing \"%s\"... ", executable);
  memset(&pi, 0, sizeof(pi));
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
        
  SetLastError(0);

  if(!DetourCreateProcessWithDll(executable, NULL, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE|CREATE_SUSPENDED, NULL, NULL, &si, &pi, NULL, dll, NULL)) {
    INFO("\r\n");
    FATAL("Error executing (code: %d): %s", GetLastError(), lasterrormessage());
    return INVALID_HANDLE_VALUE;
  } else {    
    ResumeThread(pi.hThread);
    return pi.hProcess;
  }
}

int waitforlog(HANDLE h, HANDLE log) {
  HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO csbi; 
  char *ps;
  int terminated = 0;
  
  if(console != INVALID_HANDLE_VALUE && !GetConsoleScreenBufferInfo(console, &csbi))
    console = INVALID_HANDLE_VALUE;
      
  for(ps=spinner;;) {
    if(WaitForSingleObject(h, 1) != WAIT_TIMEOUT) {
      terminated = 1;
      break;
    }
    if(GetFileSize(log, NULL) > 0)
     break;
      
    if(console != INVALID_HANDLE_VALUE) {      
      if(*(++ps) == '\0')
        ps = spinner;
        
      /* how does this interact with stderr? =) */
      FillConsoleOutputCharacter(console, *ps, 1, csbi.dwCursorPosition, NULL);
    }
  }

  INFO("\r\n");
  
  return terminated;
}

int parsebuf(char *buf, int size) {
  char *p, *lastpos = buf;
  int i;
  
  for(i=0,p=buf;i<size;i++,p++) {
    if(*p == '\r') {
      *p = '\0';
    } else if(*p == '\n') {
      *p = '\0';
      puts(lastpos);
      lastpos = p + 1;
    }
  }
    
  return (int)(lastpos - buf);
}

void taillog(HANDLE h, HANDLE log, int terminated) {
  DWORD lastsize = 0, newsize;
  char *buf = NULL;
  int startpos = 0;
  
  INFO("-------------------------------------------------------------------------------\r\n");
  for(;;) {
    newsize = GetFileSize(log, NULL);
    if(newsize != lastsize) {
      DWORD readbytes;    
      int toread = newsize - lastsize, lastpos;
      char *newbuf;
      
      if(toread > 8192) /* don't read >8192 bytes at a time */
        toread = 8192;  /* could have done this with static buffers but it was much more work */
      lastsize+=toread;
      
      newbuf = (char *)realloc(buf, startpos + toread + 1);
      if(!newbuf) {
        WARNING("Logfile read allocation error.\r\n");
        break;
      }
      buf = newbuf;
      buf[startpos + toread] = '\0';
      
      if(!ReadFile(log, buf + startpos, toread, &readbytes, 0) || (readbytes != toread)) {
        WARNING("Logfile read error.\r\n");
        break;
      }
      
      readbytes+=startpos;
      lastpos = parsebuf(buf, readbytes);
      if(lastpos == readbytes) { /* we read the entire buffer */
        free(buf);
        buf = NULL;
        startpos = 0;
      } else { /* we hit the end of file, but didn't see a newline */
        startpos = readbytes - lastpos;
        memmove(buf, buf + lastpos, startpos);
      }
      if(lastsize != newsize)
        continue;
    }
    
    if(terminated)
      break;
      
    if(WaitForSingleObject(h, 100) != WAIT_TIMEOUT) {
      terminated = 1;
      continue;
    }
  }

  if(buf)
    free(buf);
    
  if(!terminated)
    WaitForSingleObject(h, INFINITE);
  INFO("-------------------------------------------------------------------------------\r\n");
}

/* first argument that doesn't begin with / == program name */
int processargs(int argc, char **argv, char **executable, char **exeargs, patches *p) {
  int i;  
  static char argbuf[4096];
  
  *executable = NULL;
  *exeargs = NULL;
  
  for(i=1;i<argc;i++) {
    if(argv[i][0] == '/') {
      if(argv[i][1] == '-') {
        int j, found;
        for(j=found=0;j<p->count;j++) {
          if(!strcmp(argv[i] + 2, p->p[j].name)) {
            found = 1;
            p->p[j].inactive = 1;
          }
        }
        if(!found) {
          WARNING("No such patch: %s\r\n", argv[i] + 2);
          return 0;
        }
      } else if(!strcmp(argv[i] + 1, "pause")) {
        pause = 1;
      }
    } else {
      *executable = argv[i];
      /* currently not passed */
      /*
      int j;
      for(j=i+1;j<argc;j++) {

      }
      */
      break;
    }
  }
  
  if(!*executable)
    return 0;
  return 1;
}

HANDLE setuplog(char *filename) {
  HANDLE hlogfile = INVALID_HANDLE_VALUE;
  
  if(!tempfile("mw2", filename)) {
    WARNING("Unable to get log filename.\r\n");
  } else if((hlogfile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
    WARNING("Unable to open log file.\r\n");
    DeleteFile(filename);
  } else if(!SetEnvironmentVariable("mw2shim_logfile", filename)) {
    WARNING("Unable to set log environment.\r\n");
    DeleteFile(filename);
  }
  return hlogfile;
}

HANDLE getdll(char *dllname) {
  HANDLE shim;
  
  if(!SetEnvironmentVariable("mw2shim_dontattach", "1")) {
    FATAL("Unable to set environment.\r\n");
    return NULL;
  }

  shim = LoadLibrary(dllname);
  if(shim == INVALID_HANDLE_VALUE) {
    FATAL("Shim dll not found.\r\n");
    return NULL;
  }

  if(!SetEnvironmentVariable("mw2shim_dontattach", "")) {
    FATAL("Unable to unset environment.\r\n");
    FreeLibrary(shim);
    return NULL;
  }
  
  return shim;
}

int efwrite(HANDLE h, char *format, ...) {
  char buf[1024];
  va_list ap;
  size_t len;
  DWORD written;
  
  va_start(ap, format);
  len = vsnprintf_s(buf, sizeof(buf), _TRUNCATE, format, ap);
  va_end(ap);
  
  if(!WriteFile(h, buf, (DWORD)len, &written, 0) || (written != len))
    return 0;
  return 1;
}

char *setupconfig(patches *p) { 
  static char configfile[MAX_PATH];
  HANDLE h;
  
  int found = 0, i;
  for(i=0;i<p->count;i++) {
    if(p->p[i].inactive) {
      found = 1;
      break;
    }
  }
  
  if(!found)
    return NULL;
  
  if(!tempfile("mw2", configfile)) {
    WARNING("Unable to get config filename -- ALL PATCHES WILL BE ACTIVE.\r\n");
    return NULL;
  } else if((h = CreateFile(configfile, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
    WARNING("Unable to open config file -- ALL PATCHES WILL BE ACTIVE.\r\n");
    DeleteFile(configfile);
    return NULL;
  }
  
  efwrite(h, "# automatically generated by mw2inject\r\n");
  for(i=0;i<p->count;i++) {
    if(p->p[i].inactive)
      continue;
      
    if(!efwrite(h, "%s\r\n", p->p[i].name)) {
      WARNING("Unable to write config file -- ALL PATCHES WILL BE ACTIVE.\r\n");
      DeleteFile(configfile);
      CloseHandle(h);
      return NULL;  
    }
  }
  CloseHandle(h);
  if(!SetEnvironmentVariable("mw2shim_configfile", configfile)) {
    WARNING("Unable to set config file environment -- ALL PATCHES WILL BE ACTIVE.\r\n");
    DeleteFile(configfile);
    return NULL;
  }

  return configfile;
}

void closeconfig(char *filename) {
  if(filename)
    DeleteFile(filename);
}

static int setuppath(void) {
  char buf[MAX_PATH], *envbuf, *envbuf2, *p;
  DWORD len, newlen;
  
  if(!GetModuleFileName(NULL, buf, sizeof(buf))) {
    FATAL("Unable to get dll path");
    return 0;
  }
  
  for(p=buf + strlen(buf) - 1;p!=buf;p--) {
    if(*p == '\\') {
      *p = '\0';
      break;
    }
  }
  
  len = GetEnvironmentVariable("PATH", NULL, 0);
  if(!len) {
    FATAL("Unable to lookup PATH envvar.");
    return 0;
  }
  /*      %PATH% ""  ;   dllpath  */
  newlen = len + 2 + 1 + strlen(buf);
  envbuf = (char *)malloc(len + newlen);
  if(!envbuf) {
    FATAL("Path allocation error.");
    return 0;
  }
  /* len includes null terminator */
  envbuf2 = envbuf + len;
  
  if(GetEnvironmentVariable("PATH", envbuf, len) != len - 1) {
    FATAL("dll path envvar fetch error.")
    free(envbuf);
    return 0;
  }
  
  
  len = sprintf_s(envbuf2, newlen, "\"%s\";%s", buf, envbuf);
  MessageBox(0, envbuf2, 0, 0);
  printf("%d %d\n", len, newlen);
  
  if(!SetEnvironmentVariable("PATH", envbuf2)) {
    FATAL("Unable to set dll path.");
    free(envbuf);
    return 0;
  }
  
  free(envbuf);
  return 1;
}

int __main(int argc, char **argv) {
  patches *p;
  char *version, *executable = NULL, *exeargs = NULL;
  HMODULE shim;

  if(!setuppath())
    return 1;
  
  if(!(shim = getdll(DLLNAME)))
    return 1;
    
  version = getversion(shim);
  if(strcmp(version, VERSION)) {
    WARNING("WARNING: DLL and EXE versions don't match!\r\n");
    WARNING("DLL version: %s\r\n", version?version:"???");
    WARNING("EXE version: %s\r\n\r\n", VERSION);
  } else {
    INFO("Version: %s\r\n\r\n", VERSION);
  }
  
  p = getpatches(shim);
  if(!p)
    return 1;

  if(!processargs(argc, argv, &executable, &exeargs, p)) {
    usage("mw2inject.exe", p);
    free(p);
    FreeLibrary(shim);
    
  } else {
    char logfile[MAX_PATH], *configfile;
    HANDLE proc, hlogfile = INVALID_HANDLE_VALUE;
    
    configfile = setupconfig(p);
    free(p);
    FreeLibrary(shim);
        
    hlogfile = setuplog(logfile);

    proc = executeprocess(executable, exeargs, DLLNAME);
    if(proc != INVALID_HANDLE_VALUE) {
      if(hlogfile != INVALID_HANDLE_VALUE) {
        taillog(proc, hlogfile, waitforlog(proc, hlogfile));
          
        CloseHandle(hlogfile);        
        DeleteFile(logfile);
      } else {
        WaitForSingleObject(proc, INFINITE);
      }
      
      INFO("Process terminated.\r\n");
    }
    
    closeconfig(configfile);
  }
  
  return 0;  
}

int main(int argc, char *argv[]) {
  int ret;
  
  INFO("MW2Inject\r\n=========\r\n\r\nCopyright (C) Chris Porter 2008, all rights reserved.\r\n");
  
  ret = __main(argc, argv);
  if(pause) {
    INFO("\r\nPress enter to continue.\r\n");
    getc(stdin);
  }
  
  return ret;
}
