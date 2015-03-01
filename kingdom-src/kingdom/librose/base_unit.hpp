/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#ifndef LIBROSE_BASE_UNIT_HPP_INCLUDED
#define LIBROSE_BASE_UNIT_HPP_INCLUDED

#include "sdl_utils.hpp"
#include "map_location.hpp"
#include "tstring.hpp"

#define UNIT_NO_INDEX		-1

class base_map;

class base_unit
{
public:
	friend class base_map;

	class trefreshing_lock
	{
	public:
		trefreshing_lock(base_unit& u)
			: u_(u)
		{
			u_.refreshing_ = true;
		}
		~trefreshing_lock()
		{
			u_.refreshing_ = false;
		}

	private:
		base_unit& u_;
	};

	base_unit(base_map& units);
	virtual ~base_unit();

	virtual const t_string& name() const { return name_; }
	bool base() const { return base_; }

	virtual void set_location(const map_location &loc);
	const map_location& get_location() const { return loc_; }
	const std::set<map_location>& get_touch_locations() const { return touch_locs_; }
	const std::set<map_location>& get_draw_locations() const { return draw_locs_; }

	bool consistent() const { return !rect_.w || !rect_.h; }
	const SDL_Rect& get_rect() const { return rect_; }
	virtual void set_rect(const SDL_Rect& rect);

	int get_map_index() const { return map_index_; }
	void set_map_index(int index) { map_index_ = index; }

	virtual bool require_sort() const { return false; }
	virtual bool sort_compare(const base_unit& that) const;

	virtual void redraw_unit() {};

private:
	void calculate_draw_locs();

protected:
	map_location loc_;
	int map_index_;
	mutable t_string name_;
	std::set<map_location> touch_locs_;
	std::set<map_location> draw_locs_;
	bool base_;
	bool refreshing_;

	SDL_Rect rect_;

private:
	base_map& units_;
};

#endif
