MODULE 'dos'

MODULE 'ossyslink'

DEF ossyslinkbase : LONG

PROC main()
  ossyslinkbase := OpenLibrary('ossyslink.library', 0)
  
  IF ossyslinkbase = 0
    WriteF('Could not open ossyslink.library\n')
  ENDIF

  HostRun()

  CloseLibrary(ossyslinkbase)
ENDPROC