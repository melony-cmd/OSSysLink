/* ---------------------------------------------------------------------------
 *
 * OSSYSLINK.LIBRARY — UAE/Amiberry Host Support Library
 *
 * Provides a minimal example of how to build an AmigaOS-style shared library
 * that integrates with UAE trap handling.
 *
 * Version: 0.1
 * Author: Tim Roughton (OSSysLink)
 * 
 * At this point I consider this code broken, it's not opening Host_Run() it's
 * expunging the instant the function is called, it being call from the correct
 * offset, unless AmigaE is doing something funky with the pragma file.
 * 
 * So a complete rewrite is in order :( how? copy bsdsocket.library in full and
 * complete and gut the thing one line at at time, one function at a time.
 * 
 * Because writting this using bsdsocket.library as inspiration clearly isn't
 * working.
 *
 * --------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------
   OS INCLUDES
   --------------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL.h>

#if defined(SDL_VIDEO_DRIVER_X11)
  #include <X11/Xlib.h>
  #include <X11/Xatom.h>
#endif

#include <unistd.h>

/* ---------------------------------------------------------------------------
   UAE STANDARD INCLUDES
   --------------------------------------------------------------------------- */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "autoconf.h"
#include "traps.h"
#include "amiberry_gfx.h"
#include "ossyslink.h"
#include "ossyslinkinternals.h"

/* ---------------------------------------------------------------------------
   GLOBAL
   --------------------------------------------------------------------------- */

int log_osl = 0;
struct ossyslinkbase *ossyslinkbase;
static uae_u32 OSSysLinkLibBase;

static uae_u32 res_name, res_id, res_init;
static uae_u32 functable, datatable, inittable;

static uae_sem_t sem_queue;
static uae_u32 osl_forcewindowbackground (void);

static uae_u32 osl_forcewindowbackground (void);
extern "C" AmigaMonitor* gfx_get_monitor(int idx);
extern "C" void osl_forcewindowbackground_cxx(void) { osl_forcewindowbackground(); };

/* ---------------------------------------------------------------------------
   EMULATION MAPPED SUPPORT FUNCTIONS (PRIVATE CURRENTLY)
   --------------------------------------------------------------------------- */
#include <string>
#include <cstdint>

// --- Key type ---
enum KeyType { KEY_STRING, KEY_INT };

struct MapNode {
    KeyType type;
    std::string keyStr;
    uintptr_t keyInt;
    uintptr_t value;
    MapNode *next;
};

struct Map {
    size_t size;
    size_t count;
    MapNode **buckets;
};

// --- Hash a string ---
static size_t HashString(const std::string &key, size_t size) {
    size_t h = 5381;
    for (char c : key) h = ((h << 5) + h) + static_cast<unsigned char>(c);
    return h % size;
}

// --- Hash an integer key ---
static size_t HashInt(uintptr_t v, size_t size) {
    v = (v ^ (v >> 33)) * 0xff51afd7ed558ccdULL;
    v = (v ^ (v >> 33)) * 0xc4ceb9fe1a85ec53ULL;
    v =  v ^ (v >> 33);
    return v % size;
}

// --- Create map ---
Map* NewMap() {
    Map *map = new Map;
    map->size = 16;
    map->count = 0;
    map->buckets = new MapNode*[map->size]();
    return map;
}

// --- Add string key ---
void AddMapElement(Map *map, const std::string &key, uintptr_t value) {
    size_t index = HashString(key, map->size);

    MapNode *node = map->buckets[index];
    while (node) {
        if (node->type == KEY_STRING && node->keyStr == key) {
            node->value = value;
            return;
        }
        node = node->next;
    }

    MapNode *newNode = new MapNode;
    newNode->type = KEY_STRING;
    newNode->keyStr = key;
    newNode->value = value;
    newNode->next = map->buckets[index];
    map->buckets[index] = newNode;
    map->count++;
}

// --- Add integer key ---
void AddMapElement(Map *map, uintptr_t key, uintptr_t value) {
    size_t index = HashInt(key, map->size);

    MapNode *node = map->buckets[index];
    while (node) {
        if (node->type == KEY_INT && node->keyInt == key) {
            node->value = value;
            return;
        }
        node = node->next;
    }

    MapNode *newNode = new MapNode;
    newNode->type = KEY_INT;
    newNode->keyInt = key;
    newNode->value = value;
    newNode->next = map->buckets[index];
    map->buckets[index] = newNode;
    map->count++;
}

