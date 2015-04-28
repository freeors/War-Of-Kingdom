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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/move_hero.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

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

REGISTER_DIALOG(move_hero)

tmove_hero::tmove_hero(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical& city)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams_[city.side() - 1])
	, city_(city)
	, fresh_heros_()
	, candidate_cities_()
	, type_index_(0)
	, checked_heros_()
	, current_page_(twidget::npos)
	, recruits_()
{
}

void tmove_hero::type_selected(twindow& window, tlistbox& list, const int type)
{
	int cost_exponent = current_team_.cost_exponent();
	int gold = current_team_.gold();

	type_index_ = list.get_selected_row();
	
	refresh_tooltip(window);
}

void tmove_hero::hero_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();
	std::set<size_t>::iterator result = checked_heros_.find(toggled_index);

	if (result == checked_heros_.end()) {
		// toggled button isn't in checked_heros_
		if (toggle->get_value()) {
			checked_heros_.insert(toggled_index);
		} else {
			VALIDATE(false, "hero_toggled program error #1");
		}
	} else if (toggle->get_value()) { 
		VALIDATE(false, "hero_toggled program error #2");
	} else {
		checked_heros_.erase(result);
	}

	twindow* window = toggle->get_window();
	tbutton* ok = find_widget<tbutton>(window, "ok", false, true);
	ok->set_active(checked_heros_.empty()? false: true);

	refresh_tooltip(*window);
}

void tmove_hero::refresh_tooltip(twindow& window)
{
}

void tmove_hero::pre_show(CVideo& /*video*/, twindow& window)
{
	int side_num = city_.side();
	std::stringstream str;

	tlistbox* list = find_widget<tlistbox>(&window, "city_list", false, true);

	for (unit_map::iterator it2 = units_.begin(); it2 != units_.end(); ++ it2) {
		unit* it = dynamic_cast<unit*>(&*it2);
		if (!it->is_city() || it->side() != side_num) {
			continue;
		}
		if (it->cityno() == city_.cityno()) {
			continue;
		}
		artifical& city = *unit_2_artifical(&*it);
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = city.absolute_image();
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = city.name();
		list_item_item.insert(std::make_pair("name", list_item));

		str.str("");
		str << city.finish_heros().size() + city.fresh_heros().size();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("reside_hero", list_item));

		list->add_row(list_item_item);

		candidate_cities_.push_back(&city);
	}

	list->set_callback_value_change(dialog_callback3<tmove_hero, tlistbox, &tmove_hero::type_selected>);

	page_panel_ = find_widget<tstacked_widget>(&window, "page", false, true);
	lists_.push_back(find_widget<tlistbox>(&window, "ability_list", false, true));
	lists_.push_back(find_widget<tlistbox>(&window, "adaptability_list", false, true));
	lists_.push_back(find_widget<tlistbox>(&window, "personal_list", false, true));
	lists_.push_back(find_widget<tlistbox>(&window, "relation_list", false, true));

	fresh_heros_ = city_.fresh_heros();
	// std::sort(fresh_heros_.begin(), fresh_heros_.end(), compare_leadership);

	// fill data to hero_table
	catalog_page(window, ABILITY_PAGE, false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "ability", false)
		, boost::bind(
			&tmove_hero::catalog_page
			, this
			, boost::ref(window)
			, (int)ABILITY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "adaptability", false)
		, boost::bind(
			&tmove_hero::catalog_page
			, this
			, boost::ref(window)
			, (int)ADAPTABILITY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "personal", false)
		, boost::bind(
			&tmove_hero::catalog_page
			, this
			, boost::ref(window)
			, (int)PERSONAL_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "relation", false)
		, boost::bind(
			&tmove_hero::catalog_page
			, this
			, boost::ref(window)
			, (int)RELATION_PAGE
			, true));

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(false);
}

void tmove_hero::post_show(twindow& window)
{
}

