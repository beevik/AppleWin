// Required for Win98/ME support:
// . See: http://support.embarcadero.com/article/35754
// . "GetOpenFileName() fails under Windows 95/98/NT/ME due to incorrect OPENFILENAME structure size"
#define _WIN32_WINNT 0x0400
#define WINVER 0x500

// Not needed in VC7.1, but needed in VC Express
#include <tchar.h>

#include <crtdbg.h>
#include <dsound.h>
#include <dshow.h>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>

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
