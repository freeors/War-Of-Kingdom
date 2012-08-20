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

#ifndef GUI_DIALOGS_LIST_HPP_INCLUDED
#define GUI_DIALOGS_LIST_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

class game_display;
class team;
class unit_map;
class hero_map;
class game_state;

namespace gui2 {

class tlist : public tdialog
{
public:
	explicit tlist(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, int side);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void objectives(twindow& window, bool global);
	void strategy_list(twindow& window, bool global);
	void side(twindow& window, bool global);
	void troop(twindow& window, bool global);
	void city(twindow& window, bool global);
	void artifical_list(twindow& window, bool global);
	void hero_list(twindow& window, bool global);

private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	team& current_team_;
	game_state& gamestate_;
};


}


#endif /* ! GUI_DIALOGS_MENU_HPP_INCLUDED */
