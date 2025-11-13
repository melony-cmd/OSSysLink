#include <stdio.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <exec/libraries.h>

#include "ossyslink.h"

struct Library *OsSysLinkBase;

int main()
{
  OsSysLinkBase = OpenLibrary("ossyslink.library",0);

  if(OsSysLinkBase){
    printf("Opened ossyslink library.\n");
    
    HostRun();

    CloseLibrary(OsSysLinkBase);
  } else {
      printf("Failed to open ossyslink library.\n");
  }
  return 0;
}