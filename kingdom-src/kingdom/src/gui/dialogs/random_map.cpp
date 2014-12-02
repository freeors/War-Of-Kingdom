/* $Id: mp_create_game.cpp 49597 2011-05-22 17:55:49Z mordante $ */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/random_map.hpp"

#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/integer_selector.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "../../settings.hpp"
#include "filesystem.hpp"
#include "map.hpp"
#include "map_exception.hpp"
#include "map_create.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/mp_create_game_set_password.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "formula_string_utils.hpp"
#include <boost/bind.hpp>

namespace gui2 {

trandom_map::trandom_map(const config& cfg, int mode, bool local_only) :
	cfg_(cfg)
	, mode_(mode)
	, local_only_(local_only)
	, load_game_index_(-1)
	, siege_mode_index_(-1)
	, generator_(NULL)
	, parameters_()
	, generator_settings_(NULL)
	, regenerate_map_(NULL)
{
}

void trandom_map::pre_show(twindow& window)
{
	find_widget<tminimap>(&window, "minimap", false).set_config(&cfg_);

	num_players_label_ = find_widget<tlabel>(&window, "map_players", false, true);
	map_size_label_ = find_widget<tlabel>(&window, "map_size", false, true);

	regenerate_map_ = find_widget<tbutton>(&window, "regenerate", false, true);
	generator_settings_ = find_widget<tbutton>(&window, "settings", false, true);
	
	tlistbox& list = find_widget<tlistbox>(&window, "map_list", false);
#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(list, boost::bind(
				  &tmp_create_game::update_map
				, *this
				, boost::ref(window)));
#else
	list.set_callback_value_change(
			dialog_callback<trandom_map, &trandom_map::update_map>);
#endif

	// Load option (might turn it into a button later).
	string_map item;
	if (mode_ != mode_tag::TOWER) {
		item.insert(std::make_pair("label", tintegrate::generate_format(_("Load Game"), "yellow")));
		item.insert(std::make_pair("tooltip", _("Load Game...")));
		list.add_row(item);

		tgrid* grid = list.get_row_grid(list.get_item_count() - 1);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid->find("_toggle", true));
		toggle->set_data(LOAD_GAME);

		if (!local_only_) {
			item.clear();
			item.insert(std::make_pair("label", tintegrate::generate_format(dsgettext("wesnoth-multiplayer", "2p_siege_map"), "green")));
			item.insert(std::make_pair("tooltip", _("Siege map")));
			list.add_row(item);

			grid = list.get_row_grid(list.get_item_count() - 1);
			toggle = dynamic_cast<ttoggle_button*>(grid->find("_toggle", true));
			toggle->set_data(SIEGE_MODE);
		}
	}

	// Standard maps
	int index = 0;
	BOOST_FOREACH (const config &map, cfg_.child_range("multiplayer"))
	{
		index ++;
		const std::string& mode = map["mode"].str();
		if (mode_ == mode_tag::TOWER) {
			// avoid scrub score, don't diplay fix map
			if (!map.child_count("generator")) {
				continue;
			}
		} else if (!mode.empty()) {
			continue;
		}
		if (map["allow_new_game"].to_bool(true)) {
			string_map item;
			item.insert(std::make_pair("label", map["name"].str()));
			item.insert(std::make_pair("tooltip", map["name"].str()));
			list.add_row(item);

			tgrid* grid = list.get_row_grid(list.get_item_count() - 1);
			ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid->find("_toggle", true));
			toggle->set_data(BASE + index - 1);
		}
	}

	connect_signal_mouse_left_click(
		*regenerate_map_
		, boost::bind(
			&trandom_map::regenerate_map
			, this
			, boost::ref(window)));
	connect_signal_mouse_left_click(
		*generator_settings_
		, boost::bind(
			&trandom_map::generator_settings
			, this
			, boost::ref(window)));

	update_map(window);

}

