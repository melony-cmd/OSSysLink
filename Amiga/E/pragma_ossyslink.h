#pragma libcall OsSysLinkBase HostRun 1E d0
#pragma libcall OsSysLinkBase HostAllocMem 24 d0 d1
#pragma libcall OsSysLinkBase HostFreeMem 2A a0
#pragma libcall OsSysLinkBase WriteMem 30 A0
#pragma libcall OsSysLinkBase ReadMem 36 A0 D0
#pragma libcall OsSysLinkBase CopyMem 3C A0 A1 D0 D1
#pragma libcall OsSysLinkBase InsertDisk 42 D0
#pragma libcall OsSysLinkBase EjectDisk 48 D0
#pragma libcall OsSysLinkBase ForceWindowBackground 4E
