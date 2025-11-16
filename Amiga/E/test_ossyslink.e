MODULE 'dos'

MODULE '*ossyslink'

DEF ossyslinkbase : LONG

PROC main()
  ossyslinkbase := OpenLibrary('ossyslink.library', 0)
  
  IF ossyslinkbase = 0
    WriteF('Could not open ossyslink.library\n')
  ELSE
    WriteF('Opened ossyslink.library\n')
  ENDIF

  WriteF('Start -- HostRun()\n')
  HostRun()
  WriteF('End -- HostRun()\n')

  WriteF('Start -- HostCmdA()\n')
  HostCmdA()
  WriteF('End -- HostCmdA()\n')

  CloseLibrary(ossyslinkbase)
ENDPROC