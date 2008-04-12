/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */
 
#ifndef __HOOKDLL_H
#define __HOOKDLL_H

#define LOGFILE "mw2patch.log"
#define CONFIGFILE "mw2patch.conf"

#define ALPHA 0.75
#define RATE (1000/40)

#define DEBUG_LOG

#define VERSION "2.00"

void openlog(char *);
void closelog(void);
void logentry(char *, ...);

#define HUNK_FUNC  0
#define HUNK_NAME  1

typedef struct hunk {
  int type;
  void *fixedfn;
  void *truefn;
  char *dll;
  char *name;
  void *origfn;
} hunk;

typedef struct patch {
  char *name;
  char *description;
  int patches;
  hunk *hunks;
} patch;

extern patch patches[];
extern int patchcount;

#endif