// --- Delete string key ---
bool DeleteMapElement(Map *map, const std::string &key) {
    size_t index = HashString(key, map->size);
    MapNode *node = map->buckets[index];
    MapNode *prev = nullptr;
    while (node) {
        if (node->type == KEY_STRING && node->keyStr == key) {
            if (prev) prev->next = node->next;
            else map->buckets[index] = node->next;
            delete node;
            if (map->count > 0) map->count--;
            return true;
        }
        prev = node;
        node = node->next;
    }
    return false;
}

// --- Delete int key ---
bool DeleteMapElement(Map *map, uintptr_t key) {
    size_t index = HashInt(key, map->size);
    MapNode *node = map->buckets[index];
    MapNode *prev = nullptr;
    while (node) {
        if (node->type == KEY_INT && node->keyInt == key) {
            if (prev) prev->next = node->next;
            else map->buckets[index] = node->next;
            delete node;
            if (map->count > 0) map->count--;
            return true;
        }
        prev = node;
        node = node->next;
    }
    return false;
}

// --- Lookup string ---
bool MapKey(Map *map, const std::string &key, uintptr_t &out) {
    size_t index = HashString(key, map->size);
    MapNode *node = map->buckets[index];

    while (node) {
        if (node->type == KEY_STRING && node->keyStr == key) {
            out = node->value;
            return true;
        }
        node = node->next;
    }
  return false;
}

// --- Lookup integer ---
bool MapKey(Map *map, uintptr_t key, uintptr_t &out) {
    size_t index = HashInt(key, map->size);
    MapNode *node = map->buckets[index];

    while (node) {
        if (node->type == KEY_INT && node->keyInt == key) {
            out = node->value;
            return true;
        }
        node = node->next;
    }
    return false;
}

// --- Free ---
void FreeMap(Map *map) {
    if (!map) return;
    for (size_t i = 0; i < map->size; ++i) {
        MapNode *node = map->buckets[i];
        while (node) {
            MapNode *next = node->next;
            delete node;
            node = next;
        }
    }
    delete[] map->buckets;
    map->buckets = nullptr;
    map->count = 0;    // defensive, not required
    map->size = 0;     // defensive
    delete map;
}

Map *memory_table = NewMap();

/* ---------------------------------------------------------------------------
   EMULATION SUPPORT FUNCTIONS
   --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * gettask()
 *
 *  Retrieves the pointer to the current Amiga task structure.
 *
 *  Uses the system's FindTask() function via a library trap call.
 *
 *  Parameters:
 *    ctx - Trap execution context
 *
 *  Returns:
 *    Address of the current task (pointer in Amiga memory space)
 * --------------------------------------------------------------------------- */
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

/* ---------------------------------------------------------------------------
 * alloc_ossyslinkbase()
 *
 *  Allocates and initializes a new per-task OSSysLinkBase structure.
 *  This structure stores task-specific context, descriptor tables, and
 *  allocated signal numbers for asynchronous communication.
 *
 *  Parameters:
 *    ctx - Trap execution context
 *
 *  Returns:
 *    Pointer to a newly allocated ossyslinkbase structure (in host memory)
 *    or NULL on failure.
 * --------------------------------------------------------------------------- */
static struct ossyslinkbase *alloc_ossyslinkbase (TrapContext *ctx)
{
	OSLB;
	int i;

	if ((oslb = xcalloc (struct ossyslinkbase, 1)) != NULL) {
		oslb->ownertask = gettask(ctx);
		oslb->sysbase = trap_get_long(ctx, 4);

		trap_call_add_dreg(ctx, 0, -1);
		oslb->signal = trap_call_lib(ctx, oslb->sysbase, -0x14A); /* AllocSignal */

		if (oslb->signal == -1) {
			write_log (_T("ossyslink: ERROR: Couldn't allocate signal for task 0x%08x.\n"), oslb->ownertask);
			free (oslb);
			return NULL;
		}

		oslb->dtablesize = DEFAULT_DTABLE_SIZE;
		oslb->dtable = xmalloc(int, oslb->dtablesize);
		oslb->ftable = xcalloc(int, oslb->dtablesize);

		for (i = oslb->dtablesize; i--;) {
			oslb->dtable[i] = -1;
			oslb->ftable[i] = 0;
		}

		oslb->eintrsigs = 0x1000; /* SIGBREAKF_CTRL_C */

		oslb->logfacility = 1 << 3; /* LOG_USER */
		oslb->logmask = 0xff;

    if (ossyslinkbase)
			oslb->next = ossyslinkbase;
		ossyslinkbase = oslb;

		return oslb;
	}
	return NULL;
}

