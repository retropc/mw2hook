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

static int *activepatches;

static void activatepatch(char *name) {
  int i;
  
  for(i=0;i<patchcount;i++) {
    if(!strcmp(patches[i].name, name)) {
      activepatches[i] = 1;
      return;
    }
  }
  
  logentry("Unable to activate patch: %s", name);
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
    activepatches[i] = 1;
}

static void attachpatches(void) {
  patch *p;
  int i, j;

  DetourTransactionBegin();
    
  DetourUpdateThread(GetCurrentThread());
  for(i=0,p=&patches[0];i<patchcount;i++,p++) {
    if(!activepatches[i])
      continue;
        
    logentry("Attaching: %s", p->name);
    for(j=0;j<p->patches;j++) {
      if(p->hunks[j].type == HUNK_NAME) {
        p->hunks[j].truefn = DetourFindFunction(p->hunks[j].dll, p->hunks[j].name);
        if(!p->hunks[j].truefn) {
          logentry("Failed, was unable to locate '%s' inside '%s'", p->hunks[j].name, p->hunks[j].dll);
          continue;
        }

        *((void **)p->hunks[j].origfn) = p->hunks[j].truefn;
        DetourAttach(p->hunks[j].origfn, p->hunks[j].fixedfn);
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
    if(!activepatches[i])
      continue;
        
    logentry("Detaching: %s", p->name);
    for(j=0;j<p->patches;j++)
      if(p->hunks[j].truefn)
        if(p->hunks[j].name)
          DetourDetach(p->hunks[j].origfn, p->hunks[j].fixedfn);
        else
          DetourDetach(p->hunks[j].truefn, p->hunks[j].fixedfn);
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
    
    activepatches = (int *)malloc(sizeof(int) * patchcount);
    if(!activepatches)
      return TRUE;
    
    configurepatches();
    
    attachpatches();
  } else if (dwReason == DLL_PROCESS_DETACH) {
    detachpatches();
    
    free(activepatches);
    
    closelog();
  }
  return TRUE;
}

int WINAPI querypatches(int id, char **name, char **description) {
  if(id < 0)
    return patchcount;
    
  *name = patches[id].name;
  *description = patches[id].description;
  return 0;
}

void WINAPI queryversion(char **v) {
  *v = VERSION;
}
