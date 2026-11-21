/*
 Portable Linux release builds of HyperCut.

 Force-included (-include) by scripts/build-linux-release.sh so the binaries also load on distributions with an older
 glibc than the build machine:
 - glibc >= 2.38 redirects strtol / sscanf & co. to their C23 variants (__isoc23_*); switch that off, the plugin does
   not need C23 parsing (binary literals).
 - glibc 2.43 added new versions of some float math functions; bind to the original ones.
 Normal builds do not use this file.
*/
#ifndef HYPERCUT_GLIBC_COMPAT_H
#define HYPERCUT_GLIBC_COMPAT_H

#if defined(__linux__) && defined(__x86_64__)
#include <features.h>
#if defined(__GLIBC__)
#undef __GLIBC_USE_C23_STRTOL
#define __GLIBC_USE_C23_STRTOL 0
__asm__(".symver log10f,log10f@GLIBC_2.2.5");
__asm__(".symver acosf,acosf@GLIBC_2.2.5");
__asm__(".symver atan2f,atan2f@GLIBC_2.2.5");
#endif
#endif

#endif
