/* $Id: player_selection.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/tent.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "card.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/widgets/toggle_button.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include <preferences.hpp>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_load
 *
 * == Load a game ==
 *
 * This shows the dialog to select and load a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * txtFilter & & text & m &
 *         The filter for the listbox items. $
 *
 * savegame_list & & listbox & m &
 *         List of savegames. $
 *
 * -filename & & control & m &
 *         Name of the savegame. $
 *
 * -date & & control & o &
 *         Date the savegame was created. $
 *
 * preview_pane & & widget & m &
 *         Container widget or grid that contains the items for a preview. The
 *         visible status of this container depends on whether or not something
 *         is selected. $
 *
 * -minimap & & minimap & m &
 *         Minimap of the selected savegame. $
 *
 * -imgLeader & & image & m &
 *         The image of the leader in the selected savegame. $
 *
 * -lblScenario & & label & m &
 *         The name of the scenario of the selected savegame. $
 *
 * -lblSummary & & label & m &
 *         Summary of the selected savegame. $
 *
 * @end{table}
 */

ttent::ttent(hero_map& heros, card_map& cards, const config& cfg, const config& campaign_config, hero& player_hero, const std::string& mode_id)
	: chk_shroud_(register_bool("shroud", false))
	, chk_fog_(register_bool("fog", false))
	, heros_(heros)
	, cards_(cards)
	, cfg_(cfg)
	, mode_id_(mode_id)
	, campaign_config_(campaign_config)
	, card_table_(NULL)
	, checked_card_()
	, player_(0)
	, shroud_(false)
	, fog_(false)
	, rpg_mode_(false)
	, rows_mem_(NULL)
	, mem_vsize_(0)
	, player_hero_(&player_hero)
	, city_map_()
	, city_leader_map_()
	, faction_list_()
	, player_faction_(NULL)
{
}

ttent::~ttent()
{
	if (rows_mem_) {
		free(rows_mem_);
		rows_mem_ = NULL;
	}
}

void ttent::player_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "player_list", false);

	player_ = list.get_selected_row();

	tbutton* button = find_widget<tbutton>(&window, "ok", false, true);
	button->set_active(rows_mem_[player_].city_ != -1);
}

void ttent::card_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();

	std::map<int, bool>::iterator find = checked_card_.find(toggled_index);
	if (toggle->get_value()) {
		find->second = true;
	} else {
		find->second = false;
	}
}

void ttent::init_player_list(tlistbox& list, twindow& window)
{
	config cfg_from_file;
	wml_config_from_file(game_config::path + "/xwml/campaigns/" + campaign_config_["id"].str() + ".bin", cfg_from_file);
	const config& scenario = cfg_from_file.find_child("scenario", "id", campaign_config_["first_scenario"]);

	// decide NONE/RPG from NONE.
	BOOST_FOREACH (const config& side, scenario.child_range("side")) {
		if (side.has_attribute("controller")) {
			continue;
		}
		if (const config& c = side.child("if")) {
			if (const config& c1 = c.child("then")) {
				if (c1["controller"].str() == "human") {
					rpg_mode_ = true;
					break;
				}
			}
		}
	}

	if (player_hero_->valid() && rpg_mode_) {
		rows_mem_ = (hero_row*)malloc(sizeof(hero_row) * (heros_.size() + 1));
		add_row_to_heros(list, player_hero_->number_, -1, -1, hero_stratum_citizen);
	} else {
		rows_mem_ = (hero_row*)malloc(sizeof(hero_row) * heros_.size());
	}

	std::string text;

	std::map<int, int> mayor_map;
	std::vector<std::string> v;
	int leader, city, stratum;
	BOOST_FOREACH (const config& side, scenario.child_range("side")) {
		// city_map_.clear();
		leader = side["leader"].to_int();
		if (side.has_attribute("controller")) {
			text = side["controller"].str();
			if (text != "human") {
				continue;
			}
		}
		BOOST_FOREACH (const config& c, side.child_range("artifical")) {
			int mayor = -1;
			if (c.has_attribute("mayor")) {
				mayor = c["mayor"].to_int();
			}
			city = c["heros_army"].to_int();
			int cityno = c["cityno"].to_int();
			city_map_[cityno] = city;
			city_leader_map_[cityno] = leader;
			mayor_map[cityno] = mayor;

			// service_heros
			v = utils::split(c["service_heros"].str());
			for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
				int h = lexical_cast_default<int>(*it);
				if (h == leader) {
					stratum = hero_stratum_leader;
				} else if (h == mayor) {
					stratum = hero_stratum_mayor;
				} else {
					stratum = hero_stratum_citizen;
				}
				if (rpg_mode_ || stratum == hero_stratum_leader) {
					add_row_to_heros(list, h, leader, city, stratum);
				}
			}
			if (rpg_mode_) {
				// wander_heros
				v = utils::split(c["wander_heros"].str());
				for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
					int h = lexical_cast_default<int>(*it);
					stratum = hero_stratum_wander;
					// add_row_to_heros(list, h, leader, city, stratum);
				}
			}
		}
		// unit. they maybe leader
		BOOST_FOREACH (const config& u, side.child_range("unit")) {
			v = utils::split(u["heros_army"].str());
			for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
				int cityno = u["cityno"].to_int();
				if (cityno == HEROS_ROAM_CITY) {
					continue;
				}
				int h = lexical_cast_default<int>(*it);

				std::map<int, int>::const_iterator it2 = mayor_map.find(cityno);
				VALIDATE(it2 != mayor_map.end(), "Cannot find cityno = " + u["cityno"].str() + " duration parseing " + heros_[h].name());
				int mayor = it2->second;
				if (h == leader) {
					stratum = hero_stratum_leader;
				} else if (h == mayor) {
					stratum = hero_stratum_mayor;
				} else {
					stratum = hero_stratum_citizen;
				}
				if (rpg_mode_ || stratum == hero_stratum_leader) {
					std::map<int, int>::const_iterator it2 = city_map_.find(cityno);
					VALIDATE(it2 != city_map_.end(), "Cannot find cityno = " + u["cityno"].str() + " duration parseing " + heros_[h].name());
					add_row_to_heros(list, h, leader, it2->second, stratum);
				}
			}
		}
	}

	list.set_callback_value_change(dialog_callback<ttent, &ttent::player_selected>);

	// disable all sort button
	tbutton* button = find_widget<tbutton>(&window, "hero_name", false, true);
	button->set_active(false);
	button = find_widget<tbutton>(&window, "hero_stratum", false, true);
	button->set_active(false);
	button = find_widget<tbutton>(&window, "hero_side", false, true);
	button->set_active(false);
	button = find_widget<tbutton>(&window, "hero_city", false, true);
	button->set_active(false);
}

