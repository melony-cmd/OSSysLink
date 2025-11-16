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

#define MAXADDRLEN 256

#ifdef _WIN32
#define SOCKET_TYPE SOCKET
#else
#define SOCKET_TYPE int
#endif

struct UAEOSSYSLINKBase {
	uae_u8 dummy[LIBRARY_SIZEOF];
	struct ossyslinkbase *oslb;
	uae_u8 scratchbuf[SCRATCHBUFSIZE];
};

/* allocated and maintained on a per-task basis */
struct ossyslinkbase {
	struct ossyslinkbase *next;
	struct ossyslinkbase *nextsig;	/* queue for tasks to signal */

	uaecptr sysbase;
	int dosignal;		/* signal flag */
	uae_u32 ownertask;		/* task that opened the library */
	int signal;			/* signal allocated for that task */
	int sb_errno, sb_herrno;	/* errno and herrno variables */
	uae_u32 errnoptr, herrnoptr;	/* pointers */
	uae_u32 errnosize, herrnosize;	/* pinter sizes */
	int dtablesize;		/* current descriptor/flag etc. table size */
	SOCKET_TYPE *dtable;	/* socket descriptor table */
	int *ftable;		/* socket flags */
	int resultval;
	uae_u32 hostent;		/* pointer to the current hostent structure (Amiga mem) */
	uae_u32 hostentsize;
	uae_u32 protoent;		/* pointer to the current protoent structure (Amiga mem) */
	uae_u32 protoentsize;
	uae_u32 servent;		/* pointer to the current servent structure (Amiga mem) */
	uae_u32 serventsize;
	uae_u32 sigstosend;
	uae_u32 eventsigs;		/* EVENT sigmask */
	uae_u32 eintrsigs;		/* EINTR sigmask */
	int eintr;			/* interrupted by eintrsigs? */
	int eventindex;		/* current socket looked at by GetSocketEvents() to prevent starvation */
	uae_u32 logstat;
	uae_u32 logptr;
	uae_u32 logmask;
	uae_u32 logfacility;
	uaecptr fdcallback;
	uae_u64 bytestransmitted, bytesreceived;

	unsigned int *mtable;	/* window messages allocated for asynchronous event notification */
	/* host-specific fields below */
#ifdef _WIN32
	SOCKET_TYPE sockAbort;	/* for aborting WinSock2 select() (damn Microsoft) */
	SOCKET_TYPE sockAsync;	/* for aborting WSBAsyncSelect() in window message handler */
	int needAbort;		/* abort flag */
	void *hAsyncTask;		/* async task handle */
	void *hEvent;		/* thread event handle */
#else
	uae_sem_t sem;		/* semaphore to notify the socket thread of work */
	uae_thread_id thread;	/* socket thread */
	int  sockabort[2];		/* pipe used to tell the thread to abort a select */
	int action;
	int s;			/* for accept */
	uae_u32 name;		/* For gethostbyname */
	uae_u32 a_addr;		/* gethostbyaddr, accept */
	uae_u32 a_addrlen;		/* for gethostbyaddr, accept */
	uae_u32 flags;
	void *buf;
	uae_u32 len;
	uae_u32 to, tolen, from, fromlen;
	int nfds;
	uae_u32 sets [3];
	uae_u32 timeout;
	uae_u32 sigmp;
#endif
#ifdef AMIBERRY
	TrapContext *context;
#endif
}; // ossyslinkbase

#define OSLB struct ossyslinkbase *oslb
/**/


/**/
extern uaecptr ossyslinklib_startup (TrapContext*, uaecptr);
extern void ossyslinklib_install (void);
extern void ossyslinklib_reset (void);

#endif
