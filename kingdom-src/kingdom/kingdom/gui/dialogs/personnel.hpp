/* $Id: player_selection.hpp 49605 2011-05-22 17:56:24Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_PERSONNEL_HPP_INCLUDED
#define GUI_DIALOGS_PERSONNEL_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <hero.hpp>
#include <set>

class display;
class unit_type;

namespace gui2 {

class tlistbox;

class tpersonnel : public tdialog
{
public:
	explicit tpersonnel(display& disp, hero_map& heros);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void fill_member_table(twindow& window, int cursel);
	void set_3button_active(twindow& window);

	void type_selected(twindow& window, tlistbox& list, const int type = twidget::drag_none);
	void up(twindow& window);
	void down(twindow& window);
	void save(twindow& window);
private:
	display& disp_;
	hero_map& heros_;

	std::vector<int> original_order_;
	std::vector<int> order_; 
	int cursel_;
};

}

#endif

