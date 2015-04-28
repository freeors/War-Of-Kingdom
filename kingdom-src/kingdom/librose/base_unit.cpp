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
#define GETTEXT_DOMAIN "rose-lib"

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
	, redraw_counter_(0)
	, rect_(empty_rect)
	, hidden_(false)
	, anim_(NULL)
	, next_idling_(0)
	, frame_begin_time_(0)
	, draw_bars_(false)
	, facing_(map_location::SOUTH_EAST)
	, state_(STATE_STANDING)
{
}

base_unit::base_unit(const base_unit& that)
	: units_(that.units_)
	, loc_(that.loc_)
	, map_index_(that.map_index_)
	, name_(that.name_)
	, touch_locs_(that.touch_locs_)
	, draw_locs_(that.draw_locs_)
	, base_(that.base_)
	, refreshing_(that.refreshing_)
	, redraw_counter_(that.redraw_counter_)
	, rect_(that.rect_)
	, hidden_(that.hidden_)
	, anim_(NULL) // important!!
	, next_idling_(that.next_idling_)
	, frame_begin_time_(that.frame_begin_time_)
	, draw_bars_(that.draw_bars_)
	, facing_(that.facing_)
	, state_(that.state_)
{
}

base_unit::~base_unit()
{
	if (anim_) {
		delete anim_;
		anim_ = NULL;
	}
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

void base_unit::set_hidden(bool val)
{
	if (val && val != hidden_) {
		if (!draw_locs_.empty()) {
			display& disp = units_.get_controller().get_display();
			disp.invalidate(draw_locs_);
		}
	}
	hidden_ = val; 
}

bool base_unit::sort_compare(const base_unit& that) const
{
	return loc_.y < that.loc_.y || (loc_.y == that.loc_.y && loc_.x < that.loc_.x);
}

animation* base_unit::create_animation(const animation& anim)
{
	return new animation(anim);
}

void base_unit::start_animation(int start_time, const animation* anim,
	bool with_bars, bool cycles, const std::string &text, Uint32 text_color, STATE state)
{
	display& disp = units_.get_controller().get_display();
	state_ = state;
	if (!anim) {
		if (state != STATE_STANDING) {
			set_standing(with_bars);
		}
		return ;
	}
	// everything except standing select and idle
	bool accelerate = (state != STATE_FORGET && state != STATE_STANDING);
	draw_bars_ =  with_bars;
	
	delete anim_;
	anim_ = create_animation(*anim);
	
	const int real_start_time = start_time == INT_MAX ? anim_->get_begin_time() : start_time;
	anim_->start_animation(real_start_time, loc_, loc_.get_direction(facing_),
		cycles, text, text_color, accelerate);
	frame_begin_time_ = anim_->get_begin_time() -1;

	// idle anim
	next_idling_ = calculate_next_idling();
}

int base_unit::calculate_next_idling() const
{
	return get_current_animation_tick() + static_cast<int>(2000 + rand() % 3000);
}

void base_unit::set_standing(bool with_bars)
{
}

void base_unit::set_facing(map_location::DIRECTION dir) 
{
	if (dir != map_location::NDIRECTIONS) {
		facing_ = dir;
	}
}