void ttent::init_faction_list(tlistbox& list, twindow& window)
{
	faction_list_ = &list;

	player_faction_ = find_widget<tbutton>(&window, "player_faction", false, true);
	connect_signal_mouse_left_click(
		*player_faction_
		, boost::bind(
			&ttent::player_faction
			, this
			, boost::ref(window)));


	const config& cfg = cfg_.child("faction");
	if (cfg) {
		add_row_to_factions(*faction_list_, cfg);
		tent::human_leader_number = cfg["leader"].to_int();
		player_faction_->set_label(heros_[tent::human_leader_number].name());
	}

	// disable all sort button
	tbutton* button = find_widget<tbutton>(&window, "hero_fresh", false, true);
	button->set_active(false);
	button = find_widget<tbutton>(&window, "hero_wander", false, true);
	button->set_active(false);
	button = find_widget<tbutton>(&window, "hero_city", false, true);
	button->set_active(false);
}

void ttent::pre_show(CVideo& /*video*/, twindow& window)
{
	tlistbox* list = find_widget<tlistbox>(&window, "player_list", false, false);
	if (list) {
		init_player_list(*list, window);
		VALIDATE(list->get_item_count(), _("ttent::pre_show, there is no item in player_list!"));
	}

	list = find_widget<tlistbox>(&window, "faction_list", false, false);
	if (list) {
		init_faction_list(*list, window);
	}

	// card table
	size_t card_size = cards_.size();
	if (!card_size) {
		return;
	}

	std::stringstream strstr;
	card_table_ = find_widget<tlistbox>(&window, "card_table", false, true);

	int card_index = 0;
	for (card_map::iterator c = cards_.begin(); c != cards_.end(); ++ c, card_index ++) {
		if (!card_valid(c->mode()) || c->bomb()) {
			checked_card_.insert(std::make_pair(card_index, false));
			continue;
		}

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = c->name();
		list_item_item.insert(std::make_pair("name", list_item));

		strstr.str("");
		strstr << c->points();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("points", list_item));

		list_item["label"] = c->desc();
		list_item_item.insert(std::make_pair("desc", list_item));

		card_table_->add_row(list_item_item);

		tgrid* grid_ptr = card_table_->get_row_grid(card_table_->get_item_count() - 1);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&ttent::card_toggled, this, _1));
		toggle->set_data(card_index);
		toggle->set_value(true);
		checked_card_.insert(std::make_pair(card_index, true));
	}
}

void ttent::post_show(twindow& window)
{
	shroud_ = chk_shroud_->get_widget_value(window);
	fog_ = chk_fog_->get_widget_value(window);
}

bool ttent::card_valid(const std::string& mode) const
{
	if (mode.empty()) {
		return true;
	}

	const std::vector<std::string> vstr = utils::split(mode);
	return std::find(vstr.begin(), vstr.end(), mode_id_) != vstr.end();
}

config ttent::player() const
{
	config cfg;

	if (tent::human_leader_number == HEROS_INVALID_NUMBER) {
		cfg["leader"] = rows_mem_[player_].leader_;
		cfg["hero"] = rows_mem_[player_].hero_;
	} else {
		cfg["leader"] = tent::human_leader_number;
		cfg["hero"] = tent::human_leader_number;
	}

	if (rpg_mode_) {
		cfg["city"] = rows_mem_[player_].city_;
		cfg["stratum"] = rows_mem_[player_].stratum_;
	} else {
		cfg["stratum"] = hero_stratum_leader;
	}
	return cfg;
}

