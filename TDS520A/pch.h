// pch.h - Precompiled Header
// All heavy, rarely-changed system headers go here.
#pragma once

// Windows version targeting - support Windows 7 x64 and later
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000  // NTDDI_WIN7
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VC_EXTRA_LEAN

#include <afxwin.h>       // MFC core and standard components
#include <afxext.h>       // MFC extensions
#include <afxdlgs.h>      // MFC standard dialogs
#include <afxcmn.h>       // MFC common controls (CListCtrl, CSpinButtonCtrl, CProgressCtrl...)
#include <afxmt.h>        // MFC synchronization objects
#include <afxtempl.h>     // MFC template classes

// Windows SDK
#include <windows.h>
#include <windowsx.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

// C++ Standard Library
#include <string>
#include <vector>
#include <array>
#include <deque>
#include <queue>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cassert>
#include <cmath>

// NI-488.2 header (from NI-VISA / NI-488.2 installation)
// Default path: C:\Program Files\National Instruments\Shared\ExternalCompilerSupport\C\include
#include <Decl-32.h>   // NI-488.2 declarations (works for both 32/64-bit)

// Pragma comment libs - also set in .vcxproj but belt-and-suspenders
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// Suppress specific warnings
#pragma warning(disable: 4458)  // declaration hides class member (common in MFC)
#pragma warning(disable: 4100)  // unreferenced formal parameter
