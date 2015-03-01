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

#include "base_map.hpp"
#include "display.hpp"
#include "controller_base.hpp"

base_unit::base_unit(base_map& units)
	: units_(units)
	, loc_()
	, map_index_(UNIT_NO_INDEX)
	, name_()
	, touch_locs_()
	, draw_locs_()
	, base_(false)
	, refreshing_(false)
	, rect_(empty_rect)
{
}

base_unit::~base_unit()
{
}

void base_unit::calculate_draw_locs()
{
	display& disp = units_.get_controller().get_display();
	if (!draw_locs_.empty()) {
		disp.invalidate(draw_locs_);
		draw_locs_.clear();
	}
	
	if (units_.consistent()) {
		draw_locs_.insert(loc_);
	} else if (rect_.w > 0 && rect_.h > 0) {
		map_location lt = disp.pixel_position_to_hex(rect_.x, rect_.y);
		map_location rb = disp.pixel_position_to_hex(rect_.x + rect_.w - 1, rect_.y + rect_.h - 1);
		for (int y = lt.y; y <= rb.y; y ++) {
			for (int x = lt.x; x <= rb.x; x ++) {
				draw_locs_.insert(map_location(x, y));
			}
		}

	}
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

	calculate_draw_locs();
}

void base_unit::set_rect(const SDL_Rect& rect)
{
	VALIDATE(rect.w && rect.h, "rect must not be empty!");

	if (rect == rect_) {
		return;
	}
	rect_ = rect;
	if (loc_.valid()) {
		units_.extract(loc_);
		map_location loc = units_.conflict_calculate_loc(*this);
		map_location original = loc_;
		units_.place(loc, this);
		if (loc_ == original) {
			calculate_draw_locs();
		}
	}
}

bool base_unit::sort_compare(const base_unit& that) const
{
	return loc_.y < that.loc_.y || (loc_.y == that.loc_.y && loc_.x < that.loc_.x);
}