void trandom_map::update_map(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "map_list", false);
	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	int select = dynamic_cast<ttoggle_button*>(grid_ptr->find("_toggle", true))->get_data();

	generator_.assign(NULL);

	parameters_.saved_game = false;
	parameters_.siege_mode = false;
	if (select == LOAD_GAME) {
		parameters_.scenario_data.clear();
		parameters_.saved_game = true;

	} else if (select == SIEGE_MODE) {
		parameters_.scenario_data.clear();
		parameters_.siege_mode = true;

	} else {
		// multiplayer scenario
		int index = select - BASE;

		config::const_child_itors levels = cfg_.child_range("multiplayer");
		std::advance(levels.first, index);

		if (levels.first != levels.second) {
			const config &level = *levels.first;
			parameters_.scenario_data = level;
			std::string map_data = level["map_data"];

			if (map_data.empty() && !level["map"].empty()) {
				map_data = read_map(level["map"]);
			}

			// If the map should be randomly generated.
			if (!level["map_generation"].empty()) {
				config generator_cfg = level.child("generator");
				if (mode_ == mode_tag::TOWER) {
					generator_cfg["map_width"] = 16;
					generator_cfg["map_height"] = 6;
					generator_cfg["hill_size"] = 2;
					config& castle_config = generator_cfg.child("castle");
					if (castle_config) {
						castle_config["min_distance"] = 3;
						// add shallow water(Ww), deep water(Wo) if necessary
						// valid_terrain=Gg, Gs^Fp, Hh
						std::stringstream adjusted;
						adjusted << castle_config["valid_terrain"].str();
						const std::vector<std::string>& vstr = utils::split(adjusted.str());
						if (std::find(vstr.begin(), vstr.end(), "Ww") == vstr.end()) {
							adjusted << ", Ww";
						}
						if (std::find(vstr.begin(), vstr.end(), "Wo") == vstr.end()) {
							adjusted << ", Wo";
						}
						castle_config["valid_terrain"] = adjusted.str();
					}
					generator_cfg["no_ea"] = true;
				}
				generator_.assign(create_map_generator(level["map_generation"], generator_cfg));
			}


		}
	}

	generate_map(window);

	post_update_map(window, select);
}

void trandom_map::generate_map(twindow& window)
{
	tminimap& minimap = find_widget<tminimap>(&window, "minimap", false);

	if (generator_ != NULL) {
		const cursor::setter cursor_setter(cursor::WAIT);

		// Generate the random map
		cursor::setter cur(cursor::WAIT);
		parameters_.scenario_data = generator_->create_scenario(std::vector<std::string>());

		if (!parameters_.scenario_data["error_message"].empty())
			gui2::show_message(gui().video(), "map generation error", parameters_.scenario_data["error_message"]);

		// Set the scenario to have placing of sides
		// based on the terrain they prefer
		parameters_.scenario_data["modify_placing"] = true;
	}

	generator_settings_->set_visible(generator_? twidget::VISIBLE: twidget::INVISIBLE);
	regenerate_map_->set_visible(generator_? twidget::VISIBLE: twidget::INVISIBLE);

	minimap.set_map_data(tminimap::TILE_MAP, parameters_.scenario_data["map_data"]);

	const std::string& map_data = parameters_.scenario_data["map_data"];

	util::unique_ptr<gamemap> map;
	try {
		map.reset(new gamemap(cfg_, map_data));
	} catch (incorrect_map_format_error& e) {
		std::stringstream err;
		err << "map could not be loaded: " << e.message;
		gui2::show_message(gui().video(), "error", err.str());

	} catch(twml_exception& e) {
		std::stringstream err;
		err <<  "map could not be loaded: " << e.dev_message;
		gui2::show_message(gui().video(), "error", err.str());
	}

	// launch_game_->set_active(map.get() != NULL);
	
	// If there are less sides in the configuration than there are
	// starting positions, then generate the additional sides
	const int map_positions = map.get() != NULL ? map->num_valid_starting_positions() : 0;

	for (int pos = parameters_.scenario_data.child_count("side"); pos < map_positions; ++pos) {
		config& side = parameters_.scenario_data.add_child("side");
		side["side"] = pos + 1;
		side["team_name"] = pos + 1;
		side["controller"] = "human";

		std::stringstream str;
		str << "economy_area" << pos + 1;
		side["economy_area"] = parameters_.scenario_data[str.str()];
		parameters_.scenario_data.remove_attribute(str.str());
	}

	int nsides = 0;
	BOOST_FOREACH (const config &k, parameters_.scenario_data.child_range("side")) {
		if (k["allow_player"].to_bool(true)) ++nsides;
	}

	std::stringstream players;
	std::stringstream map_size;
	if (map.get() != NULL) {
		players << _("Players") << " " << nsides;
		map_size << _("Size") << " " << map.get()->w() << "x" << map.get()->h();
	} else {
		players << _("Error");
		map_size << "";
	}
	num_players_label_->set_label(players.str());
	map_size_label_->set_label(map_size.str());

	update_map_settings(window);
}

void trandom_map::regenerate_map(twindow& window)
{
	generate_map(window);
}

void trandom_map::generator_settings(twindow& window)
{
	int max_players = -1, min_w = -1, max_w = -1, min_h = -1, max_h = -1;
	if (mode_ == mode_tag::TOWER) {
		max_players = 2;
		min_w = 16;
		max_w = 20;
		min_h = 6;
		max_h = 10;
	}
	bool changed = generator_->user_config(gui(), cfg_, max_players, min_w, max_w, min_h, max_h);
	if (changed) {
		generate_map(window);
	}
}

mp_game_settings& trandom_map::get_parameters()
{
	return parameters_;
}

const config& trandom_map::scenario_data() const
{
	return parameters_.scenario_data;
}

} // namespace gui2

