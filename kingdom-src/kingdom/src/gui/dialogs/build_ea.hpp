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

#ifndef GUI_DIALOGS_BUILD_EA_HPP_INCLUDED
#define GUI_DIALOGS_BUILD_EA_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "map_location.hpp"
#include <vector>

class game_display;
class team;
class unit_map;
class hero_map;
class gamemap;
class unit_type;
class artifical;

namespace gui2 {

class tbuild_ea : public tdialog
{
public:
	explicit tbuild_ea(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, const gamemap& map, team& current_team, int cost_exponent, const map_location& loc);

	const unit_type* get_selected_ut() const;
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void set_retval(twindow& window, int val);

	void tactic_selected(twindow& window);
private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	const gamemap& map_;
	team& current_team_;
	int cost_exponent_;
	const map_location loc_;
	artifical* ownership_;

	std::vector<const unit_type*> can_build_ea_;
	std::vector<bool> valid_;
	const unit_type* selected_ut_;
};


}


#endif /* ! GUI_DIALOGS_MENU_HPP_INCLUDED */
