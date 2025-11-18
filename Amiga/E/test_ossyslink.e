MODULE 'dos'

MODULE '*ossyslink'

DEF ossyslinkbase : LONG

PROC main()

  DEF sendstring[128]:STRING
  DEF memoryptr:LONG

  ossyslinkbase := OpenLibrary('ossyslink.library', 0)
  
  IF ossyslinkbase = 0
    WriteF('Could not open ossyslink.library\n')
  ELSE
    WriteF('Opened ossyslink.library\n')
  ENDIF

  WriteF('HostRun()\n')  
  WriteF('Sending String "\s"\n',sendstring)
  StrCopy(sendstring,'dir')
  HostRun(sendstring)

  WriteF('HostMem Key = \d\n',HostAllocMem(0,1234))
  HostFreeMem(0)

  WriteF('ForceWindowBackground()\n')
  ForceWindowBackground()

  CloseLibrary(ossyslinkbase)
ENDPROC

/* BUG: This will not work yet.
  WriteF('Sending String "ls -lac"\n')
  HostRun('ls -lac')
*/