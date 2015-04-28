/* $Id: gettext.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

#include "global.hpp"
#include "gettext.hpp"

#include <cstring>

char def_textdomain[MAXLEN_TEXTDOMAIN + 1];

const char* dsgettext (const char * domainname, const char *msgid)
{
	const char *msgval = dgettext (domainname, msgid);
	if (msgval == msgid) {
		msgval = std::strrchr (msgid, '^');
		if (msgval == NULL)
			msgval = msgid;
		else
			msgval++;
	}
	return msgval;
}

const char* sngettext (const char *singular, const char *plural, int n)
{
	const char *msgval = ngettext (singular, plural, n);
	if (msgval == singular) {
		msgval = std::strrchr (singular, '^');
		if (msgval == NULL)
			msgval = singular;
		else
			msgval++;
	}
	return msgval;
}

const char* dsngettext (const char * domainname, const char *singular, const char *plural, int n)
{
	const char *msgval = dngettext (domainname, singular, plural, n);
	if (msgval == singular) {
		msgval = std::strrchr (singular, '^');
		if (msgval == NULL)
			msgval = singular;
		else
			msgval++;
	}
	return msgval;
}