void tmove_hero::catalog_page(twindow& window, int page, bool swap)
{
	if (!page_panel_) {
		return;
	}
	if (current_page_ == page) {
		return;
	}

	int selected_row = 0;
	if (swap) {
		// because sort, order is changed.
		tlistbox& list = *lists_[current_page_];
		selected_row = list.get_selected_row();
		list.clear();
	}

	page_panel_->set_radio_layer(page);
	tlistbox& list = *lists_[page];

	std::stringstream str;
	int hero_index = 0;
	for (std::vector<hero*>::iterator itor = fresh_heros_.begin(); itor != fresh_heros_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero* h = *itor;

		if (page == ABILITY_PAGE) {
			table_item["label"] = h->name();
			table_item_item.insert(std::make_pair("name", table_item));
			
			table_item["label"] = lexical_cast<std::string>(h->loyalty(*teams_[h->side_].leader()));;
			table_item_item.insert(std::make_pair("loyalty", table_item));

			table_item["label"] = hero::feature_str(h->feature_);
			table_item_item.insert(std::make_pair("feature", table_item));

			table_item["label"] = lexical_cast<std::string>(fxptoi9(h->leadership_));
			table_item_item.insert(std::make_pair("leadership", table_item));

			table_item["label"] = lexical_cast<std::string>(fxptoi9(h->force_));
			table_item_item.insert(std::make_pair("force", table_item));

			table_item["label"] = lexical_cast<std::string>(fxptoi9(h->intellect_));
			table_item_item.insert(std::make_pair("intellect", table_item));

			table_item["label"] = lexical_cast<std::string>(fxptoi9(h->spirit_));
			table_item_item.insert(std::make_pair("spirit", table_item));

			table_item["label"] = lexical_cast<std::string>(fxptoi9(h->charm_));
			table_item_item.insert(std::make_pair("charm", table_item));

		} else if (page == ADAPTABILITY_PAGE) {
			table_item["label"] = h->name();
			table_item_item.insert(std::make_pair("name", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[0]);
			table_item_item.insert(std::make_pair("arm0", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[1]);
			table_item_item.insert(std::make_pair("arm1", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[2]);
			table_item_item.insert(std::make_pair("arm2", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[3]);
			table_item_item.insert(std::make_pair("arm3", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[4]);
			table_item_item.insert(std::make_pair("arm4", table_item));

		} else if (page == PERSONAL_PAGE) {
			table_item["label"] = h->name();
			table_item_item.insert(std::make_pair("name", table_item));

			table_item["label"] = hero::gender_str(h->gender_);
			table_item_item.insert(std::make_pair("gender", table_item));

		} else if (page == RELATION_PAGE) {
			table_item["label"] = h->name();
			table_item_item.insert(std::make_pair("name", table_item));

			if (h->parent_[0].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[0].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("father", table_item));

			if (h->parent_[1].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[1].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("mother", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_OATH; i ++) {
				if (h->oath_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->oath_[i].hero_].name();
					} else {
						str << " " << heros_[h->oath_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("oath", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_CONSORT; i ++) {
				if (h->consort_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->consort_[i].hero_].name();
					} else {
						str << " " << heros_[h->consort_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("consort", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_INTIMATE; i ++) {
				if (h->intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->intimate_[i].hero_].name();
					} else {
						str << " " << heros_[h->intimate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intimate", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_HATE; i ++) {
				if (h->hate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->hate_[i].hero_].name();
					} else {
						str << " " << heros_[h->hate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hate", table_item));

		}
		list.add_row(table_item_item);

		twidget* grid_ptr = list.get_row_panel(hero_index);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&tmove_hero::hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (checked_heros_.find(hero_index) != checked_heros_.end()) {
			toggle->set_value(true);
		}
		if (rpg::stratum == hero_stratum_citizen && rpg::h != h) {
			toggle->set_active(false);
		}
	}
	list.invalidate_layout(true);

	current_page_ = page;
}

artifical* tmove_hero::dst_city() const
{ 
	return candidate_cities_[type_index_]; 
}

} // namespace gui2
