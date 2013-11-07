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

#ifndef GUI_DIALOGS_TROOP_DETAIL_HPP_INCLUDED
#define GUI_DIALOGS_TROOP_DETAIL_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>

class game_display;
class team;
class unit_map;
class hero_map;
class hero;
class unit;


namespace gui2 {

class tlistbox;

class ttroop_detail : public tdialog
{
public:
	explicit ttroop_detail(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, std::vector<const unit*>& partial_troops, const std::string& title);

	int troop_index() const { return troop_index_; }

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void type_selected(twindow& window);
	void refresh_tooltip(twindow& window);

	void hero_list(twindow& window);
	void merit(twindow& window);
private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;

	std::vector<const unit*>& partial_troops_;
	std::string title_;
	int troop_index_;

	tlistbox* hero_table_;
};

}

#endif

