/* $Id: unit_types.cpp 47388 2010-11-03 07:19:44Z silene $ */
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

/**
 *  @file
 *  Handle unit-type specific attributes, animations, advancement.
 */

#include "global.hpp"

#include "filter_tag.hpp"

#include "wml_exception.hpp"
#include "filesystem.hpp"
#include "serialization/string_utils.hpp"

namespace sound_filter_tag {
std::map<const std::string, int> tags;

void fill_tags()
{
	if (!tags.empty()) return;

	tags.insert(std::make_pair("male", (int)MALE));
	tags.insert(std::make_pair("female", (int)FEMALE));
	tags.insert(std::make_pair("neutral", (int)NEUTRAL));
}

int find(const std::string& tag) 
{
	std::map<const std::string, int>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(int tag)
{
	for (std::map<const std::string, int>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

int from_hero_gender(int gender)
{
	if (gender == hero_gender_male) {
		return MALE;
	} else if (gender == hero_gender_female) {
		return FEMALE;
	}
	return NEUTRAL;
}

std::string filter(const std::string& src, const std::string& f)
{
	if (f.empty()) {
		return src;
	}

	std::stringstream result;
	const std::vector<std::string> vstr = utils::split(src);
	bool first = true;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		size_t pos = it->find(":");
		if (pos == std::string::npos) {
			if (!first) {
				result << ",";
			} else {
				first = false;
			}
			result << *it;
		} else {
			if (it->substr(0, pos) == f && it->size() > pos + 1) {
				if (!first) {
					result << ",";
				} else {
					first = false;
				}
				std::string n = it->substr(pos + 1);
				result << it->substr(pos + 1);
			}
		}
	}
	return result.str();
}

}
