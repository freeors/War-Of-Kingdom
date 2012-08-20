/* $Id: leader_list.hpp 47655 2010-11-21 13:59:07Z mordante $ */
/*
   Copyright (C) 2007 - 2010
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License 2
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#ifndef LEADER_LIST_HPP_INCLUDED
#define LEADER_LIST_HPP_INCLUDED

namespace gui { class combo; }

#include "unit_types.hpp"

class hero_map;

#define COMBO_FEATURES_NONE			0
#define COMBO_FEATURES_RANDOM		1
#define COMBO_FEATURES_MIN_VALID	2

#define RANDOM_FORCE	-1
class force_list_manager
{
public:
	static const std::string random_enemy_picture;

	force_list_manager(int index, hero_map& heros, config& era_cfg, std::vector<std::pair<const config *, int> >& force_list,
			gui::combo* faction_combo = NULL, gui::combo* gender_combo = NULL);
	force_list_manager& operator=(const force_list_manager& that);

	void set_leader_combo(gui::combo* combo);
	void set_gender_combo(gui::combo* combo);
	void update_leader_list(int old_faction_combo_index = 0);
	void update_gender_list(const std::string& leader);
	int get_leader() const;
	std::string get_gender() const;
	void set_leader(const std::string& leader);
	void set_gender(const std::string& gender);
	void set_color(int color) {color_ = color;};
	std::string get_RC_suffix(const std::string& unit_color) const;
	void faction_changed();

private:
	void populate_leader_combo(int selected_index);

private:
	hero_map& heros_;
	int index_;
	std::vector<int> leaders_;
	std::vector<std::string> genders_;
	std::vector<std::string> gender_ids_;
	config& era_cfg_;
	std::vector<std::pair<const config *, int> >& force_list_;
	gui::combo* faction_combo_;
	gui::combo* gender_combo_;
	int color_;
};

#endif

