/* $Id: multiplayer.hpp 47847 2010-12-05 21:12:23Z shadowmaster $ */
/*
   Copyright (C) 2005 - 2010 Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef LIBROSE_ICHAT_HPP_INCLUDED
#define LIBROSE_ICHAT_HPP_INCLUDED

#include <time.h>
#include <string.h>

#ifdef _WIN32
#ifndef strncasecmp
#define strncasecmp(s1, s2, c)		_strnicmp(s1, s2, c)
#endif

#ifndef strcasecmp
#define strcasecmp(s1, s2)			_stricmp(s1, s2)
#endif
#endif

namespace irc {
struct session;
}

class ichat
{
public:
	virtual void tcp_send_len(char* buf, int len) = 0;
	virtual void handle_command(const char* word[]) = 0;
};


#endif
