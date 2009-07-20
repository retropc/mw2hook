/*
 * Copyright (C) 2008 Chris Porter
 * All rights reserved.
 * See LICENSE.txt for licensing information.
 */
 
#ifndef __HOOKDLL_H
#define __HOOKDLL_H

#define LOGFILE "mw2shim.log"
#define CONFIGFILE "mw2shim.conf"

#define ALPHA 0.75

#define DEBUG_LOG

#define VERSION "1.10"

void openlog(char *);
void closelog(void);
void logentry(char *, ...);

#define HUNK_FUNC  0 /* hook by function address */
#define HUNK_NAME  1 /* hook by function name */

typedef struct hunk {
  int type; /* hook type */
  void *fixedfn; /* patched function */
  void *truefn; /* HUNK_FUNC only: function to replace (should be null for other modes) */
  char *dll; /* HUNK_NAME only: dll to hook fnname inside */
  char *fnname; /* HUNK_NAME only: function name to hook */
  void *__origfn; /* internal usage only, used to store original function pointer */
} hunk;

typedef struct patch {
  char *name; /* patch name */
  char *description; /* patch description */
  int hunkcount; /* number of hunks */
  hunk *hunks; /* hunks */
  char *defaultargs; /* OPTIONAL: default arguments shown in usage */
  char *argshelp; /* OPTIONAL: help shown in usage for arguments */

  /* OPTIONAL
   * this function will be called with the arguments buffer,
   * or NULL if the arguments were not supplied.
   * it should return NULL if successful parsing, or an error message.
   *
   * note the arguments will be deallocated immediately after the function call,
   * if you wish to keep them make a copy, and remember to free it in .free()
   *
   * this function will be called before attach during the commit phase.
   */
  const char *(*setup)(char *); 
  void (*free)(); /* OPTIONAL: this function will be called after the patch is detached (before commit). */
} patch;

extern patch patches[];
extern int patchcount;

#endif