//#define WIN32_LEAN_AND_MEAN

// Required for Win98/ME support:
// . See: http://support.embarcadero.com/article/35754
// . "GetOpenFileName() fails under Windows 95/98/NT/ME due to incorrect OPENFILENAME structure size"
#define _WIN32_WINNT 0x0400
#define WINVER 0x500

// Mouse Wheel is not supported on Win95.
// If we didn't care about supporting Win95 (compile/run-time errors)
// we would just define the minimum windows version to support.
// #define _WIN32_WINDOWS  0x0401
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

// Not needed in VC7.1, but needed in VC Express
#include <tchar.h>

#include <crtdbg.h>
#include <dsound.h>
#include <dshow.h>

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <windows.h>
#include <winuser.h>
#include <commctrl.h>
#include <ddraw.h>
#include <htmlhelp.h>
#include <assert.h>

#include <algorithm>
#include <map>
#include <queue>
#include <stack>
#include <string>
#include <vector>
#include <memory>

#include "AppleWinX.h"
#include "resource.h"
