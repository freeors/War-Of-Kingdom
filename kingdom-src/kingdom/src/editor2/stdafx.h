#ifndef __STDAFX_H_
#define __STDAFX_H_

#include <limits.h>
#include <io.h>

// for memory lead detect begin
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <malloc.h>
#include <crtdbg.h>
// for memory lead detect end

#define PROG_NAME			"SLG Maker"
#include "posix.h"

#include <commctrl.h>
#include <time.h>

#include <iosfwd>
#include <locale>
#include <algorithm>
#include <cctype>
#include <iomanip>

#endif // __STDAFX_H_