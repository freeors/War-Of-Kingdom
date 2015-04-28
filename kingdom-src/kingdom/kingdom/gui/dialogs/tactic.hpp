/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_TACTIC_HPP_INCLUDED
#define GUI_DIALOGS_TACTIC_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

class game_display;
class team;
class unit_map;
class hero_map;
class unit;
class hero;

namespace gui2 {

class tlistbox;

class ttactic2 : public tdialog
{
public:
	enum {CAST, ACTIVE, BROWSE};

	explicit ttactic2(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, unit& tactician, int operate);

	hero* get_selected_hero() const;
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void set_retval(twindow& window, int val);

	void tactic_selected(twindow& window, tlistbox& list, const int type = twidget::drag_none);
private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;

	unit& tactician_;
	int operate_;
	std::vector<hero*> candidate_;
	std::vector<bool> valid_;
	bool cannot_valid_;
	int selected_;
};


}


#endif /* ! GUI_DIALOGS_MENU_HPP_INCLUDED */
