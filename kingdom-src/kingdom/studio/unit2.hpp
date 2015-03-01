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

#ifndef STUDIO_UNIT2_HPP_INCLUDED
#define STUDIO_UNIT2_HPP_INCLUDED

#include "unit.hpp"

class unit2: public unit
{
public:
	
	unit2(mkwin_controller& controller, mkwin_display& disp, unit_map& units, const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget, unit* parent, const SDL_Rect& rect);
	unit2(mkwin_controller& controller, mkwin_display& disp, unit_map& units, int type, unit* parent, const SDL_Rect& rect);
	unit2(const unit2& that);

private:
	bool sort_compare(const base_unit& that) const;

	void generate_window(config& cfg) const;
	void redraw_unit();

	void from_window(const config& cfg);
};

#endif
