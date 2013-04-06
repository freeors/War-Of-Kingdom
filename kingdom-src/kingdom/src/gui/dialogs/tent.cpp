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

#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "display.hpp"
#include "hero.hpp"
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

	ttent::ttent(display& gui, hero_map& heros, card_map& cards, const config& campaign_config, hero& player_hero, const std::string& mode_id)
	: chk_shroud_(register_bool("shroud", false))
	, chk_fog_(register_bool("fog", false))
	, gui_(gui)
	, heros_(heros)
	, cards_(cards)
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

void ttent::pre_show(CVideo& /*video*/, twindow& window)
{
	tlistbox* list = find_widget<tlistbox>(&window, "player_list", false, true);
	std::string text;
	config cfg_from_file;
	rpg_mode_ = campaign_config_["mode"].str() == "rpg";

	wml_config_from_file(game_config::path + "/xwml/campaigns/" + campaign_config_["id"].str() + ".bin", cfg_from_file);
	const config& scenario = cfg_from_file.child("scenario");

	if (player_hero_->valid() && rpg_mode_) {
		rows_mem_ = (hero_row*)malloc(sizeof(hero_row) * (heros_.size() + 1));
		add_row_to_heros(list, player_hero_->number_, -1, -1, hero_stratum_citizen);
	} else {
		rows_mem_ = (hero_row*)malloc(sizeof(hero_row) * heros_.size());
	}

	std::map<int, int> mayor_map;
	std::vector<std::string> v;
	int leader, city, stratum;
	foreach (const config& side, scenario.child_range("side")) {
		// city_map_.clear();
		leader = side["leader"].to_int();
		if (side.has_attribute("controller")) {
			text = side["controller"].str();
			if (text != "human") {
				continue;
			}
		}
		foreach (const config& c, side.child_range("artifical")) {
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
		foreach (const config& u, side.child_range("unit")) {
			v = utils::split(u["heros_army"].str());
			for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
				int cityno = u["cityno"].to_int();
				if (cityno == HEROS_DEFAULT_CITY) {
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

	list->set_callback_value_change(dialog_callback<ttent, &ttent::player_selected>);

	// card table
	int player_size = campaign_config_.child_count("player");
	size_t card_size = cards_.size();
	if (!card_size) {
		return;
	}

	std::stringstream strstr;
	card_table_ = find_widget<tlistbox>(&window, "card_table", false, true);

	int card_index = 0;
	for (card_map::iterator c = cards_.begin(); c != cards_.end(); ++ c, card_index ++) {
		if (c->decree() || !card_valid(c->mode())) {
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

	cfg["leader"] = rows_mem_[player_].leader_;
	cfg["hero"] = rows_mem_[player_].hero_;
	if (rpg_mode_) {
		cfg["city"] = rows_mem_[player_].city_;
		cfg["stratum"] = rows_mem_[player_].stratum_;
	} else {
		cfg["stratum"] = hero_stratum_leader;
	}
	return cfg;
}

std::vector<bool> ttent::checked_card() const
{
	std::vector<bool> ret;
	for (std::map<int, bool>::const_iterator it = checked_card_.begin(); it != checked_card_.end(); ++ it) {
		ret.push_back(it->second);
	}
	return ret;
}

void ttent::add_row_to_heros(tlistbox* list, int h, int leader, int city, int stratum)
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
	
	list->add_row(list_item_item);

	if (rpg_mode_) {
		tgrid* grid_ptr = list->get_row_grid(mem_vsize_);
		tcontrol* control = dynamic_cast<tcontrol*>(grid_ptr->find("icon", true));
		control->set_visible(twidget::INVISIBLE);
	}

	rows_mem_[mem_vsize_].hero_ = h;
	rows_mem_[mem_vsize_].leader_ = leader;
	rows_mem_[mem_vsize_].city_ = city;
	rows_mem_[mem_vsize_ ++].stratum_ = stratum;
}

} // namespace gui2
