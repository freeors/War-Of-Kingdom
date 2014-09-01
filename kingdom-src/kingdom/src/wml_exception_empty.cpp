/* $Id: wml_exception.cpp 54233 2012-05-19 19:35:44Z mordante $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Implementation for wml_exception.hpp.
 */
#ifndef ANDROID

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"
#include "wml_exception.hpp"

#include "gettext.hpp"
#include "serialization/string_utils.hpp"
#include "filesystem.hpp"
#include "posix.h"

void wml_exception(
		  const char* cond
		, const char* file
		, const int line
		, const char *function
		, const std::string& message
		, const std::string& dev_message)
{
	std::ostringstream sstr;
	if(cond) {
		sstr << "Condition '" << cond << "' failed at ";
	} else {
		sstr << "Unconditional failure at ";
	}

	sstr << file << ":" << line << " in function '" << function << "'.";

	if(!dev_message.empty()) {
		sstr << " Extra development information: " << dev_message;
	}

	std::ostringstream err;
	err << _("An error due to possibly invalid WML occurred\nThe error message is :")
		<< "\n" << sstr.str() << "\n \n"
		<< _("When reporting the bug please include the following error message :")
		<< "\n" << dev_message;
	return posix_print_mb(utf8_2_ansi(err.str().c_str()));
}

#endif
