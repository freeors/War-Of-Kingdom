/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "wesnoth-editor"

#include "base_unit.hpp"

base_unit::base_unit()
	: loc_()
	, name_()
	, touch_locs_()
	, base_(false)
	, refreshing_(false)
{
}

base_unit::~base_unit()
{
}

void base_unit::set_location(const map_location &loc)
{
	if (loc_ == loc) {
		return;
	}

	// This is centor location
	loc_ = loc;

	touch_locs_.clear();
	// Centor location must be in touch locations for all unit.
	touch_locs_.insert(loc_);
}

bool base_unit::sort_compare(const base_unit& that) const
{
	return loc_.y < that.loc_.y || (loc_.y == that.loc_.y && loc_.x < that.loc_.x);
}