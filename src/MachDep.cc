#include "Utils.hh"

#if defined(GOSS_LINUX_X64)
#include "MachDepLinux.cc"
#elif defined(GOSS_MACOSX_X64)
#include "MachDepMacOSX.cc"
#elif defined(GOSS_WINDOWS_X64)
#include "MachDepWindows.cc"
#else
#error "Something went wrong with the machine-dependent port"
#endif