/* ---------------------------------------------------------------------------
 * get_ossyslinkbase()
 *
 *  Retrieves the per-task OSSysLinkBase structure associated with the given
 *  trap context. This structure stores task-specific context, descriptor
 *  tables, and other relevant runtime information.
 *
 *  Parameters:
 *    ctx - Trap execution context
 *
 *  Returns:
 *    Pointer to the ossyslinkbase structure corresponding to the current
 *    task. The pointer is cast to `struct syslinkbase*`.
 * --------------------------------------------------------------------------- */
STATIC_INLINE struct ossyslinkbase *get_ossyslinkbase (TrapContext *ctx)
{
	return (struct ossyslinkbase*)get_pointer (trap_get_areg(ctx, 6) + offsetof (struct UAEOSSYSLINKBase, oslb));
}

/* ---------------------------------------------------------------------------
 * free_ossyslinkbase()
 *
 *  Releases a per-task OSSysLinkBase structure previously allocated by
 *  alloc_ossyslinkbase(). This should clean up all task-specific resources,
 *  including descriptor tables, allocated signals, and the linked-list entry
 *  used to track active bases.
 *
 *  NOTE:
 *      - Free dtable and ftable arrays
 *      - Free the ossyslinkbase structure itself
 *
 *  Parameters:
 *    ctx - Trap execution context
 *
 *  Returns:
 *    Nothing.
 * --------------------------------------------------------------------------- */
static void free_ossyslinkbase (TrapContext *ctx) 
{
	struct ossyslinkbase *oslb;

	if ((oslb = get_ossyslinkbase(ctx)) != NULL){
		free (oslb->dtable);
		free (oslb->ftable);
	
		free (oslb);
	}

  return;
}

static bool get_x11_handles(SDL_Window* win, Display** dpy_out, Window* win_out)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(win, &info))
        return false;

    if (info.subsystem != SDL_SYSWM_X11)
        return false;

    *dpy_out = info.info.x11.display;
    *win_out = info.info.x11.window;

    return true;
}

/* ---------------------------------------------------------------------------
   STANDARD FUNCTIONS (AmigaOS)
   --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * ossyslinklib_init()
 *
 *  Initializes the library when it is first created.
 *  Creates a new library node in system memory using MakeLibrary(),
 *  adds it to Exec’s library list with AddLibrary(),
 *  and allocates a memory block to represent its base.
 *
 *  Parameters:
 *    ctx - Trap execution context
 *
 *  Returns:
 *    0 (no return value is used by the autoinit system)
 * --------------------------------------------------------------------------- */
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

/* ---------------------------------------------------------------------------
 * ossyslinklib_Open()
 *
 *  Handles OpenLibrary() calls from AmigaOS clients.
 *  Creates a new per-task base using alloc_ossyslinkbase()
 *  and increments the library’s open count.
 *
 *  Parameters:
 *    ctx - Trap execution context
 *
 *  Returns:
 *    Result pointer (unused in this early stub version)
 * --------------------------------------------------------------------------- */
static uae_u32 REGPARAM2 ossyslinklib_Open (TrapContext *ctx)
{
	uae_u32 result = 0;
	int opencount;
	OSLB;

  write_log(_T("Open OSSYSLINK.library 0.1\n"));

 	if ((oslb = alloc_ossyslinkbase(ctx)) != NULL) {
    OSLTRACE((_T("alloc_ossyslinkbase() successful. [%d]\n"),oslb));                                                       // Appears to have a value at least. 
    OSLTRACE((_T("OSSysLinkLibBase                  [%d]\n"),OSSysLinkLibBase));                                          // This 'likely' shouldn't be 0
    //OSLTRACE((_T("OC                                [%d]\n"),opencount = trap_get_word(ctx, OSSysLinkLibBase + 32) + 1)); // 249? it's not 1?  count doesn't start from 0? weird.
    
		trap_put_word(ctx, OSSysLinkLibBase + 32, opencount = trap_get_word(ctx, OSSysLinkLibBase + 32) + 1);		
		trap_call_add_areg(ctx, 0, functable);
		trap_call_add_areg(ctx, 1, datatable);
		trap_call_add_areg(ctx, 2, 0);
		trap_call_add_dreg(ctx, 0, sizeof (struct UAEOSSYSLINKBase));
		trap_call_add_dreg(ctx, 1, 0);
		result = trap_call_lib(ctx, oslb->sysbase, -0x54);

		put_pointer(result + offsetof(struct UAEOSSYSLINKBase, oslb), oslb);

    OSLTRACE ((_T("%0x [%d]\n"), result, opencount));
	} else {
		OSLTRACE ((_T("failed (out of memory)\n")));
  }

  return result;
}

