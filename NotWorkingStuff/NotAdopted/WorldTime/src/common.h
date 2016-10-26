#ifndef _COMMON_H
#define _COMMON_H

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define VC_EXTRALEAN

#define _WIN32_WINNT 0x0400
#define MIRANDA_VER  0x0700

#define  _CRT_SECURE_NO_WARNINGS
#include <tchar.h>
#include <windows.h>
#include <m_core.h>
#include <commdlg.h>

#define PROTO	"WorldTime"

//#include <win2k.h>
#include <newpluginapi.h>
#include <statusmodes.h>
#include <m_options.h>
#include <m_langpack.h>
//#include <m_system.h>
//#include <m_system_cpp.h>
#include <m_database.h>
#include <m_protocols.h>
#include <m_protomod.h>
#include <m_protosvc.h>
#include <m_ignore.h>
#include <m_clist.h>
#include <m_clui.h>
#include <m_utils.h>
//#include <m_pluginupdater.h>

extern HINSTANCE hInst;

#ifndef MIID_WORLDTIME
#define MIID_WORLDTIME		{0x5e436914, 0x3258, 0x4dfa, { 0x8a, 0xd9, 0xf2, 0xcd, 0x26, 0x43, 0x6f, 0x1}}
#endif

#endif
