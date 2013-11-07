/* $Id: mp_create_game.hpp 48926 2011-03-17 19:41:11Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_RANDOM_MAP_HPP_INCLUDED
#define GUI_RANDOM_MAP_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "mp_game_settings.hpp"
#include "mapgen.hpp"
#include "scoped_resource.hpp"
#include "game_display.hpp"

class config;

namespace gui2 {

class tbutton;
class tlabel;
class ttext_box;

class trandom_map
{
public:

	explicit trandom_map(const config& cfg, int mode = RPG_MODE);

	mp_game_settings& get_parameters();
	const config& scenario_data() const;

protected:
	void pre_show(twindow& window);

	// another map selected
	void update_map(twindow& window);

	void regenerate_map(twindow& window);
	void generator_settings(twindow& window);

	virtual game_display& gui() = 0;
	virtual void update_map_settings(twindow& window) = 0;
	virtual void post_update_map(twindow& window, int select) {};
	void generate_map(twindow& window);

protected:
	const config& cfg_;
	int mode_;
	int load_game_index_;
	util::scoped_ptr<map_generator> generator_;
	std::vector<int> candidate_;

	mp_game_settings parameters_;

	tbutton* generator_settings_;
	tbutton* regenerate_map_;

	tlabel* num_players_label_;
	tlabel* map_size_label_;
};

} // namespace gui2

#endif

