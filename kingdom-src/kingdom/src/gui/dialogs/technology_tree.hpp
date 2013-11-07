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

#ifndef GUI_DIALOGS_TECHNOLOGY_TREE_HPP_INCLUDED
#define GUI_DIALOGS_TECHNOLOGY_TREE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>
#include "unit_types.hpp"

class game_display;
class team;
class unit_map;
class hero_map;

namespace gui2 {

class ttree_view_node;
class twindow;
class ttoggle_button;
class ttree_view;

class ttechnology_tree : public tdialog
{
public:
	explicit ttechnology_tree(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, int side, bool browse);

	const ttechnology* ing_technology() const { return ing_technology_; }
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void signal_handler_mouse_enter(ttoggle_button& widget);
	void signal_handler_mouse_leave();

	void technology_toggled(twidget* widget);
	void technology_tree_2_tv_internal(ttree_view_node* htvi, const std::vector<advance_tree::node>& advances_to, const std::vector<std::string>& advances_from2, bool base_holded);

	void show_tip(const ttechnology& t);
	std::string exp_str(const ttechnology& t);
private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	bool browse_;

	team& current_team_;
	std::vector<std::pair<std::string, std::vector<std::string> > > technology_tv_;
	twindow* window_;
	ttree_view* tree_;
	const ttechnology* ing_technology_;
};

}

#endif

