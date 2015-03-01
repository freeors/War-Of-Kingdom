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

#ifndef STUDIO_UNIT_MAP_HPP_INCLUDED
#define STUDIO_UNIT_MAP_HPP_INCLUDED

#include "unit2.hpp"
#include "base_map.hpp"

class mkwin_controller;

class unit_map: public base_map
{
public:
	unit_map(mkwin_controller& controller, const gamemap& gmap, bool consistent);

	void create_coor_map(int w, int h);
	void add(const map_location& loc, const base_unit* base_u);

	unit* find_unit(const map_location& loc) const;
	unit* find_unit(const map_location& loc, bool verlay) const;

	unit* find_unit(int i) const { return dynamic_cast<unit*>(map_[i]); }

	void move_mass(bool horizontal, bool extend, int start, int stop);
	void save_map_to(unit::tchild& child, bool clear);
	void restore_map_from(const unit::tchild& child);

	void move_line(bool horizontal, const int index);
	void swap_line(bool horizontal, const int index);

	bool line_is_spacer(bool row, int index) const;

private:
	mkwin_controller& controller_;
};

#endif
