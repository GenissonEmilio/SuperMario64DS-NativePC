// gTouchHeld is the name main now gives arm9 0x020a0de8 (TouchInfo[4], called
// data_020a0de8 by the port); src/unnamed/arm9/0203/func_0203bb60.c reads it
// under the new name. hal/portsync3_link_aliases.cpp carries the same row for
// the window targets; mp_comms_seam links neither that file nor auto_bss.cpp,
// and defines the storage itself (tests/mp_comms_seam.cpp). Lane INT093F.
#pragma comment(linker, "/alternatename:_gTouchHeld=_data_020a0de8")
