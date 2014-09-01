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

#ifndef GUI_DIALOGS_EXILE_HPP_INCLUDED
#define GUI_DIALOGS_EXILE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <hero.hpp>
#include <set>

class game_display;
class unit_type;

namespace gui2 {

class tlistbox;

class texile : public tdialog
{
public:
	explicit texile(game_display& disp, hero_map& heros, tgroup& g);

	bool dirty() const { return dirty_; }
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);
	
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void fill_exile_table(twindow& window, int cursel = 0);
	void set_2button_active(twindow& window);

	void refresh_title_flag(twindow& window) const;
	void type_selected(twindow& window);
	void join(twindow& window);
	void discard(twindow& window);
	void detail(twindow& window);
private:
	game_display& disp_;
	hero_map& heros_;
	tgroup& group_;
	int selected_number_;
	bool dirty_;
};

}

#endif

