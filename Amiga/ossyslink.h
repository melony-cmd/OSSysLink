#include <exec/types.h>
#include <exec/libraries.h>

extern struct Library *OsSysLinkBase;

void HostRun(void);

#pragma amicall(OsSysLinkBase, 0x24, HostRun())