std::string ttent::mode_str() const
{
	if (campaign_config_["mode"].str() == "tower") {
		return "tower";
	} else if (rpg_mode_) {
		return "rpg";
	} else {
		return null_str;
	}
}

std::vector<bool> ttent::checked_card() const
{
	std::vector<bool> ret;
	for (std::map<int, bool>::const_iterator it = checked_card_.begin(); it != checked_card_.end(); ++ it) {
		ret.push_back(it->second);
	}
	return ret;
}

void ttent::add_row_to_heros(tlistbox& list, int h, int leader, int city, int stratum)
{
	// Add list item
	string_map list_item;
	std::map<std::string, string_map> list_item_item;

	if (!rpg_mode_) {
		list_item["label"] = heros_[h].image();
		list_item_item.insert(std::make_pair("icon", list_item));
	}

	if (h != player_hero_->number_) {
		list_item["label"] = heros_[h].name();
	} else {
		list_item["label"] = player_hero_->name();
	}
	list_item_item.insert(std::make_pair("name", list_item));

	if (leader != -1) {
		list_item["label"] = heros_[leader].name();
	} else {
		list_item["label"] = "---";
	}
	list_item_item.insert(std::make_pair("side", list_item));

	if (city != -1) {
		list_item["label"] = heros_[city].name();
	} else {
		list_item["label"] = "---";
	}
	list_item_item.insert(std::make_pair("city", list_item));

	list_item["label"] = hero::stratum_str(stratum);
	list_item_item.insert(std::make_pair("stratum", list_item));
	
	list.add_row(list_item_item);

	if (rpg_mode_) {
		tgrid* grid_ptr = list.get_row_grid(mem_vsize_);
		tcontrol* control = dynamic_cast<tcontrol*>(grid_ptr->find("icon", true));
		control->set_visible(twidget::INVISIBLE);
	}

	rows_mem_[mem_vsize_].hero_ = h;
	rows_mem_[mem_vsize_].leader_ = leader;
	rows_mem_[mem_vsize_].city_ = city;
	rows_mem_[mem_vsize_ ++].stratum_ = stratum;
}

void ttent::add_row_to_factions(tlistbox& list, const config& cfg)
{
	std::stringstream strstr;
	// Add list item
	string_map list_item;
	std::map<std::string, string_map> list_item_item;

	int number = cfg["leader"].to_int();
	list_item["label"] = heros_[number].image();
	list_item_item.insert(std::make_pair("icon", list_item));
	
	std::vector<std::string> service_vstr = utils::split(cfg["service_heros"].str());
	std::vector<std::string> wander_vstr = utils::split(cfg["wander_heros"].str());
	int total = service_vstr.size() + wander_vstr.size();

	strstr.str("");
	if (mode_id_ != "tower") {
		strstr << service_vstr.size();
	} else if (total >= game_config::tower_fix_heros) {
		strstr << game_config::tower_fix_heros;
	} else {
		strstr << total;
	}
	list_item["label"] = strstr.str();
	list_item_item.insert(std::make_pair("fresh", list_item));

	strstr.str("");
	if (mode_id_ != "tower") {
		strstr << service_vstr.size();
	} else if (total >= game_config::tower_fix_heros) {
		strstr << total - game_config::tower_fix_heros;
	} else {
		strstr << 0;
	}
	list_item["label"] = strstr.str();
	list_item_item.insert(std::make_pair("wander", list_item));

	const config& art_cfg = cfg.child("artifical");
	number = art_cfg["heros_army"].to_int();
	list_item["label"] = heros_[number].name();
	list_item_item.insert(std::make_pair("city", list_item));

	list.add_row(list_item_item);
}

void ttent::player_faction(twindow& window)
{
	// The possible eras to play
	std::vector<std::string> items;
	std::map<int, tval_str> factions_map;
	int actived_index = 0;

	const config::const_child_itors& factions = cfg_.child_range("faction");
	int index = 0;
	BOOST_FOREACH (const config &cfg, factions) {
		int number = cfg["leader"].to_int();
		factions_map.insert(std::make_pair(index ++, tval_str(number, heros_[number].name())));
	}

	for (std::map<int, tval_str>::iterator it = factions_map.begin(); it != factions_map.end(); ++ it) {
		items.push_back(it->second.str);
		if (tent::human_leader_number == it->second.val) {
			actived_index = std::distance(factions_map.begin(), it);
		}
	}
	
	gui2::tcombo_box dlg(items, actived_index);
	dlg.show(gui().video());

	int selected = dlg.selected_index();
	if (selected == actived_index) {
		return;
	}
	tent::human_leader_number = factions_map.find(selected)->second.val;

	player_faction_->set_label(heros_[tent::human_leader_number].name());
	faction_list_->remove_row(0);
	add_row_to_factions(*faction_list_, cfg_.child("faction", selected));
}

} // namespace gui2
