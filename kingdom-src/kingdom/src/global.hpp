/* $Id: global.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef DISABLE_4786_HPP_INCLUDED
#define DISABLE_4786_HPP_INCLUDED

#ifdef _MSC_VER

#undef snprintf
#define snprintf _snprintf

// Disable warnig about source encoding not in current code page.
#pragma warning(disable: 4819)

// Disable warning about deprecated functions.
#pragma warning(disable: 4996)

#endif

#ifdef _WIN32
// for memory lead detect begin
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <malloc.h>
#include <crtdbg.h>
// for memory lead detect end
#endif

#endif