/* ---------------------------------------------------------------------------
 * ossyslinklib_Close()
 *
 *  Handles CloseLibrary() calls.
 *  Currently does not free any task state — acts as a stub.
 * --------------------------------------------------------------------------- */
static uae_u32 REGPARAM2 ossyslinklib_Close (TrapContext *ctx)
{
	int opencount;

	uae_u32 base = trap_get_areg(ctx, 6);
	uae_u32 negsize = get_word (base + 16);

	free_ossyslinkbase(ctx);

	trap_put_word(ctx, OSSysLinkLibBase + 32, opencount = trap_get_word(ctx, OSSysLinkLibBase + 32) - 1);

	trap_call_add_areg(ctx, 1, base - negsize);
	trap_call_add_dreg(ctx, 0, negsize + trap_get_word(ctx, base + 18));
	trap_call_lib(ctx, trap_get_long(ctx, 4), -0xD2); /* FreeMem */

	OSLTRACE ((_T("CloseLibrary() -> [%d]\n"), opencount));
  
  return 0;
}

/* ---------------------------------------------------------------------------
 * ossyslinklib_Expunge()
 *
 *  Handles ExpungeLibrary() calls.
 *  Currently ignored as the library is resident for the emulator lifetime.
 * --------------------------------------------------------------------------- */
static uae_u32 REGPARAM2 ossyslinklib_Expunge (TrapContext *ctx)
{
  write_log(_T("Expunge OSSYSLINK.library 0.1\n"));

  write_log(_T("Expunge() -> [ignored]\n"));
	return 0;
}

/* ---------------------------------------------------------------------------
   OSSYSLINK LIBRARY FUNCTIONS IO
   --------------------------------------------------------------------------- */

/* Run Command on Host Machine */
static uae_u32 REGPARAM2 ossyslinklib_OSLHostRun(TrapContext *ctx) {
	pid_t pid = fork();

	struct ossyslinkbase *oslb = get_ossyslinkbase(ctx);
	uaecptr strptr = trap_get_dreg(ctx,0);  // MOVE THIS TO A D0 REGISTER YOU ASSHOLE!

	printf(" ->> HostRun()\n");

	char buf[256];
	int i = 0;

	for (;;) {
		uae_u8 c = get_byte(strptr+i);
		if (c==0 || i>=sizeof(buf)-1)
			break;
		buf[i++] = c;
	}
	buf[i] = 0;


	if(pid == 0) {
		char *args[] = {buf , NULL};
		execvp(args[0], args);
	}

	printf(" ->> Run ('%s' @ PID=%d)\n",buf,pid);

  return 0;
}

