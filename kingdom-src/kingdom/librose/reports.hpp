/* $Id: reports.hpp 47614 2010-11-21 13:56:59Z mordante $ */
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

#ifndef REPORTS_HPP_INCLUDED
#define REPORTS_HPP_INCLUDED

#include "image.hpp"
#include "sdl_utils.hpp"

//this module is responsible for outputting textual reports of
//various game and unit statistics
namespace reports {

struct report {
	enum ttype {NONE, LABEL, SURFACE};

	report()
		: type(NONE)
		, text()
		, surf()
		, tooltip()
		, rect(empty_rect)
	{}
	report(ttype type, const std::string& text, const std::string& tooltip)
		: type(type)
		, text(text)
		, surf()
		, tooltip(tooltip)
		, rect(empty_rect)
	{}
	
	bool valid() const { return type != NONE; }
	bool operator==(const report& o) const 
	{
	 	return o.type == type && o.text == text && o.surf.get() == surf.get() && o.tooltip == tooltip;
	}

	bool operator!=(const report& o) const { return !(o == *this); }

	ttype type;
	std::string text;
	surface surf;
	std::string tooltip;
	SDL_Rect rect;
};

}

#endif
