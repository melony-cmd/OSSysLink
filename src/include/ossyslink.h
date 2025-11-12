/*
 * UAE - The Un*x Amiga Emulator
 *
 * osyslink.library - system bridge between emulation and the host emulation
 *
 * Copyright 2025,26 Timothy John Roughton
 *
 */

#ifndef UAE_OSSYSLINK_H
#define UAE_OSSYSLINK_H

#include "uae/types.h"
#include "traps.h"

#define OSL_TRACING_ENABLED 1

extern int log_osl;

#define ISOSLTRACE (log_osl || OSL_TRACING_ENABLED) 
#define OSLTRACE(x) do { if (ISOSLTRACE) { write_log x; } } while(0)

#define DEFAULT_DTABLE_SIZE 64

#define LIBRARY_SIZEOF 36
#define SCRATCHBUFSIZE 128

#ifdef _WIN32
//#define OSL_TYPE SOCKET -- er? fucked if I know, fucked if I care, fuck windows!
#else
#define OSL_TYPE int
#endif

struct UAEOSSYSLINKBase {
	uae_u8 dummy[LIBRARY_SIZEOF];
	struct ossyslinkbase *slb;
	uae_u8 scratchbuf[SCRATCHBUFSIZE];
};

struct syslinkbase {
  struct syslinkbase *next;
  struct syslinkbase *nextsig;	/* queue for tasks to signal */

  uaecptr sysbase;  
  int dosignal;
  uae_u32 ownertask;
  int signal;
 	int dtablesize;		/* current descriptor/flag etc. table size */
  OSL_TYPE *dtable;	/* socket descriptor table */
 	int *ftable;		  /* socket flags */
 	uae_u32 eintrsigs;		/* EINTR sigmask */
  uae_u32 logmask;
	uae_u32 logfacility;

};

#define SLB struct syslinkbase *slb

extern uaecptr ossyslinklib_startup (TrapContext*, uaecptr);
extern void ossyslinklib_install (void);
extern void ossyslinklib_reset (void);

#endif
