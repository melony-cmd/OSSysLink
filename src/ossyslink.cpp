#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "autoconf.h"
#include "traps.h"
#include "ossyslink.h"

int log_osl = 0;
struct syslinkbase *ossyslinkbase;
static uae_u32 OSSysLinkLibBase;

static uae_u32 res_name, res_id, res_init;
static uae_u32 functable, datatable, inittable;

static uae_sem_t sem_queue;

/* ---------------------------------------------------------------------------

   ASSIST FUNCTIONS

   --------------------------------------------------------------------------- */

/* Get current task */
static uae_u32 gettask (TrapContext *ctx)
{
	uae_u32 currtask, a1 = trap_get_areg(ctx, 1);
	TCHAR *tskname;

	trap_call_add_areg(ctx, 1, 0);
	currtask = trap_call_lib(ctx, trap_get_long(ctx, 4), -0x126); /* FindTask */

	trap_set_areg(ctx, 1, a1);

	if (ISOSLTRACE) {
		uae_char name[256];
		trap_get_string(ctx, name, trap_get_long(ctx, currtask + 10), sizeof name);
		tskname = au(name);
		OSLTRACE ((_T("[%s] "), tskname));
		xfree (tskname);
	}

	return currtask;
}

/* Allocate and initialize per-task state structure */
static struct syslinkbase *alloc_ossyslinkbase (TrapContext *ctx)
{
	SLB;
	int i;

	if ((slb = xcalloc (struct syslinkbase, 1)) != NULL) {
		slb->ownertask = gettask(ctx);
		slb->sysbase = trap_get_long(ctx, 4);

		trap_call_add_dreg(ctx, 0, -1);
		slb->signal = trap_call_lib(ctx, slb->sysbase, -0x14A); /* AllocSignal */

		if (slb->signal == -1) {
			write_log (_T("ossyslink: ERROR: Couldn't allocate signal for task 0x%08x.\n"), slb->ownertask);
			free (slb);
			return NULL;
		}

		slb->dtablesize = DEFAULT_DTABLE_SIZE;
		slb->dtable = xmalloc(int, slb->dtablesize);
		slb->ftable = xcalloc(int, slb->dtablesize);

		for (i = slb->dtablesize; i--;)
			slb->dtable[i] = -1;

		slb->eintrsigs = 0x1000; /* SIGBREAKF_CTRL_C */

		slb->logfacility = 1 << 3; /* LOG_USER */
		slb->logmask = 0xff;

    if (ossyslinkbase)
			slb->next = ossyslinkbase;
		ossyslinkbase = slb;

		return slb;
	}
	return NULL;
}

/* ---------------------------------------------------------------------------

   STANDARD FUNCTIONS

   --------------------------------------------------------------------------- */

static uae_u32 REGPARAM2 ossyslinklib_init (TrapContext *ctx)
{
  TCHAR verStr[32];
  uae_u32 tmp1;

  write_log(_T("Creating OSSYSLINK.library 0.1\n"));

  if (OSSysLinkLibBase)
    ossyslinklib_reset();

	trap_call_add_areg(ctx, 0, functable);
	trap_call_add_areg(ctx, 1, datatable);
	trap_call_add_areg(ctx, 2, 0);
	trap_call_add_dreg(ctx, 0, LIBRARY_SIZEOF);
	trap_call_add_dreg(ctx, 1, 0);
	tmp1 = trap_call_lib(ctx, trap_get_areg(ctx, 6), -0x54); /* MakeLibrary */

  if (!tmp1) {
		write_log (_T("OSSYSLINK: FATAL: Cannot create ossyslink.library!\n"));
		return 0;
	}

	trap_call_add_areg(ctx, 1, tmp1);
	trap_call_lib(ctx, trap_get_areg(ctx, 6), -0x18c); /* AddLibrary */

  OSSysLinkLibBase = tmp1;

 	trap_call_add_dreg(ctx, 0, tmp1);
	trap_call_add_dreg(ctx, 1, 0);
	tmp1 = trap_call_lib(ctx, trap_get_areg(ctx, 6), -0xC6); /* AllocMem */

 	if (!tmp1) {
		write_log (_T("OSSYSLINK: FATAL: Ran out of memory while creating ossyslink.library!\n"));
		return 0;
	}

 	trap_set_dreg(ctx, 0, 1);

  return 0;
}

static uae_u32 REGPARAM2 ossyslinklib_Open (TrapContext *ctx)
{
	uae_u32 result = 0;
	int opencount;
	SLB;

  write_log(_T("Open OSSYSLINK.library 0.1\n"));

 	if ((slb = alloc_ossyslinkbase(ctx)) != NULL) {
    OSLTRACE((_T("alloc_ossyslinkbase() successful. [%d]\n"),slb));                                                       // Appears to have a value at least. 
    OSLTRACE((_T("OSSysLinkLibBase                  [%d]\n"),OSSysLinkLibBase));                                          // This 'likely' shouldn't be 0
    //OSLTRACE((_T("OC                                [%d]\n"),opencount = trap_get_word(ctx, OSSysLinkLibBase + 32) + 1)); // 249? it's not 1?  count doesn't start from 0? weird.
    
		trap_put_word(ctx, OSSysLinkLibBase + 32, opencount = trap_get_word(ctx, OSSysLinkLibBase + 32) + 1);		
		trap_call_add_areg(ctx, 0, functable);
		trap_call_add_areg(ctx, 1, datatable);
		trap_call_add_areg(ctx, 2, 0);
		trap_call_add_dreg(ctx, 0, sizeof (struct UAEOSSYSLINKBase));
		trap_call_add_dreg(ctx, 1, 0);
		result = trap_call_lib(ctx, slb->sysbase, -0x54);

		put_pointer(result + offsetof(struct UAEOSSYSLINKBase, slb), slb);

    OSLTRACE ((_T("%0x [%d]\n"), result, opencount));
	} else {
		OSLTRACE ((_T("failed (out of memory)\n")));
  }

  return result;
}

