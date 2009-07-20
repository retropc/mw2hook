/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */
 
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <detours.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "mw2shim.h"

typedef struct {
  int active;
  char *args;
} activepatch;

static activepatch *activepatches;

static char *cstrdup(char *data) {
  size_t len = strlen(data);
  char *result = malloc(len + 1);
  if(result != NULL)
    memcpy(result, data, len + 1);

  return result;
}

static void activatepatch(char *name) {
  int i;
  
  char *args = strchr(name, '='), *patch = name;
  if(args != NULL)
    *args++ = '\0';

  for(i=0;i<patchcount;i++) {
    if(!strcmp(patches[i].name, patch)) {
      activepatches[i].active = 1;

      if(args) {
        if(activepatches[i].args)
          free(activepatches[i].args);
        activepatches[i].args = cstrdup(args);
      } else {
        activepatches[i].args = NULL;
      }

      return;
    }
  }
  
  logentry("Unable to activate patch: %s", patch);
}

static void configurepatches(void) {
  char buf[1024], *configfile = CONFIGFILE, *p;
  FILE *config;
  int i;
 
  if(GetEnvironmentVariableA("mw2shim_configfile", buf, sizeof(buf))) {
    configfile = buf;

    logentry("Config envvar found: %s", configfile);
    
    if(!fopen_s(&config, configfile, "r")) {
      logentry("Successfully opened config file.");
    
      while(fgets(buf, sizeof(buf), config) != NULL) {
        /* comments */
        if(buf[0] == '#')
          continue;
        
        /* strip \r\n */
        for(p=buf;*p;p++)
          if(*p == '\n' || *p == '\r')
            *p = '\0';
          
        /* empty lines */
        if(buf[0] == '\0')
          continue;
        
        activatepatch(buf);
      }
    
      fclose(config);
      return;
    } else {
      logentry("Unable to open config file.");
    }
  }
  
  logentry("No config file found, activating all patches...");
  
  for(i=0;i<patchcount;i++)
    activepatches[i].active = 1;
}

static void attachpatches(void) {
  patch *p;
  int i, j;

  DetourTransactionBegin();
    
  DetourUpdateThread(GetCurrentThread());
  for(i=0,p=&patches[0];i<patchcount;i++,p++) {
    char *args;

    if(!activepatches[i].active)
      continue;

    args = activepatches[i].args;
    if(!args || args[0] == '\0')
      args = patches[i].defaultargs;
    if(args) {
      logentry("Attaching: %s %s", p->name, args);
    } else {
      logentry("Attaching: %s", p->name);
    }

    if(patches[i].setup) {
      const char *result = patches[i].setup(args);
      if(activepatches[i].args)
        free(activepatches[i].args);

      if(result != NULL) {
        logentry("Failed setting up: %s", result);
        continue;
      }
    } else if(activepatches[i].args)
      free(activepatches[i].args);

    for(j=0;j<p->hunkcount;j++) {
      if(p->hunks[j].type == HUNK_NAME) {
        p->hunks[j].truefn = DetourFindFunction(p->hunks[j].dll, p->hunks[j].fnname);
        if(!p->hunks[j].truefn) {
          logentry("Failed, was unable to locate '%s' inside '%s'", p->hunks[j].fnname, p->hunks[j].dll);
          continue;
        }

        *((void **)p->hunks[j].__origfn) = p->hunks[j].truefn;
        DetourAttach(p->hunks[j].__origfn, p->hunks[j].fixedfn);
      } else {
        DetourAttach(p->hunks[j].truefn, p->hunks[j].fixedfn);
      }      
    }
  }
    
  DetourTransactionCommit();
}

static void detachpatches(void) {
  patch *p;
  int i, j;
  
  DetourTransactionBegin();
    
  DetourUpdateThread(GetCurrentThread());
  for(i=0,p=&patches[0];i<patchcount;p++,i++) {
    if(!activepatches[i].active)
      continue;
        
    logentry("Detaching: %s", p->name);
    for(j=0;j<p->hunkcount;j++)
      if(p->hunks[j].truefn)
        if(p->hunks[j].fnname)
          DetourDetach(p->hunks[j].__origfn, p->hunks[j].fixedfn);
        else
          DetourDetach(p->hunks[j].truefn, p->hunks[j].fixedfn);

    if(patches[i].free)
      patches[i].free();
  }
    
  DetourTransactionCommit();
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  char logbuf[1024];
  
  if(GetEnvironmentVariableA("mw2shim_dontattach", NULL, 0))
    return TRUE;

  if (dwReason == DLL_PROCESS_ATTACH) {
    if(GetEnvironmentVariableA("mw2shim_logfile", logbuf, sizeof(logbuf))) {
      openlog(logbuf);
    } else {
      openlog(LOGFILE);
    }
    
    logentry("MW2Shim " VERSION " attached to process.");
    
    activepatches = (activepatch *)malloc(sizeof(activepatch) * patchcount);
    if(!activepatches)
      return TRUE;
    memset(activepatches, 0, sizeof(activepatch) * patchcount);

    configurepatches();
    
    attachpatches();
  } else if (dwReason == DLL_PROCESS_DETACH) {
    detachpatches();
    
    free(activepatches);
    
    closelog();
  }
  return TRUE;
}

int WINAPI querypatches(int id, char **name, char **description, char **defaultargs, char **argshelp) {
  if(id < 0)
    return patchcount;
    
  *name = patches[id].name;
  *description = patches[id].description;
  *defaultargs = patches[id].defaultargs;
  *argshelp = patches[id].argshelp;
  return 0;
}

void WINAPI queryversion(char **v) {
  *v = VERSION;
}
