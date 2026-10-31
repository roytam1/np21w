/**
 * @file	np2ver.h
 * @brief	The version
 */

#if defined(SUPPORT_IA32_HAXM)
#define	NP2VER_CORE			"ver.0.86 rev.107HAX"
#elif defined(SUPPORT_IA32_WHPX)
#define	NP2VER_CORE			"ver.0.86 rev.107WHPX"
#else
#define	NP2VER_CORE			"ver.0.86 rev.107"
#endif

// #define	NP2VER_WIN9X
// #define	NP2VER_MACOSX
// #define	NP2VER_X11
// #define	NP2VER_SDL2