static uae_u32 REGPARAM2 ossyslinklib_Close (TrapContext *ctx)
{
  write_log(_T("Close OSSYSLINK.library 0.1\n"));

  write_log(_T("Expunge() -> [ignored]\n"));
	return 0;
}

static uae_u32 REGPARAM2 ossyslinklib_Expunge (TrapContext *ctx)
{
  write_log(_T("Expunge OSSYSLINK.library 0.1\n"));

  write_log(_T("Expunge() -> [ignored]\n"));
	return 0;
}

/* ---------------------------------------------------------------------------

   OSSYSLINK Library FUNCTIONS

   --------------------------------------------------------------------------- */

/* WHAT NONE ! REALLY ! YUP NONE, WE'RE STILL IN BUILD MODE THAT'S WHY DUMMY !! */

static const TrapHandler ossyslink_funcs[] = {
	ossyslinklib_init,
  ossyslinklib_Open,
  ossyslinklib_Close,
  ossyslinklib_Expunge
};

static const TCHAR * const funcnames[] = {
	_T("ossyslinklib_init"),
  _T("ossyslinklib_Open"),
  _T("ossyslinklib_Close"),
  _T("ossyslinklib_Expunge")
};

static uae_u32 ossyslink_funcvecs[sizeof (ossyslink_funcs) / sizeof (*ossyslink_funcs)];

/*
 * Amiga Reboots
 */
void ossyslinklib_reset(void)
{

  if (!OSSysLinkLibBase)
    return;

  OSSysLinkLibBase = 0;
  write_log (_T("OSSYSLINK: cleanup start..\n"));

  /* Not much to clean up here yet */

  write_log (_T("OSSYSLINK: cleanup finished\n"));  
}

/*
 * Amiga Startup
 */
uaecptr ossyslinklib_startup(TrapContext *ctx, uaecptr resaddr)
{
  /* ?! one someday we might put it as preferences setting maybe !?
  if (!currprefs.ossyslink_library)
    return redaddr;
  */
  trap_put_word(ctx, resaddr + 0x0, 0x4AFC);
  trap_put_long(ctx, resaddr + 0x2, resaddr);
  trap_put_long(ctx, resaddr + 0x6, resaddr + 0x1A); /* Continue scan here */

  if (kickstart_version >= 37) {
	  trap_put_long(ctx, resaddr + 0xA, 0x84270900 | AFTERDOS_PRI); /* RTF_AUTOINIT, RT_VERSION NT_LIBRARY, RT_PRI */
	} else {
		trap_put_long(ctx, resaddr + 0xA, 0x81270905); /* RTF_AUTOINIT, RT_VERSION NT_LIBRARY, RT_PRI */
	}

  trap_put_long(ctx, resaddr + 0xE, res_name);
	trap_put_long(ctx, resaddr + 0x12, res_id);
	trap_put_long(ctx, resaddr + 0x16, res_init);
	resaddr += 0x1A;
  return resaddr;
}

/*
 * Installs into the System
 */
void ossyslinklib_install(void)
{
	int i;

  res_name = ds (_T("ossyslink.library"));
  res_id = ds (_T("UAE ossyslink.library 0.1"));

	for (i = 0; i < (int) (sizeof (ossyslink_funcs) / sizeof (ossyslink_funcs[0])); i++) {
		ossyslink_funcvecs[i] = here ();
		calltrap (deftrap2 (ossyslink_funcs[i], TRAPFLAG_EXTRA_STACK, funcnames[i]));
		dw (RTS);
	}

	/* FuncTable */
	functable = here ();
	for (i = 1; i < 4; i++)
		dl (ossyslink_funcvecs[i]);	/* Open / Close / Expunge */

	dl (EXPANSION_nullfunc);	/* Null */

	for (i = 4; i < (int) (sizeof (ossyslink_funcs) / sizeof (ossyslink_funcs[0])); i++)
		dl (ossyslink_funcvecs[i]);

	dl (0xFFFFFFFF);		/* end of table */

	/* DataTable */
	datatable = here ();
	dw (0xE000);		/* INITBYTE */
	dw (0x0008);		/* LN_TYPE */
	dw (0x0900);		/* NT_LIBRARY */
	dw (0xE000);		/* INITBYTE */
	dw (0x0009);		/* LN_PRI */
	dw (0xCE00);		/* -50 */
	dw (0xC000);		/* INITLONG */
	dw (0x000A);		/* LN_NAME */
	dl (res_name);
	dw (0xE000);		/* INITBYTE */
	dw (0x000E);		/* LIB_FLAGS */
	dw (0x0600);		/* LIBF_SUMUSED | LIBF_CHANGED */
	dw (0xD000);		/* INITWORD */
	dw (0x0001);		/* LIB_VERSION */
	dw (0x0000);
	dw (0xD000);
	dw (0x0001);		/* LIB_REVISION */
	dw (0x0001);
	dw (0xC000);
	dw (0x0018);		/* LIB_IDSTRING */
	dl (res_id);
	dl (0x00000000);		/* end of table */

	res_init = here ();
	dl (512);
	dl (functable);
	dl (datatable);
	dl (*ossyslink_funcvecs);

  write_log (_T("ossyslink.library installed\n"));
}
