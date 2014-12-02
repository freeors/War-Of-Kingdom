/* $Id: unit_types.hpp 47388 2010-11-03 07:19:44Z silene $ */
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
#ifndef FILTER_TAG_HPP_INCLUDED
#define FILTER_TAG_HPP_INCLUDED

#include "hero.hpp"

namespace sound_filter_tag {
	enum {NONE, MALE, FEMALE, NEUTRAL};

	extern std::map<const std::string, int> tags;
	void fill_tags();
	int find(const std::string& tag);
	const std::string& rfind(int tag);
	std::string filter(const std::string& src, const std::string& f);

	int from_hero_gender(int gender);
}

#endif
