#include <windows.h>
#include <detours.h>
#include <stdio.h>

#define VERSION "1.00"

#define FATAL(x) puts(x)

static char spinner[] = "\\|/-";

typedef int (WINAPI *querypatches)(int id, char **name, char **description);
typedef void (WINAPI *queryversion)(char **v);

typedef struct patch {
  char *name;
  char *desc;
  int active;
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
    FATAL("querypatches entry point not found.");
    return NULL;
  }
  
  patchcount = qp(-1, NULL, NULL);
  if(patchcount <= 0) {
    FATAL("Bad patch count.");
    return NULL;
  }
  
  p = (patches *)malloc(sizeof(patches) + patchcount * sizeof(patch));
  if(!p) {
    FATAL("Patch allocation error.");
    return NULL;
  }
  memset(p, 0, sizeof(patches) + patchcount * sizeof(patch));
  
  p->count = patchcount;
  for(i=0;i<patchcount;i++) {
    if(qp(i, &p->p[i].name, &p->p[i].desc)) {
      FATAL("Patch query or allocation error.");
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
  if(!r || r >= sizeof(buf))
    return 0;
    
  if(!GetTempFileName(pbuf, prefix, 0, buf))
    return 0;
  
  return 1;
}

void usage(patches *p) {
  int i;
  
  printf("");
  printf("Available patches:\n");
  for(i=0;i<p->count;i++)
    printf("%-15s: %s\n", p->p[i].name, p->p[i].desc);
    
 
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

int main(int argc, char *argv[]) {
  patches *p;
  char *version, *executable;
  HMODULE shim;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  printf("MW2Inject\r\n=========\r\n\r\nPart of MW2Hook.\r\nCopyright (C) Chris Porter 2008\r\nAll rights reserved.\r\n\r\n");

  if(!SetEnvironmentVariable("mw2shim_dontattach", "1")) {
    FATAL("Unable to set environment.");
    return NULL;
  }

  shim = LoadLibrary("mw2shim.dll");
  if(shim == INVALID_HANDLE_VALUE) {
    FATAL("Shim dll not found.");
    return NULL;
  }

  if(!SetEnvironmentVariable("mw2shim_dontattach", "")) {
    FATAL("Unable to unset environment.");
    FreeLibrary(shim);
    return NULL;
  }
  
  version = getversion(shim);
  printf("DLL version: %s\r\n", version?version:"???");
  printf("EXE version: %s\r\n\r\n", VERSION);
    
  p = getpatches(shim);
  if(!p)
    return 1;

  if(argc < 2) {
    usage(p);
  } else {
    executable = argv[argc - 1];

    printf("Executing %s...\r\n", executable);
    if(!DetourCreateProcessWithDll(executable, executable, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE|CREATE_SUSPENDED, NULL, NULL, &si, &pi, NULL, "mw2shim.dll", NULL)) {
      printf("Error: %d\r\n", GetLastError());
    } else {
      char *ps = spinner;
      int finished;
      
      CONSOLE_SCREEN_BUFFER_INFO csbi; 
      HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
      
      ResumeThread(pi.hThread);
      printf("Waiting... ");
      
      if(h != INVALID_HANDLE_VALUE && !GetConsoleScreenBufferInfo(h, &csbi)) {
        printf("\r\n");
        h = INVALID_HANDLE_VALUE;
      }
      
      /* wait for the log to open */
      for(finished=0;;) {
        if(WaitForSingleObject(pi.hProcess, 100) != WAIT_TIMEOUT) {
          finished = 1;
          break;
        }
        if(h != INVALID_HANDLE_VALUE) {
          if(*(ps++) == '\0')
            ps = spinner;
          FillConsoleOutputCharacter(h, *ps, 1, csbi.dwCursorPosition, NULL);
        }
      }

      if(h != INVALID_HANDLE_VALUE)
        printf("\r\n");
      
      if(!finished) {
        printf("TAILING:\r\n");
        while(WaitForSingleObject(pi.hProcess, 100) == WAIT_TIMEOUT) {
        }
      }
    }
    
    printf("\r\nProgram terminated.");
  }
  
  free(p);
  FreeLibrary(shim);
  getc(stdin);
}

