/* $Id: editor_palettes.cpp 47295 2010-10-30 09:13:32Z silene $ */
/*
  Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
  Part of the Battle for Wesnoth Project http://www.wesnoth.org/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY.

  See the COPYING file for more details.
*/

/**
 * Manage the terrain-palette in the editor.
 * Note: this is a near-straight rip from the old editor.
*/

#define GETTEXT_DOMAIN "kingdom-lib"

#include "editor_common.hpp"
#include "editor_palettes.hpp"

#include "gettext.hpp"
#include "serialization/string_utils.hpp"
#include "sound.hpp"
#include "marked-up_text.hpp"

#include <boost/foreach.hpp>

namespace editor {

static bool is_valid_terrain(t_translation::t_terrain c) {
	return !(c == t_translation::VOID_TERRAIN || c == t_translation::FOGGED);
}

terrain_group::terrain_group(const config& cfg, display& gui):
	id(cfg["id"]), 
	icon(cfg["icon"].str()),
	name(cfg["name"].t_str()),
	core(cfg["core"].to_bool())
{
}

terrain_palette::terrain_palette(display &gui,
								 const config& cfg,
								 t_translation::t_terrain& fore,
								 t_translation::t_terrain& back)
	: gui_(gui)
	, tstart_(0)
	, terrain_map_()
	, terrains_()
	, current_group_id_()
	, terrain_groups_()
	, non_core_terrains_()
	, nterrains_()
	, terrain_start_()
	, selected_fg_terrain_(fore)
	, selected_bg_terrain_(back)
{
	// Get the available terrains temporary in terrains_
	terrains_ = map().get_terrain_list();

	//move "invalid" terrains to the end
	std::stable_partition(terrains_.begin(), terrains_.end(), is_valid_terrain);

	// Get the available groups and add them to the structure
	std::set<std::string> group_names;
	BOOST_FOREACH (const config &group, cfg.child_range("editor_group"))
	{
		if (group_names.find(group["id"]) == group_names.end()) {

			config cfg;
			cfg["id"] = group["id"];
			cfg["name"] = group["name"];

			cfg["icon"] = "buttons/terrain_" + group["icon"].str() + "_30-pressed.png";
			cfg["core"] = group["core"];

			terrain_groups_.push_back(terrain_group(cfg, gui));
			group_names.insert(terrain_groups_.back().id);
			// By default the 'all'-button is pressed
		}
	}
	
	std::map<std::string, terrain_group*> id_to_group;
	BOOST_FOREACH (terrain_group& tg, terrain_groups_) {
		id_to_group.insert(std::make_pair(tg.id, &tg));
	}

	// add the groups for all terrains to the map
	BOOST_FOREACH (const t_translation::t_terrain& t, terrains_) {
		const terrain_type& t_info = map().get_terrain_info(t);
		DBG_ED << "Palette: processing terrain " << t_info.name()
			<< "(editor name: '" << t_info.editor_name() << "') "
			<< "(" << t_info.number() << ")"
			<< ": " << t_info.editor_group() << "\n";

		// don't display terrains that were automatically created from base+overlay
		if (t_info.is_combined()) continue;
		// nor display terrains that have hide_in_editor=true
		if (t_info.hide_in_editor()) continue;

		// add the terrain to the requested groups
		const std::vector<std::string>& keys = utils::split(t_info.editor_group());
		bool core = false;
		BOOST_FOREACH (const std::string& k, keys) {
			terrain_map_[k].push_back(t);
			std::map<std::string, terrain_group*>::iterator i = id_to_group.find(k);
			if (i != id_to_group.end()) {
				if (i->second->core) {
					core = true;
				}
			}
		}
		// A terrain is considered core iff it appears in at least
		// one core terrain group
		if (core) {
			// Add the terrain to the default group
			terrain_map_["all"].push_back(t);
		} else {
			non_core_terrains_.insert(t);
		}

	}
	typedef std::pair<std::string, t_translation::t_list> map_pair;

	// Set the default group
	terrains_ = terrain_map_["all"];

	if(terrains_.empty()) {
		ERR_ED << "No terrain found.\n";
	}
	update_report();
}

void terrain_palette::set_group(const std::string& id)
{
	terrains_ = terrain_map_[id];
	if(terrains_.empty()) {
		ERR_ED << "No terrain found.\n";
	}
	current_group_id_ = id;
}

t_translation::t_terrain terrain_palette::selected_fg_terrain() const
{
	return selected_fg_terrain_;
}

t_translation::t_terrain terrain_palette::selected_bg_terrain() const
{
	return selected_bg_terrain_;
}

void terrain_palette::select_fg_terrain(t_translation::t_terrain terrain)
{
	if (selected_fg_terrain_ != terrain) {
		selected_fg_terrain_ = terrain;
		update_report();
	}
}

void terrain_palette::select_bg_terrain(t_translation::t_terrain terrain)
{
	if (selected_bg_terrain_ != terrain) {
		selected_bg_terrain_ = terrain;
		update_report();
	}
}

void terrain_palette::swap()
{
	std::swap(selected_fg_terrain_, selected_bg_terrain_);
	update_report();
}


std::string terrain_palette::get_terrain_string(const t_translation::t_terrain t)
{
	std::stringstream str;
	const std::string& name = map().get_terrain_info(t).editor_name();

	const t_translation::t_list& underlying = map().underlying_union_terrain(t);
	str << name;
	if(underlying.size() != 1 || underlying[0] != t) {
		str << " (";
		for(t_translation::t_list::const_iterator i = underlying.begin();
			i != underlying.end(); ++i) {

			str << map().get_terrain_info(*i).editor_name();
			if(i+1 != underlying.end()) {
				str << ",";
			}
		}
		str << ")";
	}
	return str.str();
}

void terrain_palette::click_terrain(int tselect)
{
	select_fg_terrain(terrains_[tselect]);
	gui_.invalidate_game_status();
}

size_t terrain_palette::num_terrains() const {
	return terrains_.size();
}

std::string selected_terrain;

void terrain_palette::update_report()
{
	std::ostringstream msg;
	msg << _("FG: ") << get_terrain_string(selected_fg_terrain())
		<< '\n' << _("BG: ") << get_terrain_string(selected_bg_terrain());
	selected_terrain = msg.str();
}

} // end namespace editor