// The Memory AmigaOS knows nothing about, so can be considered 'protected' memory.
static uae_u32 REGPARAM2 ossyslinklib_OSLAllocMem(TrapContext *ctx) {
	printf(" ->> ossyslinklib_OSLAllocMem()\n");
	uae_u32 reqkey = trap_get_dreg(ctx,0);
	uae_u32 memsize32 = trap_get_dreg(ctx,1);
	uintptr_t out;
	size_t  memsize64bit = (size_t) memsize32;

	if(MapKey(memory_table, reqkey, out)) {
		printf("Key already exists\n");
		return OSL_INVALID_KEY;
	} else {
		void *memptr = malloc(memsize64bit);
		AddMapElement(memory_table,reqkey,reinterpret_cast<uintptr_t>(memptr));

		printf("Allocate(%llu)\n",memsize32);
		printf("memptr = (%llu)\n",memptr);
		printf("mapping\n");
	}

	return reqkey;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLFreeMem(TrapContext *ctx) {
    printf(" ->> ossyslinklib_OSLFreeMem()\n");

    uae_u32 key = trap_get_areg(ctx, 0);
    uintptr_t memptr = 0;

    if (!MapKey(memory_table, key, memptr)) {
        printf("OSLFreeMem ERROR: key %u not found!\n", key);
        return 0;   // AmigaOS doesn't get to crash because of it :)
    }

    free(reinterpret_cast<void*>(memptr));

    DeleteMapElement(memory_table, key);

    printf("Memory at key %u freed successfully.\n", key);
    return 0;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLWriteMem(TrapContext *ctx) {
  uae_u32 key = trap_get_areg(ctx, 0);
  uae_u32 data = trap_get_dreg(ctx, 0);
  uae_u32 offset = trap_get_dreg(ctx, 1);
  uintptr_t memptr = 0;

  if (!MapKey(memory_table, key, memptr)) {
    printf("Write ERROR: key %u not found!\n", key);
    return FALSE;
  }

  printf("Attempt write (%d) into (%d)\n",data,memptr);

  return TRUE;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLReadMem(TrapContext *ctx) {
  uae_u32 key = trap_get_areg(ctx, 0);
  uae_u32 data = 0; // technically we don't need this.
  uae_u32 offset = trap_get_dreg(ctx, 1);
  uintptr_t memptr = 0;

  if (!MapKey(memory_table, key, memptr)) {
    printf("Read ERROR: key %u not found!\n", key);
    return FALSE;   
  }
  printf("Attempt read offset (%d) out of (%d)\n",offset,memptr);

  return memptr+offset;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLCopyMem(TrapContext *ctx) {
  return 0;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLInsertDisk(TrapContext *ctx) {
  return 0;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLEjectDisk(TrapContext *ctx) {
  return 0;
}

/* ---------------------------------------------------------------------------
   OSSYSLINK LIBRARY FUNCTIONS UI
   --------------------------------------------------------------------------- */
static uae_u32 REGPARAM2 ossyslinklib_OSLForceWindowPatchMouse(TrapContext *ctx) {
  return 0;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLForceWindowPatchLoop(TrapContext *ctx) {
  return 0;
}

static uae_u32 osl_forcewindowbackground (void) {
  AmigaMonitor *mon = gfx_get_monitor(0);

  SDL_Window *win = mon->amiga_window;
  Display* dpy;
  Window xwin;

  if (!get_x11_handles(win, &dpy, &xwin))
    return False;

  printf("Setting _NET_WM_STATE & _NET_WM_STATE_BELOW\n");
  Atom wmState = XInternAtom(dpy, "_NET_WM_STATE", False);
  Atom wmStateBelow = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);

  XEvent e;
  memset(&e, 0, sizeof(e));

  e.xclient.type = ClientMessage;
  e.xclient.serial = 0;
  e.xclient.send_event = True;
  e.xclient.window = xwin;
  e.xclient.message_type = wmState;
  e.xclient.format = 32;
  e.xclient.data.l[0] = 1;        /* _NET_WM_STATE_ADD */
  e.xclient.data.l[1] = wmStateBelow;
  e.xclient.data.l[2] = 0;
  e.xclient.data.l[3] = 1;

  XSendEvent(dpy,DefaultRootWindow(dpy),False,SubstructureRedirectMask | SubstructureNotifyMask,&e);

  XSetInputFocus(dpy, PointerRoot, RevertToNone, CurrentTime);

  XFlush(dpy);
  return 0;
}

static uae_u32 REGPARAM2 ossyslinklib_OSLForceWindowBackground(TrapContext *ctx) {

  osl_forcewindowbackground();

  return 0;
}

/* ---------------------------------------------------------------------------
   EMULATION FUNCTIONS
   --------------------------------------------------------------------------- */

/* Table of exported trap functions */
static const TrapHandler ossyslink_funcs[] = {
  /* Standard Support Functionality */
	ossyslinklib_init,
  ossyslinklib_Open,
  ossyslinklib_Close,
  ossyslinklib_Expunge,                  /* Everything past this point, I presume/should be custom stuff */  
  ossyslinklib_OSLHostRun,
	ossyslinklib_OSLAllocMem,
	ossyslinklib_OSLFreeMem,
  ossyslinklib_OSLWriteMem,
	ossyslinklib_OSLReadMem,
	ossyslinklib_OSLCopyMem,
	ossyslinklib_OSLInsertDisk,            /* Insert Disk DF0...DF3 */
	ossyslinklib_OSLEjectDisk,             /* Eject Disk DF0...DF3 */
  /* 
     This section is entirely devoted to intergrating the window into the background
     of which there are many few methods.
   */
	ossyslinklib_OSLForceWindowBackground, /* one shot, fire and forget */
  ossyslinklib_OSLForceWindowPatchMouse, /* consistantly attempts to 'force' the window to background on mouse click events. */
  ossyslinklib_OSLForceWindowPatchLoop   /* each, loop the window is forced to the background */
};

/* NB: ossyslinklib_OSLForceWindowPatchMouse & 
       ossyslinklib_OSLForceWindowPatchLoop 

       Are invasive into the emulation code space, and require editing of the orginal source not that this library
       doesn't anyway, but this is over and above just simply adding library setup/install etc.
*/
 
/* Function names for debugging */
static const TCHAR * const funcnames[] = {
  /* Standard Support Functionality */
	_T("ossyslinklib_init"),
  _T("ossyslinklib_Open"),
  _T("ossyslinklib_Close"),
  _T("ossyslinklib_Expunge"), /* Everything past this point, I presume/should be custom stuff */
  _T("ossyslinklib_OSLHostRun"),
	_T("ossyslinklib_OSLAllocMem"),
	_T("ossyslinklib_OSLFreeMem"),
  _T("ossyslinklib_OSLWriteMem"),
	_T("ossyslinklib_OSLReadMem"),
	_T("ossyslinklib_OSLCopyMem"),
	_T("ossyslinklib_OSLInsertDisk"),
  _T("ossyslinklib_OSLEjectDisk"),
  _T("ossyslinklib_OSLForceWindowBackground"),
  _T("ossyslinklib_OSLForceWindowPatchMouse"),
  _T("ossyslinklib_OSLForceWindowPatchLoop")
};

static uae_u32 ossyslink_funcvecs[sizeof (ossyslink_funcs) / sizeof (*ossyslink_funcs)];

/* ---------------------------------------------------------------------------
 * ossyslinklib_reset()
 *
 *  Called when the Amiga is reset or the emulator reboots.
 *  Resets the global base and releases any residual state.
 * --------------------------------------------------------------------------- */
void ossyslinklib_reset(void)
{

  if (!OSSysLinkLibBase)
    return;

  OSSysLinkLibBase = 0;
  write_log (_T("OSSYSLINK: cleanup start..\n"));

  /* Not much to clean up here yet */

  write_log (_T("OSSYSLINK: cleanup finished\n"));  
}

/* ---------------------------------------------------------------------------
 * ossyslinklib_startup()
 *
 *  Called during autoconfiguration to place the resident module header.
 *  Writes a proper Resident structure at resaddr so that Exec can identify
 *  and initialize the library automatically.
 *
 *  Parameters:
 *    ctx     - Trap context
 *    resaddr - Address in memory to place the Resident structure
 *
 *  Returns:
 *    Updated resaddr (next free position)
 * --------------------------------------------------------------------------- */
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

/* ---------------------------------------------------------------------------
 * ossyslinklib_install()
 *
 *  Installs the library into the UAE trap system.
 *  Defines all trap entry points, builds the function and data tables,
 *  and registers the resident module.
 * --------------------------------------------------------------------------- */
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
  {
    write_log (_T("ossyslink: Standard Library Function Vectors [%+d]\n"),ossyslink_funcvecs[i]);
		dl (ossyslink_funcvecs[i]);	/* Open / Close / Expunge */
  }

	dl (EXPANSION_nullfunc);	/* Null */

	for (i = 4; i < (int) (sizeof (ossyslink_funcs) / sizeof (ossyslink_funcs[0])); i++) {
    write_log (_T("ossyslink: User Function Vectors [%+d]\n"),ossyslink_funcvecs[i]);
		dl (ossyslink_funcvecs[i]);
  }
	dl (0xFFFFFFFF);		/* end of table */

  /* Debugging Print the table */

  int count = sizeof(funcnames) / sizeof(funcnames[0]);

  for (int i = 0; i < count; i++) {
    uae_u32 amiga_offset = i * 6;   /*-> this is the real LVO offset*/
    write_log(_T("ossyslink: %s => (%d) %08X => %d(%08X)\n"),funcnames[i],ossyslink_funcvecs[i],ossyslink_funcvecs[i],amiga_offset,amiga_offset);
  }

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
